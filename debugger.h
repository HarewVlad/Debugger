struct Breakpoint {
  DWORD64 address;
	BYTE original_instruction;
};

struct Debugger {
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  std::unordered_map<DWORD64, Breakpoint *> breakpoints;
  std::vector<std::string> source_files;
  std::map<DWORD64, int> lines; // Address, Line number
  CONTEXT original_context;
  HANDLE continue_event;
  DWORD64 start_address;
  DWORD64 end_address; // To halt debugging when this address is being hit
  DWORD64 current_line_address;
};