#include "main.h"
#include "imgui/imgui.cpp"
#include "imgui/imgui_demo.cpp"
#include "imgui/imgui_draw.cpp"
#include "imgui/imgui_impl_dx11.cpp"
#include "imgui/imgui_impl_win32.cpp"
#include "imgui/imgui_tables.cpp"
#include "imgui/imgui_widgets.cpp"

#include "utils.cpp"
#include "registers.cpp"
#include "local_variable.cpp"
#include "breakpoint.cpp"
#include "directx11.cpp"
#include "debugger.cpp"
#include "source.cpp"
#include "imgui_manager.cpp"

void Test() {
  // TestLogAll("Hello", "World", 123, "Damn");
  // LOG(SHIT) << "SFJSDF" << 123;
  // LOG_IMGUI(INFO, "Hello", 123);
}

int main(int argc, char **argv) {
  if (argc < 3) {
    LOG(INFO) << "Usage: <executable filename with pdb>, <main function name>\n";
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

  Registers registers = {};
  LocalVariables local_variables;
  Source source;
  Breakpoints breakpoints;

  Debugger debugger = CreateDebugger(&registers, &local_variables, &source, &breakpoints, argv[1], argv[2], continue_event);
  ImGuiManager imgui_manager = CreateImGuiManager(&registers, &local_variables, &source, &breakpoints);
  debugger.OnLineAddressChange = [&](DWORD64 address) {
    imgui_manager.current_line_address = address;
  };
  imgui_manager.OnStepOver = [&]() {
    DebuggerSetState(&debugger, DebuggerState::STEP_OVER);
    SetEvent(continue_event);
  };
  imgui_manager.OnPrintCallstack = [&]() { DebuggerPrintCallstack(&debugger); };
  imgui_manager.OnSetBreakpoint = [&](DWORD64 address) -> bool {
    return DebuggerSetBreakpoint(&debugger, address);
  };
  imgui_manager.OnRemoveBreakpoint = [&](DWORD64 address) -> bool {
    return DebuggerRemoveBreakpoint(&debugger, address);
  };
  imgui_manager.OnContinue = [&]() {
    DebuggerSetState(&debugger, DebuggerState::CONTINUE);
    SetEvent(continue_event);
  };

  std::thread thread([&]() {
    Directx11 *directx = CreateDirectx11();

    ImGui_ImplWin32_Init(directx->window);
    ImGui_ImplDX11_Init(directx->device, directx->device_context);

    while (Global_IsOpen) {
      Directx11RenderBegin(directx);
      ImGuiManagerBeginDirectx11();
      ImGuiManagerDraw(&imgui_manager);
      ImGuiManagerUpdate(&imgui_manager);
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