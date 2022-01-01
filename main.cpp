#include "main.h"
#include "imgui/imgui.cpp"
#include "imgui/imgui_demo.cpp"
#include "imgui/imgui_draw.cpp"
#include "imgui/imgui_impl_dx11.cpp"
#include "imgui/imgui_impl_win32.cpp"
#include "imgui/imgui_tables.cpp"
#include "imgui/imgui_widgets.cpp"

#include "utils.cpp"
#include "directx11.cpp"
#include "imgui_manager.cpp"
#include "debugger.cpp"

void Test() {
  // TestLogAll("Hello", "World", 123, "Damn");
  // LOG(SHIT) << "SFJSDF" << 123;
  // LOG_IMGUI(INFO, "Hello", 123);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    LOG(INFO) << "Specify process name\n";
    return 1;
  }

  // Test();
  if (!ImGuiLogInitialize(&Global_ImGuiLog)) {
    LOG(main) << "Unable to initialize imgui log\n";
    return 1;
  }

  HANDLE continue_event = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (!continue_event) {
    return 1;
  }

  Debugger debugger = CreateDebugger(argv[1], continue_event);
  ImGuiManager imgui_manager = CreateImGuiManager();
  debugger.OnLoadSourceFiles =
      [&](const std::unordered_map<std::string, std::vector<Line>>& source_filename_to_lines) {
        ImGuiManagerLoadSourceFile(&imgui_manager, source_filename_to_lines);
      };
  debugger.OnLineIndexChange = [&](const Line& line) {
    imgui_manager.current_line = line;
  };
  imgui_manager.OnStepOver = [&]() {
    DebuggerStepOver(&debugger);

    SetEvent(continue_event);
  };
  imgui_manager.OnPrintCallstack = [&]() { DebuggerPrintCallstack(&debugger); };
  imgui_manager.OnPrintRegisters = [&]() { DebuggerPrintRegisters(&debugger); };
  imgui_manager.OnSetBreakpoint = [&](DWORD64 hash) -> bool {
    return DebuggerSetBreakpoint(&debugger, hash);
  };
  imgui_manager.OnRemoveBreakpoint = [&](DWORD64 hash) -> bool {
    return DebuggerRemoveBreakpoint(&debugger, hash);
  };
  imgui_manager.OnContinue = [&]() { SetEvent(continue_event); };

  std::thread thread([&]() {
    Directx11 *directx = CreateDirectx11();

    ImGui_ImplWin32_Init(directx->window);
    ImGui_ImplDX11_Init(directx->device, directx->device_context);

    while (Global_IsOpen) {
      Directx11RenderBegin(directx);
      ImGuiManagerBeginDirectx11();
      ImGuiManagerDraw(&imgui_manager);
      ImGuiManagerEndDirectx11();

      if (FAILED(Directx11RenderEnd(directx)))
        Global_IsOpen = false;

      MSG msg;
      if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (msg.message == WM_QUIT)
          Global_IsOpen = false;
      }
    }
  });

  DebuggerRun(&debugger);
  thread.join();
}