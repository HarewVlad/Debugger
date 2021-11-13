extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;

  switch (msg) {
    case WM_SIZE:
      // if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
      // {
      //     CleanupRenderTarget();
      //     g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam),
      //     (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
      //     CreateRenderTarget();
      // }
      return 0;
    case WM_SYSCOMMAND:
      if ((wParam & 0xfff0) == SC_KEYMENU)  // Disable ALT application menu
        return 0;
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hWnd, msg, wParam, lParam);
}

inline bool Directx11CreateDeviceAndSwapChain(Directx11 *directx) {
  DXGI_SWAP_CHAIN_DESC sd = {};
  sd.BufferCount = 2;
  sd.BufferDesc.Width = 0;
  sd.BufferDesc.Height = 0;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = directx->window;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  UINT create_device_flags = 0;
  D3D_FEATURE_LEVEL feature_level;
  const D3D_FEATURE_LEVEL feature_levels[2] = {
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_0,
  };
  if (D3D11CreateDeviceAndSwapChain(
          NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, create_device_flags,
          feature_levels, 2, D3D11_SDK_VERSION, &sd, &directx->swap_chain,
          &directx->device, &feature_level, &directx->device_context) != S_OK)
    return false;

  return true;
}

inline bool Directx11CreateWindow(Directx11 *directx) {
  WNDCLASSEX wc = {sizeof(WNDCLASSEX),    CS_CLASSDC, WndProc, 0L,   0L,
                   GetModuleHandle(NULL), NULL,       NULL,    NULL, NULL,
                   _T("Debugger"),        NULL};
  RegisterClassEx(&wc);
  directx->window =
      CreateWindow(wc.lpszClassName, _T("Debugger"), WS_OVERLAPPEDWINDOW, 100,
                   100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

  ShowWindow(directx->window, SW_SHOWDEFAULT);
  UpdateWindow(directx->window);

  return true;
}

inline bool Directx11CreateRenderTarget(Directx11 *directx) {
  auto swap_chain = directx->swap_chain;
  auto device = directx->device;

  ID3D11Texture2D *back_buffer;
  swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
  device->CreateRenderTargetView(back_buffer, NULL,
                                 &directx->render_target_view);
  back_buffer->Release();

  return true;
}

static Directx11 *CreateDirectx11() {
  Directx11 *result = new Directx11;

  if (!Directx11CreateWindow(result) ||
      !Directx11CreateDeviceAndSwapChain(result) ||
      !Directx11CreateRenderTarget(result)) {
    free(result);
    return nullptr;
  }

  return result;
}

static bool Directx11RenderBegin(Directx11 *directx) {
  auto device_context = directx->device_context;
  auto render_target_view = directx->render_target_view;

  const ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
  const float clear_color_with_alpha[4] = {
      clear_color.x * clear_color.w, clear_color.y * clear_color.w,
      clear_color.z * clear_color.w, clear_color.w};
  device_context->OMSetRenderTargets(1, &render_target_view, NULL);
  device_context->ClearRenderTargetView(render_target_view,
                                        clear_color_with_alpha);

  return true;
}

static HRESULT Directx11RenderEnd(Directx11 *directx) {
  auto swap_chain = directx->swap_chain;

  return swap_chain->Present(1, 0);
}