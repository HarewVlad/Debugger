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

#define BUFSIZE 512

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "DbgHelp.lib")

#define LOG(TYPE) std::cout << #TYPE << ": "

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include <d3d11.h>

#include "debugger.h"
#include "interface.h"