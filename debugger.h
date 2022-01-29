enum class DebuggerState {
  NONE,
  STEP_OVER,
  CONTINUE
};

struct Line {
  DWORD index;
  DWORD64 address;
  std::string text;

  Line &operator=(const Line &other) {
    if (this != &other) {
      index = other.index;
      address = other.address;
      text = other.text; // TODO: Mb replace with move later
    } else {
      assert(0);
    }

    return *this;
  }
};

// From "cvconst.h"
enum BasicType {
  btNoType = 0,
  btVoid = 1,
  btChar = 2,
  btWChar = 3,
  btInt = 6,
  btUInt = 7,
  btFloat = 8,
  btBCD = 9,
  btBool = 10,
  btLong = 13,
  btULong = 14,
  btCurrency = 25,
  btDate = 26,
  btVariant = 27,
  btComplex = 28,
  btBit = 29,
  btBSTR = 30,
  btHresult = 31,
};

// From "cvconst.h"
enum SymTagEnum {
  SymTagNull,
  SymTagExe,
  SymTagCompiland,
  SymTagCompilandDetails,
  SymTagCompilandEnv,
  SymTagFunction,
  SymTagBlock,
  SymTagData,
  SymTagAnnotation,
  SymTagLabel,
  SymTagPublicSymbol,
  SymTagUDT,
  SymTagEnum,
  SymTagFunctionType,
  SymTagPointerType,
  SymTagArrayType,
  SymTagBaseType,
  SymTagTypedef,
  SymTagBaseClass,
  SymTagFriend,
  SymTagFunctionArgType,
  SymTagFuncDebugStart,
  SymTagFuncDebugEnd,
  SymTagUsingNamespace,
  SymTagVTableShape,
  SymTagVTable,
  SymTagCustom,
  SymTagThunk,
  SymTagCustomType,
  SymTagManagedType,
  SymTagDimension,
  SymTagCallSite,
  SymTagInlineSite,
  SymTagBaseInterface,
  SymTagVectorType,
  SymTagMatrixType,
  SymTagHLSLType
};

struct Source;

struct Debugger {
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  std::vector<std::string> source_files;
  CONTEXT original_context;
  HANDLE continue_event;
  std::function<void(DWORD64)> OnLineAddressChange;
  DebuggerState state;

  // Local modules
  Breakpoints invisible_breakpoints;

  // External modules
  Registers *registers;
  LocalVariables *local_variables;
  Source *source;
  Breakpoints *breakpoints;
};