struct Breakpoint {
  DWORD64 address;
	BYTE original_instruction;
};

struct Line {
  DWORD index;
  std::string filename;
};

struct Debugger {
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  std::unordered_map<DWORD64, Breakpoint *> breakpoints;
  std::unordered_map<DWORD64, Breakpoint *> invisible_breakpoints;
  std::vector<std::string> source_files;
  std::map<DWORD64, Line> address_to_line; // Address, Line number
  std::unordered_map<DWORD64, DWORD64> filename_and_line_to_address;
  CONTEXT original_context;
  HANDLE continue_event;
  DWORD64 start_address;
  DWORD64 end_address; // To halt debugging when this address is being hit
  DWORD64 current_line_address;

  std::function<void(const std::vector<std::string>&)> OnLoadSourceFiles;
};