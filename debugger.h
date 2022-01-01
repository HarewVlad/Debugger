struct Breakpoint {
  DWORD64 address;
	BYTE original_instruction;
};

// NOTE: Store only hashes of lines

struct Line {
  DWORD index;
  std::string path;

  Line& operator=(const Line& other) {
    if (this != &other) {
      index = other.index;
      path = other.path;
    } else {
      assert(0);
    }

    return *this;
  }
};

struct Debugger {
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  std::unordered_map<DWORD64, Breakpoint *> breakpoints;
  std::unordered_map<DWORD64, Breakpoint *> invisible_breakpoints;
  std::vector<std::string> source_files;
  std::map<DWORD64, Line> address_to_line; // Address, Line number
  std::unordered_map<DWORD64, DWORD64> line_hash_to_address; // Line hash = filename hash + line number hash
  CONTEXT original_context;
  HANDLE continue_event;
  DWORD64 current_line_address;

  std::function<void(const std::vector<std::string>&)> OnLoadSourceFiles;
  std::function<void(const Line &)> OnLineIndexChange;
};