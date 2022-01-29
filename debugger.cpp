static Debugger CreateDebugger(Registers *registers,
                               LocalVariables *local_variables, Source *source,
                               Breakpoints *breakpoints,
                               const std::string &process_name,
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
  result.registers = registers;
  result.local_variables = local_variables;
  result.source = source;
  result.breakpoints = breakpoints;

  return result;
}

struct EnumSymbolsCallbackData {
  Debugger *debugger;
  LocalVariables *local_variables;
  ULONG64 frame_address;
};

inline std::string DebuggerGetValueFromSymbol(Debugger *debugger,
                                              PSYMBOL_INFO symbol_info,
                                              ULONG64 frame_address) {
  const auto pi = debugger->pi;

  enum SymTagEnum tag = (enum SymTagEnum)0;
  SymGetTypeInfo(pi.hProcess, symbol_info->ModBase, symbol_info->TypeIndex,
                 TI_GET_SYMTAG, &tag);
  switch (tag) {
  case SymTagBaseType: {
    BasicType type = (BasicType)0;
    SymGetTypeInfo(pi.hProcess, symbol_info->ModBase, symbol_info->TypeIndex,
                   TI_GET_BASETYPE, &type);
    ULONG64 length = 0;
    SymGetTypeInfo(pi.hProcess, symbol_info->ModBase, symbol_info->TypeIndex,
                   TI_GET_LENGTH, &length);

    // Load actual value
    DWORD read_bytes;
    switch (type) {
    case btInt: {
      int data;
      if (ReadProcessMemory(pi.hProcess,
                            (void *)(frame_address + symbol_info->Address),
                            &data, length, &read_bytes)) {
        return std::to_string(data);
      }
    }

    break;
    case btFloat: {
      float data;
      if (ReadProcessMemory(pi.hProcess,
                            (void *)(frame_address + symbol_info->Address),
                            &data, length, &read_bytes)) {
        return std::to_string(data);
      }
    }

    break;
    default:
      return "Unsupported type";
      break;
    }
  } break;
  default:
    return "Unsupported type";
    break;
  }

  return "";
}

inline BOOL WINAPI EnumSymbolsCallback(PSYMBOL_INFO pSymInfo, ULONG SymbolSize,
                                       PVOID UserContext) {
  if ((pSymInfo->Flags & SYMFLAG_PARAMETER) == 0) {
    // Indicates that parameter on the stack is not a function argument
    // TODO: Remove return
    // return TRUE;
  }

  auto enum_symbols_callback_data =
      reinterpret_cast<EnumSymbolsCallbackData *>(UserContext);
  auto &local_variables = enum_symbols_callback_data->local_variables->data;

  std::string value =
      DebuggerGetValueFromSymbol(enum_symbols_callback_data->debugger, pSymInfo,
                                 enum_symbols_callback_data->frame_address);

  local_variables.emplace_back(LocalVariable{pSymInfo->Name, value});

  return TRUE;
}

inline void DebuggerGetLocalVariables(Debugger *debugger, CONTEXT context) {
  const auto &pi = debugger->pi;
  auto &local_variables = debugger->local_variables;

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
    return;
  }

  // Function
  PSYMBOL_INFO symbol_info =
      (PSYMBOL_INFO) new char[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
  symbol_info->MaxNameLen = MAX_SYM_NAME;
  symbol_info->SizeOfStruct = sizeof(SYMBOL_INFO);
  DWORD64 displacement;
  SymFromAddr(pi.hProcess, stack.AddrPC.Offset, &displacement, symbol_info);
  std::string name = symbol_info->Name;

  IMAGEHLP_STACK_FRAME stack_frame = {};
  stack_frame.InstructionOffset = stack.AddrPC.Offset;

  if (SymSetContext(pi.hProcess, &stack_frame, NULL) == FALSE &&
      GetLastError() != ERROR_SUCCESS) {
    return;
  }

  LocalVariablesReset(local_variables);

  EnumSymbolsCallbackData data{debugger, local_variables,
                               stack.AddrFrame.Offset};

  if (SymEnumSymbols(pi.hProcess, 0, NULL, EnumSymbolsCallback, (PVOID)&data) ==
      FALSE) {
    return;
  }
}

inline DWORD64 DebuggetGetFunctionReturnAddress(CONTEXT context,
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

  return stack.AddrReturn.Offset;
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

struct Function {
  DWORD64 start_address;
  DWORD64 end_address;
};

// TODO: Make function return false or true, just fill fields in here i guess.
// Do it with other similar functions
inline Function DebuggerGetFunctionInfo(Debugger *debugger, CONTEXT context) {
  Function result = {};

  auto pi = debugger->pi;

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
    return result;
  }

  PSYMBOL_INFO symbol_info =
      (PSYMBOL_INFO) new char[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
  symbol_info->MaxNameLen = MAX_SYM_NAME;
  symbol_info->SizeOfStruct = sizeof(SYMBOL_INFO);
  DWORD64 displacement;
  SymFromAddr(pi.hProcess, stack.AddrPC.Offset, &displacement, symbol_info);

  result.start_address = stack.AddrPC.Offset;
  result.end_address =
      stack.AddrPC.Offset + symbol_info->Size - 4; // -4 is a fix
  // TODO: If symbol_info->Size == 0, error

  return result;
}

inline void DebuggerPlaceFunctionInvisibleBreakpoints(Debugger *debugger,
                                                      CONTEXT context) {
  const auto source = debugger->source;
  const auto &filename_to_lines = source->filename_to_lines;
  const auto &address_to_line = source->address_to_line;
  auto &invisible_breakpoints = debugger->invisible_breakpoints;
  auto pi = debugger->pi;

  Function function_info = DebuggerGetFunctionInfo(debugger, context);
  auto begin = address_to_line.find(function_info.start_address);
  auto end = address_to_line.find(function_info.end_address);
  for (auto it = begin; it != end; ++it) {
    // TODO: Fix
    // if (invisible_breakpoints.find(it->first) == invisible_breakpoints.end())
    //   invisible_breakpoints[it->first] = CreateBreakpoint(pi.hProcess,
    //   it->first);
  }
}

inline BOOL WINAPI EnumSourceFilesCallback(PSOURCEFILE SourceFile,
                                           PVOID UserContext) {
  Debugger *debugger = (Debugger *)UserContext;
  // LOG(INFO) << "ModBase: " << std::hex << SourceFile->ModBase << ", Filename:
  // " << SourceFile->FileName << '\n';
  debugger->source_files.push_back(SourceFile->FileName);

  return TRUE;
}

struct EnumLinesCallbackData {
  PROCESS_INFORMATION pi;
  std::vector<Line> lines;
};

inline BOOL WINAPI EnumLinesCallback(PSRCCODEINFO LineInfo, PVOID UserContext) {
  // auto lines = reinterpret_cast<std::vector<Line> *>(UserContext);
  auto data = reinterpret_cast<EnumLinesCallbackData *>(UserContext);
  auto pi = data->pi;
  auto &lines = data->lines;
  // LOG_IMGUI_TO_FILE(INFO, "Module: ", LineInfo->FileName, " - Line: ",
  // std::dec,
  //                   LineInfo->LineNumber, " - Address: ", std::hex,
  //                   LineInfo->Address)
  // IMAGEHLP_LINE64 line = {};
  // line.SizeOfStruct = sizeof(line);

  // DWORD displacement;
  // if (SymGetLineFromAddr64(pi.hProcess, LineInfo->Address,
  //                          &displacement, &line)) {
  // }

  lines.emplace_back(
      Line{LineInfo->LineNumber, LineInfo->Address,
           GetStringDWORDHash(LineInfo->FileName, LineInfo->LineNumber)});

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
    LOG_IMGUI(INFO, "Loaded DLL ", filename, ", at address", base_address,
              module_info.SymType == SymPdb ? ", symbols loaded."
                                            : "symbols not loaded")

    if (module_info.SymType == SymPdb) {
      SymEnumSourceFiles(pi.hProcess, base, "*.[ic][np][lp?]",
                         EnumSourceFilesCallback, debugger);

      for (int i = 0; i < debugger->source_files.size(); ++i) {
        // std::vector<Line> lines;
        EnumLinesCallbackData data = {debugger->pi};
        SymEnumLines(pi.hProcess, base, NULL, debugger->source_files[i].c_str(),
                     EnumLinesCallback, (PVOID)&data);

        // Load text
        std::ifstream file(debugger->source_files[i]);
        if (!file.is_open()) {
          LOG_IMGUI(INFO, "Unable to open source file ",
                    debugger->source_files[i]);
          continue;
        }

        std::string line;
        std::vector<std::string> text;
        for (size_t j = 0; std::getline(file, line); ++j) {
          text.emplace_back(std::move(line));
        }

        // Copy text
        auto &lines = data.lines;
        for (size_t j = 0; j < lines.size(); ++j) {
          // TODO: Add lines that is not present in debugger info
          lines[j].text = text[lines[j].index - 1];
        }

        // Line specific staff
        for (size_t j = 0; j < lines.size(); ++j) {
          debugger->address_to_line[lines[j].address] = lines[j];
          debugger->line_hash_to_address[lines[j].hash] = lines[j].address;
        }

        auto source = debugger->source;
        const std::string filename =
            GetFilenameFromPath(debugger->source_files[i]);

        for (size_t j = 0; j < lines.size(); ++j) {
          source->address_to_line[lines[j].address] = lines[j];
        }
        source->filename_to_lines.emplace(
            std::make_pair(std::move(filename), std::move(lines)));
      }
    }
  } else {
    LOG_IMGUI(DebuggerLoadTargetModules, "Unable to load ", filename)
    return false;
  }

  return true;
}

static bool DebuggerRemoveBreakpoint(Debugger *debugger, DWORD64 hash) {
  const auto &line_hash_to_address = debugger->line_hash_to_address;
  auto &breakpoints = debugger->breakpoints->data;
  const auto pi = debugger->pi;

  auto line_hash_to_address_it = line_hash_to_address.find(hash);
  if (line_hash_to_address_it != line_hash_to_address.end()) {
    if (breakpoints.find(line_hash_to_address_it->second) !=
        breakpoints.end()) {

      Breakpoint breakpoint = breakpoints[line_hash_to_address_it->second];

      // Restore original instruction
      DWORD read_bytes;
      if (!WriteProcessMemory(pi.hProcess, (void *)breakpoint.address,
                              &breakpoint.original_instruction, 1,
                              &read_bytes)) {
        LOG_IMGUI(DebuggerRemoveBreakpoint,
                  "WriteProcessMemory failed, error = ", GetLastError())
        return false;
      }
      FlushInstructionCache(pi.hProcess, (void *)breakpoint.address, 1);

      // Remove breakpoint
      breakpoints.erase(line_hash_to_address_it->second);
    } else {
      LOG_IMGUI(DebuggerRemoveBreakpoint, "Breakpoint for ", hash,
                " doesn't exists!")
    }
  } else {
    LOG_IMGUI(DebuggerRemoveBreakpoint, "Unable to remove breakpoint for ",
              hash);
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
  auto &breakpoints = debugger->breakpoints->data;
  auto &invisible_breakpoints = debugger->invisible_breakpoints.data;
  const auto &current_line_address = debugger->current_line_address;

  auto current_line_it = address_to_line.find(current_line_address);
  ++current_line_it;

  if (breakpoints.find(current_line_it->first) == breakpoints.end()) {
    invisible_breakpoints[current_line_it->first] =
        CreateBreakpoint(pi.hProcess, current_line_it->first);
  } else {
    LOG_IMGUI(DebuggerStepOver, "Breakpoint \"", current_line_it->second.hash,
              "\", ", current_line_it->second.index, " already exists")
  }

  return true;
}

// TODO: Use hash created by ImGuiManager
static bool DebuggerSetBreakpoint(Debugger *debugger, DWORD64 hash) {
  const auto &line_hash_to_address = debugger->line_hash_to_address;
  auto &breakpoints = debugger->breakpoints->data;
  const auto pi = debugger->pi;

  auto line_hash_to_address_it = line_hash_to_address.find(hash);
  if (line_hash_to_address_it != line_hash_to_address.end()) {
    if (breakpoints.find(line_hash_to_address_it->second) ==
        breakpoints.end()) {
      breakpoints[line_hash_to_address_it->second] =
          CreateBreakpoint(pi.hProcess, line_hash_to_address_it->second);
      ;
    } else {
      LOG_IMGUI(DebuggerSetBreakpoint, "Breakpoint for", hash,
                ", already present!")
      return false;
    }
  } else {
    LOG_IMGUI(DebuggerSetBreakpoint, "Unable to set breakpoint for ", hash)
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
  auto &breakpoints = debugger->breakpoints->data;
  auto &invisible_breakpoints = debugger->invisible_breakpoints.data;
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

    CONTEXT context = {};
    context.ContextFlags = CONTEXT_ALL;
    GetThreadContext(pi.hThread, &context);

    // Replace first instruction with int3
    DWORD64 start_address =
        DebuggerGetTargetStartAddress(pi.hProcess, pi.hThread);

    const Line &line = address_to_line[start_address];

    if (debugger->OnLineHashChange) {
      debugger->OnLineHashChange(line.hash);
    }

    // TODO: Layter remove it to user to place that breakpoint
    // TODO: Mb make it special breakpoint

    breakpoints[line.address] = CreateBreakpoint(pi.hProcess, line.address);
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

        LOG_IMGUI(DebuggerProcessEvent, "Breakpoint at (", line.hash, ", ",
                  std::dec, line.index, ", ", std::hex, exception_address, ')');

        debugger->current_line_address = exception_address;

        if (debugger->OnLineHashChange) {
          debugger->OnLineHashChange(line.hash);
        }

        CONTEXT context = {};
        context.ContextFlags = CONTEXT_ALL;
        GetThreadContext(pi.hThread, &context);

        // Restore it to be before debug instruction, because exception already
        // occured, that means target instruction already been executed
        --context.Eip;

        debugger->original_context = context;

        context.EFlags |= 0x100; // Trap flag
        SetThreadContext(pi.hThread, &context);

#ifdef BREAKPOINT_END_ADDRESS
        DWORD64 end_address = DebuggetGetFunctionReturnAddress(context, pi);
        if (invisible_breakpoints.find(end_address) ==
            invisible_breakpoints.end()) {
          invisible_breakpoints[end_address] =
              CreateBreakpoint(pi.hProcess, end_address);
        }
#endif

        Breakpoint breakpoint = {};
        const auto it = breakpoints.find(exception_address);
        if (it != breakpoints.end()) {
          breakpoint = breakpoints[exception_address];
        } else {
          breakpoint = invisible_breakpoints[exception_address];
        }

        BreakpointRestore(pi.hProcess, breakpoint);

        // So it is an invisible breakpoint, need to delete
        if (it == breakpoints.end()) {
          invisible_breakpoints.erase(it->first);
        }

        RegistersUpdateFromContext(debugger->registers,
                                   debugger->original_context);
        DebuggerGetLocalVariables(debugger, debugger->original_context);
      }
    } break;
    case EXCEPTION_SINGLE_STEP: {
      // Reinstall exception if there is a breakpoint still
      if (breakpoints.find(debugger->original_context.Eip) !=
          breakpoints.end()) {
        BreakpointRestore(pi.hProcess, debugger->original_context.Eip, 0xCC);
      }

      // DebuggerPlaceFunctionInvisibleBreakpoints(debugger,
      // debugger->original_context);

      // Wait for user events
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