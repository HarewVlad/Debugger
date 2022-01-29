#include <Windows.h>
#include <dbghelp.h>
#include <psapi.h>
#include <strsafe.h>
#include <tchar.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <map>
#include <sstream>
#include <iomanip>
#include <thread>
#include <functional>
#include <assert.h>
#include <fstream>
#include <vector>
#include <set>
#include <sstream>
#include <initializer_list>

#define BUFSIZE 512

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "DbgHelp.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "D3D11.lib")

static bool Global_IsOpen = true;

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include <d3d11.h>

#include "directx11.h"
#include "registers.h"
#include "local_variable.h"
#include "breakpoint.h"
#include "debugger.h"
#include "source.h"
#include "imgui_manager.h"

static ImGuiLog Global_ImGuiLog;

#define LOG(TYPE) std::cout << #TYPE << ": "