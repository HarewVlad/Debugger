enum class BreakpointType {
  USER,
  INVISIBLE
};

struct Breakpoint {
  DWORD64 address;
  BYTE original_instruction;
  BreakpointType type;

  Breakpoint &operator=(const Breakpoint &other) {
    if (this != &other) {
      address = other.address;
      original_instruction = other.original_instruction;
      type = other.type;
    } else {
      assert(0);
    }

    return *this;
  }
};

struct Breakpoints {
  std::unordered_map<DWORD64, Breakpoint> data;
};