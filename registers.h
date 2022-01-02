// TODO: Make for x64
struct Registers {
  ULONG Edi;
  ULONG Esi;
  ULONG Ebx;
  ULONG Edx;
  ULONG Ecx;
  ULONG Eax;
  ULONG Ebp;
  ULONG Eip;
  ULONG SegCs;
  ULONG EFlags;
  ULONG Esp;
  ULONG SegSs;
};

#define ASSIGN_P_V(a, b, c) a->c = b.c // Assign 1 - pointer, 2 - by value