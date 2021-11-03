extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    return true;

  switch (msg) {
  case WM_SIZE:
    // if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
    // {
    //     CleanupRenderTarget();
    //     g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam),
    //     (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0); CreateRenderTarget();
    // }
    return 0;
  case WM_SYSCOMMAND:
    if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
      return 0;
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(hWnd, msg, wParam, lParam);
}

inline HWND CreateWindowForInterface() {
  HWND result = NULL;

  WNDCLASSEX wc = {sizeof(WNDCLASSEX),    CS_CLASSDC, WndProc, 0L,   0L,
                   GetModuleHandle(NULL), NULL,       NULL,    NULL, NULL,
                   _T("Debugger"),        NULL};
  RegisterClassEx(&wc);
  result = CreateWindow(wc.lpszClassName, _T("Debugger"), WS_OVERLAPPEDWINDOW,
                        100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

  return result;
}

static UserInterface CreateUserInterface() {
  UserInterface result;

  result.window = CreateWindowForInterface();

  return result;
}

static void UserInterfaceRun(UserInterface *user_interface) {
  // std::string input = {};
  // while (true) {
  //   std::getline(std::cin, input);

  //   if (input == "aight") {
  //     break;
  //   } else if (input == "step_over") {
  //     if (user_interface->step_over_callback) {
  //       user_interface->step_over_callback();
  //     }
  //   } else if (input == "callstack") {
  //     if (user_interface->print_callstack_callback) {
  //       user_interface->print_callstack_callback();
  //     }
  //   } else if (input == "registers") {
  //     if (user_interface->print_registers_callback) {
  //       user_interface->print_registers_callback();
  //     }
  //   } else if (input == "set_breakpoint") {
  //     std::string filename = {};
  //     DWORD line;

  //     std::cout << "Enter filename: ";
  //     std::cin >> filename;
  //     std::cout << "Enter line: ";
  //     std::cin >> line;

  //     user_interface->set_breakpoint_callback(filename, line);
  //   }

  //   Sleep(100);
  // }
}