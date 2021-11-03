#include "main.h"
#include "utils.cpp"
#include "debugger.cpp"
#include "imgui/imgui.cpp"
#include "imgui/imgui_impl_win32.cpp"
#include "imgui/imgui_impl_dx11.cpp"
#include "interface.cpp"

int main(int argc, char **argv) {
  if (argc < 2) {
    LOG(INFO) << "Specify process name\n";
    return 1;
  }

  HANDLE continue_event = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (!continue_event) {
    return 1;
  }

  Debugger debugger = CreateDebugger(argv[1], continue_event);
  UserInterface user_interface = CreateUserInterface();
  user_interface.step_over_callback = [&](){
    DebuggerStepOver(&debugger);

    SetEvent(continue_event);
  };
  user_interface.print_callstack_callback = [&]() {
    DebuggerPrintCallstack(&debugger);
  };
  user_interface.print_registers_callback = [&]() {
    DebuggerPrintRegisters(&debugger);
  };
  user_interface.set_breakpoint_callback = [&](const std::string& filename, DWORD line) {
    DebuggerSetBreakpoint(&debugger, filename, line);
  };

  std::thread([&]() {
    UserInterfaceRun(&user_interface);
  }).detach();
  
  DebuggerRun(&debugger);
}