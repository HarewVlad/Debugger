struct UserInterface {
  std::function<void()> step_over_callback;
  std::function<void()> print_callstack_callback;
  std::function<void()> print_registers_callback;
  std::function<void(const std::string&, DWORD)> set_breakpoint_callback;

  HWND window;
};