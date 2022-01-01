struct Breakpoint {
  DWORD64 address;
  BYTE original_instruction;
};

// NOTE: Store only hashes of lines

struct Line {
  DWORD index;
  DWORD64 hash; // filename + line index = hash

  Line &operator=(const Line &other) {
    if (this != &other) {
      index = other.index;
      hash = other.hash;
    } else {
      assert(0);
    }

    return *this;
  }
};

struct Debugger {
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  std::unordered_map<DWORD64, Breakpoint *>
      breakpoints; // User defined breakpoints
  std::unordered_map<DWORD64, Breakpoint *>
      invisible_breakpoints; // Our internal breakpoints to do some stuff
  std::vector<std::string> source_files;
  std::map<DWORD64, Line> address_to_line; // Used to get line based on address
  std::unordered_map<DWORD64, DWORD64>
      line_hash_to_address; // Used to set and remove breakpoints using line
                            // hash
  std::unordered_map<std::string, std::vector<Line>>
      source_filename_to_lines; // Used to map sources to vector of lines
  CONTEXT original_context;
  HANDLE continue_event;
  DWORD64 current_line_address;

  std::function<void(
      const std::unordered_map<std::string, std::vector<Line>> &)>
      OnLoadSourceFiles;
  std::function<void(const Line &)> OnLineIndexChange;
};