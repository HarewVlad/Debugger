#include "main.h"
#include "utils.cpp"
#include "debugger.cpp"

int main(int argc, char **argv) {
  if (argc < 2) {
    LOG(INFO) <<  "Specify process name\n";
    return 1;
  }

  Debugger *debugger = CreateDebugger(argv[1]);

  DebuggerRun(debugger);
}