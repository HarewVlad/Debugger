static Debugger *CreateDebugger(const std::string &process_name) {
  Debugger *result = new Debugger;

  STARTUPINFO si = {};
  PROCESS_INFORMATION pi = {};
  if (!CreateProcessA(process_name.c_str(), NULL, NULL, NULL, FALSE,
                      DEBUG_ONLY_THIS_PROCESS, NULL, NULL, &si, &pi)) {
    LOG(CreateDebugger) << "Unable to create debugge process\n";
    delete result;
    return nullptr;
  }

  if (!SymInitialize(pi.hProcess, NULL, false)) {
    LOG(CreateDebugger) << "Unable to initialize debug symbols!\n";
    delete result;
    return nullptr;
  }

  result->si = si;
  result->pi = pi;

  return result;
}

inline DWORD DebuggerGetTargetStartAddress(HANDLE process, HANDLE thread) {
  DWORD result = 0;

  SYMBOL_INFO *symbol;
  symbol = (SYMBOL_INFO *)new BYTE[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  symbol->MaxNameLen = MAX_SYM_NAME;
  SymFromName(process, "main", symbol);

  result = symbol->Address;

  delete[](BYTE *) symbol;

  return result;
}

inline bool DebuggerLoadTargetModules(Debugger *debugger, HANDLE file,
                                      HANDLE process, DWORD64 base_address) {
  TCHAR filename[MAX_PATH + 1];
  if (!GetFileNameFromHandle(file, filename)) {
    LOG(DebuggerProcessEvent) << "Unable to get file name from handle!\n";
    return false;
  }

  DWORD64 base = SymLoadModuleEx(process, NULL, filename, NULL, base_address, 0,
                                 NULL, NULL);
  if (!base) {
    LOG(DebuggerProcessEvent) << "Unable to get module base address!\n";
    return false;
  }

  IMAGEHLP_MODULE64 module_info;
  module_info.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
  if (SymGetModuleInfo64(process, base, &module_info)) {
    LOG(INFO) << "Loaded DLL " << filename << ", at address " << std::hex
              << base_address
              << (module_info.SymType == SymPdb ? ", symbols loaded. \n"
                                                : "\n");
  } else {
    LOG(ERROR) << "Unable to load " << filename << '\n';
  }
}

static Breakpoint *CreateBreakpoint(HANDLE process, DWORD address) {
  Breakpoint *result = new Breakpoint;

  BYTE instruction;
  DWORD read_bytes;
  if (!ReadProcessMemory(process, (void *)address, &instruction, 1,
                         &read_bytes)) {
    DWORD error = GetLastError();
    LOG(DebuggerProcessEvent)
        << "Unable to read from process memory, error = " << GetLastError()
        << '\n';
    delete result;
    return nullptr;
  }

  BYTE original_instruction = instruction;

  instruction = 0xcc;
  WriteProcessMemory(process, (void *)address, &instruction, 1, &read_bytes);
  FlushInstructionCache(process, (void *)address, 1);

  result->original_instruction = original_instruction;
  result->address = address;

  return result;
}

static bool DebuggerPrintRegisters(Debugger *debugger) {
  auto pi = debugger->pi;

  CONTEXT context;
  context.ContextFlags = CONTEXT_ALL;
  GetThreadContext(pi.hThread, &context);

  LOG(INFO) << "EAX = " << context.Eax << "\nEBX = " << context.Ebx
            << "\nECX = " << context.Ecx << '\n';

  return true;
}

static bool DebuggerRemoveBreakpoint(Debugger *debugger, int line) {
  auto &breakpoints = debugger->breakpoints;
  auto breakpoint = breakpoints[line];
  auto pi = debugger->pi;

  // Restore EIP
  CONTEXT context;
  context.ContextFlags = CONTEXT_ALL;
  GetThreadContext(pi.hThread, &context);

  --context.Eip;
  SetThreadContext(pi.hThread, &context);

  // Restore original instruction
  DWORD read_bytes;
  if (!WriteProcessMemory(pi.hProcess, (void *)breakpoint->address,
                          &breakpoint->original_instruction, 1, &read_bytes)) {
    LOG(DebuggerRemoveBreakpoint)
        << "Unable to write process memory, error = " << GetLastError() << '\n';
    return false;
  }

  // Delete breakpoint
  breakpoints.erase(line);
  delete breakpoint;

  return true;
}

static bool DebuggerProcessEvent(Debugger *debugger, DEBUG_EVENT debug_event,
                                 DWORD &continue_status) {
  auto pi = debugger->pi;
  auto &breakpoints = debugger->breakpoints;

  switch (debug_event.dwDebugEventCode) {
    case LOAD_DLL_DEBUG_EVENT: {
      const LOAD_DLL_DEBUG_INFO load_dll_debug_info = debug_event.u.LoadDll;

      DebuggerLoadTargetModules(debugger, load_dll_debug_info.hFile,
                                pi.hProcess,
                                (DWORD64)load_dll_debug_info.lpBaseOfDll);
    } break;
    case CREATE_PROCESS_DEBUG_EVENT: {
      const CREATE_PROCESS_DEBUG_INFO create_process_debug_info =
          debug_event.u.CreateProcessInfo;

      DebuggerLoadTargetModules(
          debugger, create_process_debug_info.hFile, pi.hProcess,
          (DWORD64)create_process_debug_info.lpBaseOfImage);

      // Replace first instruction with int3

      DWORD start_address =
          DebuggerGetTargetStartAddress(pi.hProcess, pi.hThread);

      Breakpoint *breakpoint = CreateBreakpoint(pi.hProcess, start_address);

      breakpoints[0] = breakpoint;  // Line 0

      continue_status = DBG_CONTINUE;
    } break;
    case OUTPUT_DEBUG_STRING_EVENT: {
      const OUTPUT_DEBUG_STRING_INFO output_debug_string_info =
          debug_event.u.DebugString;
      WORD size = output_debug_string_info.nDebugStringLength;
      LPSTR data = output_debug_string_info.lpDebugStringData;
      WCHAR *message = new WCHAR[size];

      if (!ReadProcessMemory(pi.hProcess, data, message, size, NULL)) {
        LOG(DebuggerProcessEvent) << "Unable to read from process memory!\n";
        return false;
      }

      if (!output_debug_string_info.fUnicode) {
        LOG(OUTPUT_DEBUG_STRING_EVENT) << (char *)message << '\n';
      } else {
        // LOG(OUTPUT_DEBUG_STRING_EVENT, message); // Output to console is not
        // supported for now (cause i'm dumb) :(
      }

      continue_status = DBG_CONTINUE;
    } break;
    case EXCEPTION_DEBUG_EVENT: {
      const EXCEPTION_DEBUG_INFO &exception_debug_info =
          debug_event.u.Exception;
      DWORD exception_code = exception_debug_info.ExceptionRecord.ExceptionCode;
      switch (exception_code) {
        case EXCEPTION_BREAKPOINT: {
          static bool ignore_breakpoint = true;
          if (ignore_breakpoint) {
            ignore_breakpoint = false;
          } else {
            LOG(EXCEPTION_BREAKPOINT)
                << "Breakpoint at address "
                << exception_debug_info.ExceptionRecord.ExceptionAddress
                << '\n';
          }

          continue_status = DBG_CONTINUE;
        } break;
        default:
          if (exception_debug_info.dwFirstChance == 1) {
          }

          continue_status = DBG_EXCEPTION_NOT_HANDLED;
          break;
      }
    } break;
    default:
      continue_status = DBG_EXCEPTION_NOT_HANDLED;
      return true;
  }

  return true;
}

static bool DebuggerRun(Debugger *debugger) {
  DEBUG_EVENT debug_event = {};
  while (true) {
    if (!WaitForDebugEvent(&debug_event, INFINITE)) return 1;

    DWORD continue_status;
    DebuggerProcessEvent(debugger, debug_event, continue_status);

    ContinueDebugEvent(debug_event.dwProcessId, debug_event.dwThreadId,
                       continue_status);
  }
}