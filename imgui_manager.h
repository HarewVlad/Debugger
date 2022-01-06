struct ImGuiLogRecord {
  std::string level;
  std::string message;
};

struct ImGuiLog {
  std::vector<ImGuiLogRecord> records;
  std::ofstream file;
};

struct ImGuiLineInfo {
  DWORD index;
  DWORD64 hash;
  std::string text;
};

struct ImGuiManager {
  std::function<void()> OnStepOver;
  std::function<void()> OnPrintCallstack;
  std::function<bool(DWORD64)> OnSetBreakpoint;
  std::function<bool(DWORD64)> OnRemoveBreakpoint;
  std::function<void()> OnContinue;

  std::unordered_map<std::string, std::vector<ImGuiLineInfo>> filename_to_source_code_line_info;
  std::set<DWORD64> breakpoints;
  DWORD64 current_line_hash;

  // Modules
  Registers *registers;
  LocalVariables *local_variables;
};

template <typename ... T>
static inline void ImGuiLogAddHelper(const char *type, T && ... args);

template <typename ... T>
static inline void ImGuiLogAddHelperToFile(const char *type, T && ... args);

#define LOG_FILENAME "imgui_log.txt"
#define LOG_IMGUI(TYPE, ...) ImGuiLogAddHelper(#TYPE, __VA_ARGS__);
#define LOG_IMGUI_TO_FILE(TYPE, ...) ImGuiLogAddHelperToFile(#TYPE, __VA_ARGS__);