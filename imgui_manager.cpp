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
  auto &breakpoints = imgui_manager->breakpoints;
  const auto &current_line = imgui_manager->current_line;
  const auto &filename_to_source_code_line_info =
      imgui_manager->filename_to_source_code_line_info;

  ImGui::Begin("Code");
  ImGui::BeginTabBar("Files");

  for (auto it = filename_to_source_code_line_info.begin();
       it != filename_to_source_code_line_info.end(); ++it) {
    if (ImGui::BeginTabItem(it->first.c_str())) {
      for (size_t i = 0; i < it->second.size(); ++i) {
        static const float circle_offset_x = 17.0f;
        static const float line_number_offset_y = 2.5f;
        static const float scroll_y = ImGui::GetScrollY();

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + line_number_offset_y);
        ImGui::Text("%d", i + 1);
        ImGui::SameLine();

        // Breakpoints
        if (breakpoints.find(it->second[i].hash) != breakpoints.end()) {
          // Draw red circle
          ImDrawList *draw_list = ImGui::GetWindowDrawList();

          const ImVec2 scroll =
              ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());

          draw_list->AddCircleFilled(
              ImGui::GetWindowPos() + ImGui::GetCursorPos() - ImVec2(-5, -6) -
                  scroll,
              10, ImGui::GetColorU32(ImVec4(1, 0, 0, 1)), 10);
        }

        // Draw cursor
        float h = 4 / 7.0f;
        if (current_line.hash == it->second[i].hash &&
            current_line.index == it->second[i].index) {
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
        ImGui::PushID(i);
        if (ImGui::Button(it->second[i].text.c_str())) {
          if (breakpoints.find(it->second[i].hash) != breakpoints.end()) {
            if (imgui_manager->OnRemoveBreakpoint &&
                imgui_manager->OnRemoveBreakpoint(it->second[i].hash)) {
              breakpoints.erase(it->second[i].hash);
            }
          } else {
            if (imgui_manager->OnSetBreakpoint &&
                imgui_manager->OnSetBreakpoint(it->second[i].hash)) {
              breakpoints.insert(it->second[i].hash);
            }
          }
        }
        ImGui::PopID();
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
  imgui_log->records.emplace_back(ImGuiLogRecord{type + ":", text});
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

static void ImGuiManagerLoadSourceFile(
    ImGuiManager *imgui_manager,
    const std::unordered_map<std::string, std::vector<Line>>
        &source_filename_to_lines) {
  auto &filename_to_source_code_line_info =
      imgui_manager->filename_to_source_code_line_info;

  for (auto it = source_filename_to_lines.begin();
       it != source_filename_to_lines.end(); ++it) {
    std::ifstream *file = new std::ifstream(it->first);
    if (!file->is_open()) {
      continue;
    }

    // 1. Fill line context
    std::string line;
    std::vector<ImGuiLineInfo> source_code_lines;
    for (size_t i = 0; std::getline(*file, line); ++i) {
      source_code_lines.emplace_back(ImGuiLineInfo{0, 0, line});
    }

    // 2. Fill line index and hash from input
    for (size_t i = 0; i < it->second.size(); ++i) {
      const Line &line = it->second[i];
      source_code_lines[line.index - 1].index = line.index;
      source_code_lines[line.index - 1].hash = line.hash;
    }

    const std::string filename = GetFilenameFromPath(it->first);
    filename_to_source_code_line_info.emplace(
        std::make_pair(std::move(filename), std::move(source_code_lines)));
  }
}

inline void ImGuiLogWriteToFile(ImGuiLog *imgui_log, const std::string &type,
                                const std::string &text) {
  auto &file = imgui_log->file;

  if (file.is_open()) {
    file << type << ": " << text;
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

template <typename... T>
static inline void ImGuiLogAddHelperToFile(const char *type, T &&... args) {
  std::stringstream ss;
  const auto log = [&](const auto &arg) -> int {
    ss << arg;
    return 0;
  };

  (void)std::initializer_list<int>{log(args)...};

  ss << '\n';

  ImGuiLogWriteToFile(&Global_ImGuiLog, type, ss.str());
}

static bool ImGuiLogInitialize(ImGuiLog *imgui_log) {
  auto &file = imgui_log->file;

  file.open(LOG_FILENAME, std::ofstream::out | std::ofstream::trunc);
  if (!file.is_open()) {
    return false;
  }

  return true;
}