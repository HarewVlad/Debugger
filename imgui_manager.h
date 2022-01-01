struct ImGuiLogRecord {
  std::string level;
  std::string message;
};

struct ImGuiLog {
  std::vector<ImGuiLogRecord> records;
  std::ofstream file;
};

struct ImGuiManager {
  std::function<void()> OnStepOver;
  std::function<void()> OnPrintCallstack;
  std::function<void()> OnPrintRegisters;
  std::function<bool(const std::string&, DWORD)> OnSetBreakpoint;
  std::function<bool(const std::string&, DWORD)> OnRemoveBreakpoint;
  std::function<void()> OnContinue;

  std::unordered_map<std::string, std::vector<std::string>> path_to_source_code;
  std::unordered_map<std::string, std::string> path_to_filename;
  std::set<DWORD64> breakpoints;
  Line current_line;
};

template <typename ... T>
static inline void ImGuiLogAddHelper(const char *type, T && ... args);

template <typename ... T>
static inline void ImGuiLogAddHelperToFile(const char *type, T && ... args);

#define LOG_FILENAME "imgui_log.txt"
#define LOG_IMGUI(TYPE, ...) ImGuiLogAddHelper(#TYPE, __VA_ARGS__);
#define LOG_IMGUI_TO_FILE(TYPE, ...) ImGuiLogAddHelperToFile(#TYPE, __VA_ARGS__);