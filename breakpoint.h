struct Breakpoint {
  DWORD64 address;
  BYTE original_instruction;

  Breakpoint &operator=(const Breakpoint &other) {
    if (this != &other) {
      address = other.address;
      original_instruction = other.original_instruction;
    } else {
      assert(0);
    }

    return *this;
  }
};

struct Breakpoints {
  std::unordered_map<DWORD64, Breakpoint> data;
};