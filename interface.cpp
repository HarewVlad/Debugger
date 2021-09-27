static UserInterface CreateUserInterface() {
  UserInterface result;

  return result;
}

static void UserInterfaceRun(UserInterface *user_interface) {
  std::string input = {};
  while (true) {
    std::getline(std::cin, input);

    if (input == "aight") {
      break;
    } else if (input == "step_over") {
      if (user_interface->step_over_callback) {
        user_interface->step_over_callback();
      }
    } else if (input == "callstack") {
      if (user_interface->print_callstack_callback) {
        user_interface->print_callstack_callback();
      }
    } else if (input == "registers") {
      if (user_interface->print_registers_callback) {
        user_interface->print_registers_callback();
      }
    }

    Sleep(100);
  }
}