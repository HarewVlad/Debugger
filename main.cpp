#include "main.h"
#include "utils.cpp"
#include "debugger.cpp"
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

  std::thread([&]() {
    UserInterfaceRun(&user_interface);
  }).detach();

  DebuggerRun(&debugger);
}