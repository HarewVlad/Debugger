static Debugger CreateDebugger(const std::string &process_name,
                               HANDLE continue_event) {
  Debugger result;

  STARTUPINFO si = {};
  PROCESS_INFORMATION pi = {};
  if (!CreateProcessA(process_name.c_str(), NULL, NULL, NULL, FALSE,
                      DEBUG_ONLY_THIS_PROCESS, NULL, NULL, &si, &pi)) {
    LOG(CreateDebugger) << "CreateProcessA failed, error = " << GetLastError()
                        << '\n';
    assert(false);
  }

  if (!SymInitialize(pi.hProcess, NULL, false)) {
    LOG(CreateDebugger) << "SymInitialize failed, error = " << GetLastError()
                        << '\n';
    assert(false);
  }

  result.si = si;
  result.pi = pi;
  result.continue_event = continue_event;

  return result;
}

inline DWORD64 DebuggerGetTargetEndAddress(CONTEXT &context,
                                           PROCESS_INFORMATION &pi) {
  STACKFRAME64 stack = {};
  stack.AddrPC.Offset = context.Eip;
  stack.AddrPC.Mode = AddrModeFlat;
  stack.AddrFrame.Offset = context.Ebp;
  stack.AddrFrame.Mode = AddrModeFlat;
  stack.AddrStack.Offset = context.Esp;
  stack.AddrStack.Mode = AddrModeFlat;
  if (!StackWalk64(IMAGE_FILE_MACHINE_I386, pi.hProcess, pi.hThread, &stack,
                   &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64,
                   0)) {
    return NULL;
  }

  return stack.AddrReturn.Offset + 0x5;  // ?
}

inline DWORD64 DebuggerGetTargetStartAddress(HANDLE process, HANDLE thread) {
  DWORD64 result = 0;

  SYMBOL_INFO *symbol;
  symbol = (SYMBOL_INFO *)new BYTE[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  symbol->MaxNameLen = MAX_SYM_NAME;
  SymFromName(process, "main", symbol);

  result = symbol->Address;

  delete[](BYTE *) symbol;

  return result;
}

inline BOOL WINAPI EnumSourceFilesCallback(PSOURCEFILE pSourceFile,
                                           PVOID UserContext) {
  Debugger *debugger = (Debugger *)UserContext;
  debugger->source_files.push_back(pSourceFile->FileName);

  return TRUE;
}

inline BOOL WINAPI EnumLinesCallback(PSRCCODEINFO LineInfo, PVOID UserContext) {
  Debugger *debugger = (Debugger *)UserContext;

  // LOG(INFO) << "Module: " << LineInfo->FileName << " - Line: " << std::dec <<
  // LineInfo->LineNumber
  //<< " - Address: " << std::hex << LineInfo->Address << '\n';
  debugger->lines[LineInfo->Address] = LineInfo->LineNumber;

  return TRUE;
}

inline bool DebuggerLoadTargetModules(Debugger *debugger, HANDLE file,
                                      HANDLE process, DWORD64 base_address) {
  auto pi = debugger->pi;

  TCHAR filename[MAX_PATH + 1];
  if (!GetFileNameFromHandle(file, filename)) {
    LOG(DebuggerProcessEvent)
        << "GetFileNameFromHandle failed, error = " << GetLastError() << '\n';
    return false;
  }

  DWORD64 base = SymLoadModuleEx(process, NULL, filename, NULL, base_address, 0,
                                 NULL, NULL);
  if (!base) {
    LOG(DebuggerProcessEvent)
        << "SymLoadModuleEx failed, error = " << GetLastError() << "!\n";
    return false;
  }

  IMAGEHLP_MODULE64 module_info;
  module_info.SizeOfStruct = sizeof(module_info);
  if (SymGetModuleInfo64(process, base, &module_info)) {
    LOG(INFO) << "Loaded DLL " << filename << ", at address " << std::hex
              << base_address
              << (module_info.SymType == SymPdb ? ", symbols loaded. \n"
                                                : "\n");

    if (module_info.SymType == SymPdb) {
      SymEnumSourceFiles(pi.hProcess, base, NULL, EnumSourceFilesCallback,
                         debugger);
      for (int i = 0; i < debugger->source_files.size(); ++i) {
        SymEnumLines(pi.hProcess, base, NULL, debugger->source_files[i].c_str(),
                     EnumLinesCallback, debugger);
      }
    }
  } else {
    LOG(ERROR) << "Unable to load " << filename << '\n';
    return false;
  }

  return true;
}

static Breakpoint *CreateBreakpoint(HANDLE process, DWORD64 address) {
  Breakpoint *result = new Breakpoint;

  BYTE instruction;
  DWORD read_bytes;
  if (!ReadProcessMemory(process, (void *)address, &instruction, 1,
                         &read_bytes)) {
    LOG(DebuggerProcessEvent)
        << "ReadProcessMemory failed, error = " << GetLastError() << '\n';
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
  auto context = debugger->original_context;

  LOG(INFO) << "EAX = " << context.Eax << "\nEBX = " << context.Ebx
            << "\nECX = " << context.Ecx << '\n';

  return true;
}

inline bool DebuggerRestoreInstruction(Debugger *debugger,
                                       Breakpoint *breakpoint,
                                       CONTEXT context) {
  auto pi = debugger->pi;

  // Restore EIP
  --context.Eip;
  context.EFlags |= 0x100;  // Trap flag
  SetThreadContext(pi.hThread, &context);

  // Restore original instruction
  DWORD read_bytes;
  if (!WriteProcessMemory(pi.hProcess, (void *)breakpoint->address,
                          &breakpoint->original_instruction, 1, &read_bytes)) {
    LOG(DebuggerRemoveBreakpoint)
        << "WriteProcessMemory failed, error = " << GetLastError() << '\n';
    return false;
  }
  FlushInstructionCache(pi.hProcess, (void *)breakpoint->address, 1);

  return true;
}

static bool DebuggerPrintCallstack(Debugger *debugger) {
  auto pi = debugger->pi;
  auto context = debugger->original_context;

  STACKFRAME64 stack = {};
  stack.AddrPC.Offset = context.Eip;
  stack.AddrPC.Mode = AddrModeFlat;
  stack.AddrFrame.Offset = context.Ebp;
  stack.AddrFrame.Mode = AddrModeFlat;
  stack.AddrStack.Offset = context.Esp;
  stack.AddrStack.Mode = AddrModeFlat;

  LOG(Callstack) << '\n';
  do {
    if (!StackWalk64(IMAGE_FILE_MACHINE_I386, pi.hProcess, pi.hThread, &stack,
                     &context, NULL, SymFunctionTableAccess64,
                     SymGetModuleBase64, 0)) {
      break;
    }

    std::stringstream ss;

    IMAGEHLP_MODULE64 module = {};
    module.SizeOfStruct = sizeof(module);
    if (SymGetModuleInfo64(pi.hProcess, (DWORD64)stack.AddrPC.Offset,
                           &module)) {
      ss << "      Module: " << module.ModuleName << '\n';
    }

    IMAGEHLP_SYMBOL64 *symbol;
    DWORD displacement;
    symbol =
        (IMAGEHLP_SYMBOL64 *)new BYTE[sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME];
    ZeroMemory(symbol, sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME);
    symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
    symbol->MaxNameLength = MAX_SYM_NAME;

    if (SymGetSymFromAddr64(pi.hProcess, (DWORD64)stack.AddrPC.Offset,
                            (DWORD64 *)&displacement, symbol)) {
      ss << "      Function: " << symbol->Name << '\n';
    }

    IMAGEHLP_LINE64 line = {};
    line.SizeOfStruct = sizeof(line);

    if (SymGetLineFromAddr64(pi.hProcess, (DWORD64)stack.AddrPC.Offset,
                             &displacement, &line)) {
      ss << "      Filename: " << line.FileName << '\n';
      ss << "      Line: " << line.LineNumber << '\n';
    }
    if (ss.rdbuf()->in_avail() != 0) {
      std::cout << std::setw(10) << std::hex << stack.AddrPC.Offset << ":\n";
      std::cout << ss.rdbuf();
    }
  } while (stack.AddrReturn.Offset != 0);

  return true;
}

static bool DebuggerStepOver(Debugger *debugger) {
  auto pi = debugger->pi;
  const auto &lines = debugger->lines;
  auto &breakpoints = debugger->breakpoints;
  const auto &current_line_address = debugger->current_line_address;

  auto current_line_it = lines.find(current_line_address);
  ++current_line_it;

  Breakpoint *breakpoint =
      CreateBreakpoint(pi.hProcess, current_line_it->first);
  breakpoints[current_line_it->first] = breakpoint;

  return true;
}

static bool DebuggerProcessEvent(Debugger *debugger, DEBUG_EVENT debug_event,
                                 DWORD &continue_status) {
  auto pi = debugger->pi;
  auto &breakpoints = debugger->breakpoints;
  auto &lines = debugger->lines;

  continue_status = DBG_CONTINUE;

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

      debugger->start_address =
          DebuggerGetTargetStartAddress(pi.hProcess, pi.hThread);
      debugger->current_line_address = debugger->start_address;

      Breakpoint *breakpoint =
          CreateBreakpoint(pi.hProcess, debugger->start_address);

      breakpoints[debugger->start_address] = breakpoint;
    } break;
    case OUTPUT_DEBUG_STRING_EVENT: {
      const OUTPUT_DEBUG_STRING_INFO output_debug_string_info =
          debug_event.u.DebugString;
      WCHAR *message = new WCHAR[output_debug_string_info.nDebugStringLength];

      if (!ReadProcessMemory(
              pi.hProcess, output_debug_string_info.lpDebugStringData, message,
              output_debug_string_info.nDebugStringLength, NULL)) {
        LOG(DebuggerProcessEvent)
            << "ReadProcessMemory failed, error = " << GetLastError() << '\n';
        return false;
      }

      if (!output_debug_string_info.fUnicode) {
        LOG(OUTPUT_DEBUG_STRING_EVENT) << (char *)message << '\n';
      } else {
        // LOG(OUTPUT_DEBUG_STRING_EVENT, message); // Output to console is not
        // supported for now (cause i'm dumb) :(
      }
    } break;
    case EXCEPTION_DEBUG_EVENT: {
      const EXCEPTION_DEBUG_INFO &exception_debug_info =
          debug_event.u.Exception;
      switch (exception_debug_info.ExceptionRecord.ExceptionCode) {
        case EXCEPTION_BREAKPOINT: {
          static bool ignore_breakpoint = true;
          if (ignore_breakpoint) {
            ignore_breakpoint = false;
          } else {
            DWORD64 exception_address =
                (DWORD64)exception_debug_info.ExceptionRecord.ExceptionAddress;

            LOG(DebuggerProcessEvent)
                << "Breakpoint at (" << std::dec << lines[exception_address]
                << ", " << std::hex << exception_address << ")\n";

            CONTEXT context;
            context.ContextFlags = CONTEXT_ALL;
            GetThreadContext(pi.hThread, &context);

            if (exception_address == debugger->start_address) {
              debugger->end_address = DebuggerGetTargetEndAddress(context, pi);

              Breakpoint *breakpoint =
                  CreateBreakpoint(pi.hProcess, debugger->end_address);
              breakpoints[debugger->end_address] = breakpoint;
            }

            Breakpoint *breakpoint = breakpoints[exception_address];

            debugger->original_context = context;

            DebuggerRestoreInstruction(debugger, breakpoint, context);
          }
        } break;
        case EXCEPTION_SINGLE_STEP: {
          // Reinstall exception
          BYTE instruction = 0xcc;
          DWORD read_bytes;
          if (!WriteProcessMemory(pi.hProcess,
                                  (void *)(debugger->original_context.Eip - 1),
                                  &instruction, 1, &read_bytes)) {
            LOG(DebuggerProcessEvent)
                << "WriteProcessMemory failed, error = " << GetLastError()
                << '\n';
            return false;
          }
          FlushInstructionCache(
              pi.hProcess, (void *)(debugger->original_context.Eip - 1), 1);

          debugger->current_line_address = debugger->original_context.Eip - 1;

          // Step over / print registers / print callstack
          WaitForSingleObject(debugger->continue_event, INFINITE);
        } break;
        default:
          if (exception_debug_info.dwFirstChance == 1) {
          }

          continue_status = DBG_EXCEPTION_NOT_HANDLED;
          break;
      }
    } break;
    case EXIT_PROCESS_DEBUG_EVENT: {
      LOG(DebuggerProcessEvent) << "Process is terminated, exiting ... \n";
      exit(0);
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
    if (!DebuggerProcessEvent(debugger, debug_event, continue_status)) {
      return false;
    }
    ContinueDebugEvent(debug_event.dwProcessId, debug_event.dwThreadId,
                       continue_status);
  }
}