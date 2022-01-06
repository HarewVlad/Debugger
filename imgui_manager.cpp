static ImGuiManager CreateImGuiManager(Registers *registers,
                                       LocalVariables *local_variables) {
  ImGuiManager result;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();
  ImGui::StyleColorsClassic();

  result.registers = registers;
  result.local_variables = local_variables;

  return result;
}

// TODO: Change for x64
inline void ImGuiDrawRegisters(ImGuiManager *imgui_manager) {
  const auto registers = imgui_manager->registers;

  ImGui::Begin("Registers");
  ImGui::Text("Edi: %lu", registers->Edi);
  ImGui::Text("Esi: %lu", registers->Esi);
  ImGui::Text("Ebx: %lu", registers->Ebx);
  ImGui::Text("Edx: %lu", registers->Edx);
  ImGui::Text("Ecx: %lu", registers->Ecx);
  ImGui::Text("Eax: %lu", registers->Eax);
  ImGui::Text("Ebp: %lu", registers->Ebp);
  ImGui::Text("Eip: %lu", registers->Eip);
  ImGui::Text("SegCs: %lu", registers->SegCs);
  ImGui::Text("EFlags: %lu", registers->EFlags);
  ImGui::Text("Esp: %lu", registers->Esp);
  ImGui::Text("SegSs: %lu", registers->SegSs);

  ImGui::End();
}

inline void ImGuiDrawLocalVariables(ImGuiManager *imgui_manager) {
  const auto &local_variables = imgui_manager->local_variables;
  const auto &variables = local_variables->variables;

  ImGui::Begin("Local variables");
  for (size_t i = 0; i < variables.size(); ++i) {
    ImGui::Text("Name: %s, Value = %s", variables[i].name.c_str(),
                variables[i].value.c_str());
  }
  ImGui::End();
}

inline void ImGuiDrawCode(ImGuiManager *imgui_manager) {
  auto &breakpoints = imgui_manager->breakpoints;
  DWORD64 current_line_hash = imgui_manager->current_line_hash;
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
        if (current_line_hash == it->second[i].hash) {
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
  static bool is_f5_pressed = false;
  static bool is_f10_pressed = false;

  ImGui::Begin("Interface");
  if (ImGui::Button("Continue") ||
      (GetAsyncKeyState(VK_F5) & 0x8000) && !is_f5_pressed) {
    imgui_manager->OnContinue();

    is_f5_pressed = true;
  } else if (!(GetAsyncKeyState(VK_F5) & 0x8000)) {
    is_f5_pressed = false;
  }

  if (ImGui::Button("Step over") ||
      (GetAsyncKeyState(VK_F10) & 0x8000) && !is_f10_pressed) {
    imgui_manager->OnStepOver();

    is_f10_pressed = true;
  } else if (!(GetAsyncKeyState(VK_F10) & 0x8000)) {
    is_f10_pressed = false;
  }

  // if (ImGui::Button("Callstack")) {
  //   imgui_manager->OnPrintCallstack();
  // }

  if (ImGui::Button("Exit") || (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
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
  ImGuiDrawRegisters(imgui_manager);
  ImGuiDrawLocalVariables(imgui_manager);
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