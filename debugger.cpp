static Debugger CreateDebugger(const std::string &process_name,
                               HANDLE continue_event) {
  Debugger result;

  STARTUPINFO si = {};
  PROCESS_INFORMATION pi = {};
  if (!CreateProcessA(process_name.c_str(), NULL, NULL, NULL, FALSE,
                      DEBUG_ONLY_THIS_PROCESS, NULL, NULL, &si, &pi)) {
    LOG_IMGUI(CreateDebugger, "CreateProcesA failed, error = ", GetLastError())
    assert(false);
  }

  if (!SymInitialize(pi.hProcess, NULL, false)) {
    LOG_IMGUI(CreateDebugger, "SymInitialize failed, error = ", GetLastError())
    assert(false);
  }

  result.si = si;
  result.pi = pi;
  result.continue_event = continue_event;

  return result;
}

inline DWORD64 DebuggetGetFunctionReturnAddress(CONTEXT &context,
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

  return stack.AddrReturn.Offset; // + 0x5 ?
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

inline BOOL WINAPI EnumSourceFilesCallback(PSOURCEFILE SourceFile,
                                           PVOID UserContext) {
  Debugger *debugger = (Debugger *)UserContext;
  // LOG(INFO) << "ModBase: " << std::hex << SourceFile->ModBase << ", Filename:
  // " << SourceFile->FileName << '\n';
  debugger->source_files.push_back(SourceFile->FileName);

  return TRUE;
}

inline BOOL WINAPI EnumLinesCallback(PSRCCODEINFO LineInfo, PVOID UserContext) {
  Debugger *debugger = (Debugger *)UserContext;

  LOG_IMGUI_TO_FILE(INFO, "Module: ", LineInfo->FileName, " - Line: ", std::dec,
                    LineInfo->LineNumber, " - Address: ", std::hex,
                    LineInfo->Address)
  debugger->address_to_line[LineInfo->Address] =
      Line{LineInfo->LineNumber, LineInfo->FileName};

  DWORD64 filename_line_hash =
      GetStringDWORDHash(LineInfo->FileName, LineInfo->LineNumber);
  debugger->filename_and_line_to_address[filename_line_hash] =
      LineInfo->Address;

  return TRUE;
}

inline bool DebuggerLoadTargetModules(Debugger *debugger, HANDLE file,
                                      HANDLE process, DWORD64 base_address) {
  auto pi = debugger->pi;

  TCHAR filename[MAX_PATH + 1];
  if (!GetFileNameFromHandle(file, filename)) {
    LOG_IMGUI(DebuggerProcessEvent,
              "GetFileNameFromHandle failed, error = ", GetLastError())
    return false;
  }

  DWORD64 base = SymLoadModuleEx(process, NULL, filename, NULL, base_address, 0,
                                 NULL, NULL);
  if (!base) {
    LOG_IMGUI(DebuggerProcessEvent,
              "SymLoadModuleEx failed, error = ", GetLastError())
    return false;
  }

  IMAGEHLP_MODULE64 module_info;
  module_info.SizeOfStruct = sizeof(module_info);
  if (SymGetModuleInfo64(process, base, &module_info)) {
    LOG_IMGUI(INFO, "Loaded DLL", filename, ", at address", base_address,
              module_info.SymType == SymPdb ? ", symbols loaded."
                                            : "symbols not loaded")

    if (module_info.SymType == SymPdb) {
      SymEnumSourceFiles(pi.hProcess, base, "*.[ic][np][lp?]",
                         EnumSourceFilesCallback, debugger);

      // Send source files with debug symbols
      if (debugger->OnLoadSourceFiles)
        debugger->OnLoadSourceFiles(debugger->source_files);

      for (int i = 0; i < debugger->source_files.size(); ++i) {
        SymEnumLines(pi.hProcess, base, NULL, debugger->source_files[i].c_str(),
                     EnumLinesCallback, debugger);
      }
    }
  } else {
    LOG_IMGUI(DebuggerLoadTargetModules, "Unable to load ", filename)
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
    LOG_IMGUI(DebuggerProcessEvent,
              "ReadProcessMemory failed, error = ", GetLastError())
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

// TODO: Rethink design like create struct or something
static void DebuggerPrintRegisters(Debugger *debugger) {
  auto pi = debugger->pi;
  auto context = debugger->original_context;

  // LOG(INFO) << "EAX = " << context.Eax << "\nEBX = " << context.Ebx
  //           << "\nECX = " << context.Ecx << '\n';
}

inline bool DebuggerRestoreInstruction(Debugger *debugger,
                                       Breakpoint *breakpoint,
                                       CONTEXT context) {
  auto pi = debugger->pi;

  // Restore EIP
  --context.Eip;
  context.EFlags |= 0x100; // Trap flag
  SetThreadContext(pi.hThread, &context);

  // Restore original instruction
  DWORD read_bytes;
  if (!WriteProcessMemory(pi.hProcess, (void *)breakpoint->address,
                          &breakpoint->original_instruction, 1, &read_bytes)) {
    LOG_IMGUI(DebuggerRestoreInstruction,
              "WriteProcessMemory failed, error = ", GetLastError())
    return false;
  }
  FlushInstructionCache(pi.hProcess, (void *)breakpoint->address, 1);

  return true;
}

static bool DebuggerRemoveBreakpoint(Debugger *debugger,
                                     const std::string &filename, DWORD line) {
  const auto &filename_and_line_to_address =
      debugger->filename_and_line_to_address;
  auto &breakpoints = debugger->breakpoints;
  const auto pi = debugger->pi;

  DWORD64 filename_line_hash = GetStringDWORDHash(filename, line);
  DWORD64 filename_line_hash_temp = GetStringDWORDHash(filename, line);
  auto filename_and_line_to_address_it =
      filename_and_line_to_address.find(filename_line_hash);
  if (filename_and_line_to_address_it != filename_and_line_to_address.end()) {
    if (debugger->breakpoints.find(filename_and_line_to_address_it->second) !=
        debugger->breakpoints.end()) {

      Breakpoint *breakpoint =
          breakpoints[filename_and_line_to_address_it->second];

      // Restore original instruction
      DWORD read_bytes;
      if (!WriteProcessMemory(pi.hProcess, (void *)breakpoint->address,
                              &breakpoint->original_instruction, 1,
                              &read_bytes)) {
        LOG_IMGUI(DebuggerRemoveBreakpoint,
                  "WriteProcessMemory failed, error = ", GetLastError())
        return false;
      }
      FlushInstructionCache(pi.hProcess, (void *)breakpoint->address, 1);

      // Remove breakpoint
      breakpoints.erase(filename_and_line_to_address_it->second);
      delete breakpoint;
    } else {
      LOG_IMGUI(DebuggerRemoveBreakpoint, "Breakpoint for ", filename, ", ",
                line, " doesn't exists!")
    }
  } else {
    LOG_IMGUI(DebuggerRemoveBreakpoint, "Unable to remove breakpoint in \"",
              filename, "\", ", line);
  }

  return true;
}

// TODO: Rethink callstack
static void DebuggerPrintCallstack(Debugger *debugger) {
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
}

static bool DebuggerStepOver(Debugger *debugger) {
  auto pi = debugger->pi;
  const auto &address_to_line = debugger->address_to_line;
  const auto &breakpoints = debugger->breakpoints;
  auto &invisible_breakpoints = debugger->invisible_breakpoints;
  const auto &current_line_address = debugger->current_line_address;

  auto current_line_it = address_to_line.find(current_line_address);
  ++current_line_it;

  if (breakpoints.find(current_line_it->first) == breakpoints.end()) {
    invisible_breakpoints[current_line_it->first] =
        CreateBreakpoint(pi.hProcess, current_line_it->first);
  } else {
    LOG_IMGUI(DebuggerStepOver, "Breakpoint \"", current_line_it->second.path,
              "\", ", current_line_it->second.index, " already exists")
  }

  return true;
}

// TODO: Use hash created by ImGuiManager
static bool DebuggerSetBreakpoint(Debugger *debugger,
                                  const std::string &filename, DWORD line) {
  const auto &filename_and_line_to_address =
      debugger->filename_and_line_to_address;
  auto &breakpoints = debugger->breakpoints;
  const auto pi = debugger->pi;

  DWORD64 filename_line_hash = GetStringDWORDHash(filename, line);
  DWORD64 filename_line_hash_temp = GetStringDWORDHash(filename, line);
  auto filename_and_line_to_address_it =
      filename_and_line_to_address.find(filename_line_hash);
  if (filename_and_line_to_address_it != filename_and_line_to_address.end()) {
    if (breakpoints.find(filename_and_line_to_address_it->second) ==
        breakpoints.end()) {
      breakpoints[filename_and_line_to_address_it->second] = CreateBreakpoint(
          pi.hProcess, filename_and_line_to_address_it->second);
      ;
    } else {
      LOG_IMGUI(DebuggerSetBreakpoint, "Breakpoint for", filename, ", ", line,
                " already present!")
      return false;
    }
  } else {
    LOG_IMGUI(DebuggerSetBreakpoint, "Unable to set breakpoint in \"", filename,
              "\", for line: ", line)
    return false;
  }

  return true;
}

inline void DebuggerWaitForAction(Debugger *debugger) {
  while (Global_IsOpen) {
    if (WaitForSingleObject(debugger->continue_event, 0) == WAIT_OBJECT_0) {
      break;
    }
  }
}

static bool DebuggerProcessEvent(Debugger *debugger, DEBUG_EVENT debug_event,
                                 DWORD &continue_status) {
  auto pi = debugger->pi;
  auto &breakpoints = debugger->breakpoints;
  auto &invisible_breakpoints = debugger->invisible_breakpoints;
  auto &address_to_line = debugger->address_to_line;

  continue_status = DBG_CONTINUE;

  switch (debug_event.dwDebugEventCode) {
  case LOAD_DLL_DEBUG_EVENT: {
    const LOAD_DLL_DEBUG_INFO load_dll_debug_info = debug_event.u.LoadDll;

    DebuggerLoadTargetModules(debugger, load_dll_debug_info.hFile, pi.hProcess,
                              (DWORD64)load_dll_debug_info.lpBaseOfDll);
  } break;
  case CREATE_PROCESS_DEBUG_EVENT: {
    const CREATE_PROCESS_DEBUG_INFO create_process_debug_info =
        debug_event.u.CreateProcessInfo;

    DebuggerLoadTargetModules(debugger, create_process_debug_info.hFile,
                              pi.hProcess,
                              (DWORD64)create_process_debug_info.lpBaseOfImage);

    // Replace first instruction with int3
    debugger->start_address =
        DebuggerGetTargetStartAddress(pi.hProcess, pi.hThread);

    const Line &line = address_to_line[debugger->start_address];

    if (debugger->OnLineIndexChange) {
      debugger->OnLineIndexChange(line);
    }

    invisible_breakpoints[debugger->start_address] =
        CreateBreakpoint(pi.hProcess, debugger->start_address);
  } break;
  case OUTPUT_DEBUG_STRING_EVENT: {
    const OUTPUT_DEBUG_STRING_INFO output_debug_string_info =
        debug_event.u.DebugString;
    WCHAR *message = new WCHAR[output_debug_string_info.nDebugStringLength];

    if (!ReadProcessMemory(pi.hProcess,
                           output_debug_string_info.lpDebugStringData, message,
                           output_debug_string_info.nDebugStringLength, NULL)) {
      LOG_IMGUI(DebuggerProcessEvent,
                "ReadProcessMemory failed, error = ", GetLastError())
      return false;
    }

    if (!output_debug_string_info.fUnicode) {
      LOG_IMGUI(OUTPUT_DEBUG_STRING_EVENT, (char *)message)
    } else {
      // LOG(OUTPUT_DEBUG_STRING_EVENT, message); // Output to console is not
      // supported for now (cause i'm dumb) :(
    }
  } break;
  case EXCEPTION_DEBUG_EVENT: {
    const EXCEPTION_DEBUG_INFO &exception_debug_info = debug_event.u.Exception;
    switch (exception_debug_info.ExceptionRecord.ExceptionCode) {
    case EXCEPTION_BREAKPOINT: {
      static bool ignore_breakpoint = true;
      if (ignore_breakpoint) {
        ignore_breakpoint = false;
      } else {
        DWORD64 exception_address =
            (DWORD64)exception_debug_info.ExceptionRecord.ExceptionAddress;

        const Line &line = address_to_line[exception_address];

        LOG_IMGUI(DebuggerProcessEvent, "Breakpoint at (", line.path, ", ",
                  std::dec, line.index, ", ", std::hex, exception_address, ')');

        debugger->current_line_address = exception_address;

        if (debugger->OnLineIndexChange) {
          debugger->OnLineIndexChange(line);
        }

        CONTEXT context;
        context.ContextFlags = CONTEXT_ALL;
        GetThreadContext(pi.hThread, &context);

        // Return address
        debugger->end_address = DebuggetGetFunctionReturnAddress(context, pi);
        if (invisible_breakpoints.find(debugger->end_address) ==
            invisible_breakpoints.end()) {
          invisible_breakpoints[debugger->end_address] =
              CreateBreakpoint(pi.hProcess, debugger->end_address);
        }

        Breakpoint *breakpoint = nullptr;
        const auto it = breakpoints.find(exception_address);
        if (it != breakpoints.end()) {
          breakpoint = breakpoints[exception_address];
        } else {
          breakpoint = invisible_breakpoints[exception_address];
        }

        debugger->original_context = context;

        DebuggerRestoreInstruction(debugger, breakpoint, context);

        // So it is an invisible breakpoint, need to delete
        if (it == breakpoints.end()) {
          invisible_breakpoints.erase(it->first);
        }
      }
    } break;
    case EXCEPTION_SINGLE_STEP: {
      // Reinstall exception
      if (breakpoints.find(debugger->original_context.Eip - 1) !=
          breakpoints.end()) {
        BYTE instruction = 0xcc;
        DWORD read_bytes;
        if (!WriteProcessMemory(pi.hProcess,
                                (void *)(debugger->original_context.Eip - 1),
                                &instruction, 1, &read_bytes)) {
          LOG_IMGUI(DebuggerProcessEvent,
                    "WriteProcessMemory failed, error = ", GetLastError())
          return false;
        }

        FlushInstructionCache(pi.hProcess,
                              (void *)(debugger->original_context.Eip - 1), 1);
      }

      // Wait for user events ("step over", "print callstack", ...)
      DebuggerWaitForAction(debugger);
    } break;
    default:
      if (exception_debug_info.dwFirstChance == 1) {
      }

      continue_status = DBG_EXCEPTION_NOT_HANDLED;
      break;
    }
  } break;
  case EXIT_PROCESS_DEBUG_EVENT: {
    LOG_IMGUI(DebuggerProcessEvent, "Process is terminated, exiting ...")
    exit(0);
  } break;
  default:
    continue_status = DBG_EXCEPTION_NOT_HANDLED;
    return true;
  }

  return true;
}

static void DebuggerRun(Debugger *debugger) {
  DEBUG_EVENT debug_event = {};
  while (Global_IsOpen) {
    DWORD continue_status;
    if (!WaitForDebugEvent(&debug_event, INFINITE) ||
        !DebuggerProcessEvent(debugger, debug_event, continue_status)) {
      Global_IsOpen = false;
      return;
    }

    ContinueDebugEvent(debug_event.dwProcessId, debug_event.dwThreadId,
                       continue_status);
  }
}