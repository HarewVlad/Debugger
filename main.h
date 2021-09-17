#include <Windows.h>
#include <dbghelp.h>
#include <psapi.h>
#include <strsafe.h>
#include <tchar.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>
#include <iomanip>

#define BUFSIZE 512

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "DbgHelp.lib")

#define LOG(TYPE) std::cout << #TYPE << ": "

#include "debugger.h"