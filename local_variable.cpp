static LocalVariables CreateLocalVariables() {
  LocalVariables result = {};

  return result;
}

static void LocalVariablesReset(LocalVariables *local_variables) {
  auto &variables = local_variables->variables;
  variables.clear();
}