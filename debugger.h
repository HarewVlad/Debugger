struct Breakpoint {
	BYTE original_instruction;
	DWORD address;
};

struct Debugger {
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  std::unordered_map<int, Breakpoint *> breakpoints;
};