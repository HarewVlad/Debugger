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

  UserInterface *user_interface = CreateUserInterface(continue_event);

  Debugger *debugger;
  std::thread([&]() {
    debugger = CreateDebugger(argv[1], continue_event);
    DebuggerRun(debugger);
  }).detach();

  std::string input = {};
  while (true) {
    std::getline(std::cin, input);

    if (input == "aight") {
      break;
    } else if (input == "step_over") {
      DebuggerStepOver(debugger);

      SetEvent(continue_event);
    } else if (input == "callstack") {
      DebuggerPrintCallstack(debugger);
    } else if (input == "registers") {
      DebuggerPrintRegisters(debugger);
    }

    Sleep(100);
  }
}