static UserInterface *CreateUserInterface(HANDLE continue_event) {
  UserInterface *result = new UserInterface;

  result->continue_event = continue_event;

  return result;
}