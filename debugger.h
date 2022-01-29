struct Line {
  DWORD index;
  DWORD64 address;
  DWORD64 hash; // filename + line index = hash
  std::string text;

  Line &operator=(const Line &other) {
    if (this != &other) {
      index = other.index;
      hash = other.hash;
      address = other.address;
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
  std::map<DWORD64, Line> address_to_line; // Used to get line based on address
  std::unordered_map<DWORD64, DWORD64>
      line_hash_to_address; // Used to set and remove breakpoints using line
                            // hash
  CONTEXT original_context;
  HANDLE continue_event;
  DWORD64 current_line_address;
  std::function<void(DWORD64)> OnLineHashChange;

  // Local modules
  Breakpoints invisible_breakpoints;

  // External modules
  Registers *registers;
  LocalVariables *local_variables;
  Source *source;
  Breakpoints *breakpoints;
};