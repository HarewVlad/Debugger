struct Line {
  DWORD line_number;
  DWORD64 address;
};

struct Breakpoint {
  Line line;
	BYTE original_instruction;
};

struct Debugger {
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  std::unordered_map<DWORD64, Breakpoint *> breakpoints;
  std::vector<std::string> source_files;
  std::unordered_map<std::string, std::vector<Line>> lines;
};