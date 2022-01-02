static Registers CreateRegisters() {
  Registers result = {};

  return result;
}

// TODO: Make for x64
static void RegistersUpdateFromContext(Registers *registers, CONTEXT context) {
  ASSIGN_P_V(registers, context, Edi);
  ASSIGN_P_V(registers, context, Esi);
  ASSIGN_P_V(registers, context, Ebx);
  ASSIGN_P_V(registers, context, Edx);
  ASSIGN_P_V(registers, context, Ecx);
  ASSIGN_P_V(registers, context, Eax);
  ASSIGN_P_V(registers, context, Ebp);
  ASSIGN_P_V(registers, context, Eip);
  ASSIGN_P_V(registers, context, SegCs);
  ASSIGN_P_V(registers, context, EFlags);
  ASSIGN_P_V(registers, context, Esp);
  ASSIGN_P_V(registers, context, SegSs);
}