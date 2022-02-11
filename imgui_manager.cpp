static ImGuiManager CreateImGuiManager(Registers *registers,
                                       LocalVariables *local_variables,
                                       Source *source,
                                       Breakpoints *breakpoints) {
  ImGuiManager result;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui::StyleColorsDark();
  ImGui::StyleColorsClassic();

  result.registers = registers;
  result.local_variables = local_variables;
  result.source = source;
  result.breakpoints = breakpoints;
  result.previous_line_address = 0;

  return result;
}

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
  const auto local_variables = imgui_manager->local_variables;
  const auto &data = local_variables->data;

  ImGui::Begin("Local variables");
  for (size_t i = 0; i < data.size(); ++i) {
    ImGui::Text("Name: %s, Value = %s", data[i].name.c_str(),
                data[i].value.c_str());
  }
  ImGui::End();
}

inline void ImGuiDrawCode(ImGuiManager *imgui_manager) {
  auto &breakpoints = imgui_manager->breakpoints->data;
  DWORD64 current_line_address = imgui_manager->current_line_address;
  const auto &filename_to_lines = imgui_manager->source->filename_to_lines;
  const auto &line_address_to_filename =
      imgui_manager->source->line_address_to_filename;
  auto &previous_line_address = imgui_manager->previous_line_address;

  ImGui::Begin("Code");
  ImGui::BeginTabBar("Files");

  int tab_button_index = 0;
  for (auto it = filename_to_lines.begin(); it != filename_to_lines.end();
       ++it, ++tab_button_index) {
    static int current_tab_button_index = 0;

    if (previous_line_address != current_line_address) {
      if (line_address_to_filename.find(current_line_address) !=
              line_address_to_filename.end() &&
          line_address_to_filename.at(current_line_address) == it->first) {
        current_tab_button_index = tab_button_index;

        previous_line_address = current_line_address;
      }
    }

    if (ImGui::TabItemButton(it->first.c_str()) ||
        current_tab_button_index == tab_button_index) {
      for (size_t i = 0; i < it->second.size(); ++i) {
        static const float circle_offset_x = 17.0f;
        static const float line_number_offset_y = 2.5f;
        static const float scroll_y = ImGui::GetScrollY();

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + line_number_offset_y);
        ImGui::Text("%d", i + 1);
        ImGui::SameLine();

        // Breakpoints
        auto breakpoint_it = breakpoints.find(it->second[i].address);
        if (breakpoint_it != breakpoints.end() &&
            breakpoint_it->second.type == BreakpointType::USER) {
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
        float h = 0.571428f; // 4 / 7
        if (current_line_address == it->second[i].address) {
          h = 1.142857f; // 8 / 7
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
          if (breakpoint_it != breakpoints.end() &&
              breakpoint_it->second.type == BreakpointType::USER) {
            if (imgui_manager->OnRemoveBreakpoint) {
              imgui_manager->OnRemoveBreakpoint(it->second[i].address);
            }
          } else {
            if (imgui_manager->OnSetBreakpoint) {
              imgui_manager->OnSetBreakpoint(it->second[i].address);
            }
          }
        }
        ImGui::PopID();
        ImGui::PopStyleColor(3);
      }
    }

    if (ImGui::IsItemActive()) {
      current_tab_button_index = tab_button_index;
    }
  }

  ImGui::EndTabBar();
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
  auto &records = imgui_log->records;

  records.emplace_back(ImGuiLogRecord{type + ":", text});

  if (records.size() > IMGUI_LOG_MAX_SIZE) {
    records.erase(records.begin());
  }
}

static void ImGuiManagerUpdate(ImGuiManager *imgui_manager) {
  static bool is_f5_pressed = false;
  static bool is_f10_pressed = false;
  static bool is_f11_pressed = false;

  if ((GetAsyncKeyState(VK_F5) & 0x8000) && !is_f5_pressed) {
    imgui_manager->OnContinue();

    is_f5_pressed = true;
  } else if (!(GetAsyncKeyState(VK_F5) & 0x8000)) {
    is_f5_pressed = false;
  }

  if ((GetAsyncKeyState(VK_F10) & 0x8000) && !is_f10_pressed) {
    imgui_manager->OnStepOver();

    is_f10_pressed = true;
  } else if (!(GetAsyncKeyState(VK_F10) & 0x8000)) {
    is_f10_pressed = false;
  }

  if ((GetAsyncKeyState(VK_F11) & 0x8000) && !is_f11_pressed) {
    imgui_manager->OnStepIn();

    is_f11_pressed = true;
  } else if (!(GetAsyncKeyState(VK_F11) & 0x8000)) {
    is_f11_pressed = false;
  }

  if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
    Global_IsOpen = false;
  }
}

static void ImGuiManagerDraw(ImGuiManager *imgui_manager) {
  // static bool draw_demo = true;
  // ImGui::ShowDemoWindow(&draw_demo);

  static bool opt_fullscreen = true;
  static bool opt_padding = false;
  static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

  // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window
  // not dockable into, because it would be confusing to have two docking
  // targets within each others.
  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
  if (opt_fullscreen) {
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |=
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
  } else {
    dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
  }

  // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render
  // our background and handle the pass-thru hole, so we ask Begin() to not
  // render a background.
  if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
    window_flags |= ImGuiWindowFlags_NoBackground;

  // Important: note that we proceed even if Begin() returns false (aka window
  // is collapsed). This is because we want to keep our DockSpace() active. If a
  // DockSpace() is inactive, all active windows docked into it will lose their
  // parent and become undocked. We cannot preserve the docking relationship
  // between an active window and an inactive docking, otherwise any change of
  // dockspace/settings would lead to windows being stuck in limbo and never
  // being visible.
  if (!opt_padding)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin("DockSpace Demo", nullptr, window_flags);
  if (!opt_padding)
    ImGui::PopStyleVar();

  if (opt_fullscreen)
    ImGui::PopStyleVar(2);

  // Submit the DockSpace
  ImGuiIO &io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
  }

  // if (ImGui::BeginMenuBar()) {
  //   if (ImGui::BeginMenu("Options")) {
  //     // Disabling fullscreen would allow the window to be moved to the front
  //     of
  //     // other windows, which we can't undo at the moment without finer
  //     window
  //     // depth/z control.
  //     ImGui::MenuItem("Fullscreen", NULL, &opt_fullscreen);
  //     ImGui::MenuItem("Padding", NULL, &opt_padding);
  //     ImGui::Separator();

  //     if (ImGui::MenuItem("Flag: NoSplit", "",
  //                         (dockspace_flags & ImGuiDockNodeFlags_NoSplit) !=
  //                             0)) {
  //       dockspace_flags ^= ImGuiDockNodeFlags_NoSplit;
  //     }
  //     if (ImGui::MenuItem("Flag: NoResize", "",
  //                         (dockspace_flags & ImGuiDockNodeFlags_NoResize) !=
  //                             0)) {
  //       dockspace_flags ^= ImGuiDockNodeFlags_NoResize;
  //     }
  //     if (ImGui::MenuItem("Flag: NoDockingInCentralNode", "",
  //                         (dockspace_flags &
  //                          ImGuiDockNodeFlags_NoDockingInCentralNode) != 0))
  //                          {
  //       dockspace_flags ^= ImGuiDockNodeFlags_NoDockingInCentralNode;
  //     }
  //     if (ImGui::MenuItem(
  //             "Flag: AutoHideTabBar", "",
  //             (dockspace_flags & ImGuiDockNodeFlags_AutoHideTabBar) != 0)) {
  //       dockspace_flags ^= ImGuiDockNodeFlags_AutoHideTabBar;
  //     }
  //     if (ImGui::MenuItem(
  //             "Flag: PassthruCentralNode", "",
  //             (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) !=
  //             0, opt_fullscreen)) {
  //       dockspace_flags ^= ImGuiDockNodeFlags_PassthruCentralNode;
  //     }
  //     ImGui::Separator();

  //     ImGui::EndMenu();
  //   }

  //   ImGui::EndMenuBar();
  // }

  ImGuiDrawCode(imgui_manager);
  ImGuiLogDraw(&Global_ImGuiLog);
  ImGuiDrawRegisters(imgui_manager);
  ImGuiDrawLocalVariables(imgui_manager);

  ImGui::End();
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