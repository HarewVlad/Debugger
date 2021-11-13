static ImGuiManager CreateImGuiManager() {
  ImGuiManager result;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();
  ImGui::StyleColorsClassic();

  return result;
}

inline void ImGuiDrawCode(ImGuiManager *imgui_manager) {
  const auto &path_to_source_code = imgui_manager->path_to_source_code;
  auto &breakpoints = imgui_manager->breakpoints;
  const auto &path_to_filename = imgui_manager->path_to_filename;
  const auto &current_line = imgui_manager->current_line;

  ImGui::Begin("Code");
  ImGui::BeginTabBar("Files");

  for (auto it = path_to_source_code.begin(); it != path_to_source_code.end();
       ++it) {
    const std::string filename = path_to_filename.at(it->first);
    if (ImGui::BeginTabItem(filename.c_str())) {
      for (size_t i = 0; i < it->second.size(); ++i) {
        // TODO: Precompute hash
        const DWORD64 filename_line_hash = GetStringDWORDHash(it->first, i + 1);
        static const float circle_offset_x = 17.0f;
        static const float line_number_offset_y = 2.5f;

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + line_number_offset_y);
        ImGui::Text("%d", i + 1);
        ImGui::SameLine();
        if (breakpoints.find(filename_line_hash) != breakpoints.end()) {
          // Draw debug cirle
          ImDrawList *draw_list = ImGui::GetWindowDrawList();

          draw_list->AddCircleFilled(
              ImGui::GetWindowPos() + ImGui::GetCursorPos() - ImVec2(-5, -6),
              10, ImGui::GetColorU32(ImVec4(1, 0, 0, 1)), 10);
        }

        // Cursor
        float h = 4 / 7.0f;
        if (current_line.path == it->first &&
            current_line.index == i + 1) { // TODO: Cache
          h = 8 / 7.0f;
        }

        ImGui::PushStyleColor(ImGuiCol_Button,
                              (ImVec4)ImColor::HSV(h, 0.6f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              (ImVec4)ImColor::HSV(h, 0.7f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              (ImVec4)ImColor::HSV(h, 0.8f, 0.8f));
        ImGui::SameLine();
        ImGui::SetCursorPos({ImGui::GetCursorPosX() + circle_offset_x,
                             ImGui::GetCursorPosY() - line_number_offset_y});
        if (ImGui::Button(it->second[i].c_str())) {
          if (breakpoints.find(filename_line_hash) != breakpoints.end()) {
            if (imgui_manager->OnRemoveBreakpoint &&
                imgui_manager->OnRemoveBreakpoint(it->first, i + 1)) {
              breakpoints.erase(filename_line_hash);
            }
          } else {
            if (imgui_manager->OnSetBreakpoint &&
                imgui_manager->OnSetBreakpoint(it->first, i + 1)) {
              breakpoints.insert(filename_line_hash);
            }
          }
        }
        ImGui::PopStyleColor(3);
      }
      ImGui::EndTabItem();
    }
  }
  ImGui::EndTabBar();
  ImGui::End();
}

inline void ImGuiManagerDrawInterface(ImGuiManager *imgui_manager) {
  ImGui::Begin("Interface");
  if (ImGui::Button("Continue")) {
    imgui_manager->OnContinue();
  }
  if (ImGui::Button("Step over")) {
    imgui_manager->OnStepOver();
  }

  if (ImGui::Button("Callstack")) {
    imgui_manager->OnPrintCallstack();
  }

  if (ImGui::Button("Registers")) {
    imgui_manager->OnPrintRegisters();
  }

  if (ImGui::Button("Exit")) {
    Global_IsOpen = false;
  }

  ImGui::End();
}

static inline void ImGuiLogDraw(ImGuiLog *imgui_log) {
  const auto &records = imgui_log->records;

  ImGui::Begin("Log");
  for (size_t i = 0; i < records.size(); ++i) {
    ImGui::Text(records[i].level.c_str());
    ImGui::SameLine();
    ImGui::Text(records[i].message.c_str());
  }
  ImGui::End();
}

static inline void ImGuiLogAdd(ImGuiLog *imgui_log, const std::string &type,
                               const std::string &text) {
  imgui_log->records.emplace_back(ImGuiLogRecord{type, text});
}

static void ImGuiManagerDraw(ImGuiManager *imgui_manager) {
  // static bool draw_demo = true;
  // ImGui::ShowDemoWindow(&draw_demo);

  ImGuiManagerDrawInterface(imgui_manager);
  ImGuiDrawCode(imgui_manager);
  ImGuiLogDraw(&Global_ImGuiLog);
}

static void ImGuiManagerBeginDirectx11() {
  ImGui_ImplDX11_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
}

static void ImGuiManagerEndDirectx11() {
  ImGui::Render();
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

static void
ImGuiManagerLoadSourceFile(ImGuiManager *imgui_manager,
                           const std::vector<std::string> &source_files) {
  auto &path_to_source_code = imgui_manager->path_to_source_code;
  auto &path_to_filename = imgui_manager->path_to_filename;

  for (size_t i = 0; i < source_files.size(); ++i) {
    std::ifstream *file = new std::ifstream(source_files[i]);
    if (file->is_open()) {
      const std::string path = source_files[i];

      std::string line;
      std::vector<std::string> source_code;
      while (std::getline(*file, line)) {
        source_code.emplace_back(std::move(line));
      }

      path_to_source_code[path] = source_code;
      path_to_filename[path] = GetFilenameFromPath(path);
    }
  }
}

template <typename... T>
static inline void ImGuiLogAddHelper(const char *type, T &&... args) {
  std::stringstream ss;
  const auto log = [&](const auto &arg) -> int {
    ss << arg;
    return 0;
  };

  (void)std::initializer_list<int>{log(args)...};

  ImGuiLogAdd(&Global_ImGuiLog, type, ss.str());
}