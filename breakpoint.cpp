// TODO: Mb redesign function return error codes
static Breakpoint CreateBreakpoint(HANDLE process, DWORD64 address) {
  Breakpoint result = {};

  BYTE instruction;
  DWORD read_bytes;
  if (!ReadProcessMemory(process, (void *)address, &instruction, 1,
                         &read_bytes)) {
    LOG_IMGUI(DebuggerProcessEvent,
              "ReadProcessMemory failed, error = ", GetLastError())
    return result;
  }

  BYTE original_instruction = instruction;

  instruction = 0xcc;
  WriteProcessMemory(process, (void *)address, &instruction, 1, &read_bytes);
  FlushInstructionCache(process, (void *)address, 1);

  result.original_instruction = original_instruction;
  result.address = address;

  return result;
}

static inline bool BreakpointRestore(HANDLE process,
                                     const Breakpoint &breakpoint) {
  DWORD read_bytes;
  if (!WriteProcessMemory(process, (void *)breakpoint.address,
                          &breakpoint.original_instruction, 1, &read_bytes)) {
    LOG_IMGUI(DebuggerRestoreInstruction,
              "WriteProcessMemory failed, error = ", GetLastError())
    return false;
  }
  FlushInstructionCache(process, (void *)breakpoint.address, 1);

  return true;
}

static inline bool BreakpointRestore(HANDLE process, DWORD64 address,
                                     BYTE instruction) {
  DWORD read_bytes;
  if (!WriteProcessMemory(process, (void *)address, &instruction, 1,
                          &read_bytes)) {
    LOG_IMGUI(DebuggerRestoreInstruction,
              "WriteProcessMemory failed, error = ", GetLastError())
    return false;
  }
  FlushInstructionCache(process, (void *)address, 1);

  return true;
}