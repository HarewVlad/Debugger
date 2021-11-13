struct ImGuiLogRecord {
  std::string level;
  std::string message;
};

struct ImGuiLog {
  std::vector<ImGuiLogRecord> records;
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
};

template <typename ... T>
static inline void ImGuiLogAddHelper(const char *type, T && ... args);

#define LOG_IMGUI(TYPE, ...) ImGuiLogAddHelper(#TYPE, __VA_ARGS__);