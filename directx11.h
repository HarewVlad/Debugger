struct Directx11 {
  ID3D11Device* device;
  ID3D11DeviceContext* device_context;
  IDXGISwapChain* swap_chain;
  ID3D11RenderTargetView* render_target_view;
  HWND window;
};