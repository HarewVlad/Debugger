#include <Windows.h>
#include <dbghelp.h>
#include <psapi.h>
#include <strsafe.h>
#include <tchar.h>
#include <iostream>
#include <string>

#define BUFSIZE 512

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "DbgHelp.lib")

#define LOG(TYPE) std::cout << #TYPE << ": "

struct Debugger {
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
};

Debugger *CreateDebugger(const std::string &process_name) {
  Debugger *result = new Debugger;

  STARTUPINFO si = {};
  PROCESS_INFORMATION pi = {};
  if (!CreateProcessA(process_name.c_str(), NULL, NULL, NULL, FALSE,
                      DEBUG_ONLY_THIS_PROCESS, NULL, NULL, &si, &pi)) {
    LOG(CreateDebugger) << "Unable to create debugge process\n";
    delete result;
    return nullptr;
  }

  if (!SymInitialize(pi.hProcess, NULL, false)) {
    LOG(CreateDebugger) << "Unable to initialize debug symbols!\n";
    delete result;
    return nullptr;
  }

  result->si = si;
  result->pi = pi;

  return result;
}

inline DWORD DebuggerGetTargetStartAddress(HANDLE process, HANDLE thread) {
  DWORD result = 0;

  SYMBOL_INFO *symbol;
  symbol = (SYMBOL_INFO *)new BYTE[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  symbol->MaxNameLen = MAX_SYM_NAME;
  SymFromName(process, "wWinMainCRTStartup", symbol);

  result = symbol->Address;

  delete[](BYTE *) symbol;

  return result;
}

// TODO: Move to utils.h / utils.cpp
BOOL GetFileNameFromHandle(HANDLE hFile, TCHAR *pszFilename) {
  BOOL bSuccess = FALSE;
  HANDLE hFileMap;

  // Get the file size.
  DWORD dwFileSizeHi = 0;
  DWORD dwFileSizeLo = GetFileSize(hFile, &dwFileSizeHi);

  if (dwFileSizeLo == 0 && dwFileSizeHi == 0) {
    _tprintf(TEXT("Cannot map a file with a length of zero.\n"));
    return FALSE;
  }

  // Create a file mapping object.
  hFileMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 1, NULL);

  if (hFileMap) {
    // Create a file mapping to get the file name.
    void *pMem = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 1);

    if (pMem) {
      if (GetMappedFileName(GetCurrentProcess(), pMem, pszFilename, MAX_PATH)) {
        // Translate path with device name to drive letters.
        TCHAR szTemp[BUFSIZE];
        szTemp[0] = '\0';

        if (GetLogicalDriveStrings(BUFSIZE - 1, szTemp)) {
          TCHAR szName[MAX_PATH];
          TCHAR szDrive[3] = TEXT(" :");
          BOOL bFound = FALSE;
          TCHAR *p = szTemp;

          do {
            // Copy the drive letter to the template string
            *szDrive = *p;

            // Look up each device name
            if (QueryDosDevice(szDrive, szName, MAX_PATH)) {
              size_t uNameLen = _tcslen(szName);

              if (uNameLen < MAX_PATH) {
                bFound = _tcsnicmp(pszFilename, szName, uNameLen) == 0 &&
                         *(pszFilename + uNameLen) == _T('\\');

                if (bFound) {
                  // Reconstruct pszFilename using szTempFile
                  // Replace device path with DOS path
                  TCHAR szTempFile[MAX_PATH];
                  StringCchPrintf(szTempFile, MAX_PATH, TEXT("%s%s"), szDrive,
                                  pszFilename + uNameLen);
                  StringCchCopyN(pszFilename, MAX_PATH + 1, szTempFile,
                                 _tcslen(szTempFile));
                }
              }
            }

            // Go to the next NULL character.
            while (*p++)
              ;
          } while (!bFound && *p);  // end of string
        }
      }
      bSuccess = TRUE;
      UnmapViewOfFile(pMem);
    }

    CloseHandle(hFileMap);
  }
  return (bSuccess);
}

inline bool DebuggerLoadTargetModules(Debugger *debugger, HANDLE file,
                                HANDLE process, DWORD64 base_address) {
  TCHAR filename[MAX_PATH + 1];
  if (!GetFileNameFromHandle(file, filename)) {
    LOG(DebuggerProcessEvent) << "Unable to get file name from handle!\n";
    return false;
  }

  DWORD64 base = SymLoadModuleEx(process, NULL, filename, NULL,
                                 base_address, 0, NULL, NULL);
  if (!base) {
    LOG(DebuggerProcessEvent) << "Unable to get module base address!\n";
    return false;
  }

  IMAGEHLP_MODULE64 module_info;
  module_info.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
  if (SymGetModuleInfo64(process, base, &module_info)) {
    LOG(INFO) << "Loaded DLL " << filename << ", at address " << std::hex
                << base_address << (module_info.SymType == SymPdb ? ", symbols loaded \n" : "\n");
  } else {
  	LOG(ERROR) << "Unable to load " << filename << '\n';
  }
}

bool DebuggerSetBreakpoint(Debugger *debugger, DWORD address) {
	
}

bool DebuggerProcessEvent(Debugger *debugger, DEBUG_EVENT debug_event,
                          DWORD &continue_status) {
  PROCESS_INFORMATION pi = debugger->pi;
  DWORD debug_event_code = debug_event.dwDebugEventCode;

  switch (debug_event_code) {
    case LOAD_DLL_DEBUG_EVENT: {
      const LOAD_DLL_DEBUG_INFO load_dll_debug_info = debug_event.u.LoadDll;

      HANDLE file = load_dll_debug_info.hFile;
      HANDLE process = pi.hProcess;
      DWORD64 base_of_dll = (DWORD64)load_dll_debug_info.lpBaseOfDll;
      DebuggerLoadTargetModules(debugger, file, process, base_of_dll);
    } break;
    case CREATE_PROCESS_DEBUG_EVENT: {
      const CREATE_PROCESS_DEBUG_INFO create_process_debug_info =
          debug_event.u.CreateProcessInfo;

      HANDLE file = create_process_debug_info.hFile;
      HANDLE process = pi.hProcess;
      DWORD64 base_of_image = (DWORD64)create_process_debug_info.lpBaseOfImage;
      DebuggerLoadTargetModules(debugger, file, process, base_of_image);

      // Replace first instruction with int3
      DWORD start_address = DebuggerGetTargetStartAddress(pi.hProcess, pi.hThread);
      BYTE instruction;
      DWORD read_bytes;

      if (!ReadProcessMemory(pi.hProcess, (void *)start_address, &instruction, 1, &read_bytes)) {
      	LOG(DebuggerProcessEvent) << "Unable to read from process memory!\n";
      }

      continue_status = DBG_CONTINUE;
    } break;
    case OUTPUT_DEBUG_STRING_EVENT: {
      const OUTPUT_DEBUG_STRING_INFO output_debug_string_info =
          debug_event.u.DebugString;
      WORD size = output_debug_string_info.nDebugStringLength;
      LPSTR data = output_debug_string_info.lpDebugStringData;
      WCHAR *message = new WCHAR[size];

      if (!ReadProcessMemory(pi.hProcess, data, message, size, NULL)) {
        LOG(DebuggerProcessEvent) << "Unable to read from process memory!\n";
        return false;
      }

      if (!output_debug_string_info.fUnicode) {
        LOG(OUTPUT_DEBUG_STRING_EVENT) << (char *)message << '\n';
      } else {
        // LOG(OUTPUT_DEBUG_STRING_EVENT, message); // Output to console is not
        // supported for now (cause i'm dumb) :(
      }

      continue_status = DBG_CONTINUE;
    } break;
    case EXCEPTION_DEBUG_EVENT: {
      const EXCEPTION_DEBUG_INFO &exception_debug_info =
          debug_event.u.Exception;
      DWORD exception_code = exception_debug_info.ExceptionRecord.ExceptionCode;
      switch (exception_code) {
        case STATUS_BREAKPOINT: {
          static bool ignore_breakpoint = true;
          if (ignore_breakpoint) {
            ignore_breakpoint = false;
          } else {
            LOG(STATUS_BREAKPOINT) << "I'm breakpoint!\n";
          }

          continue_status = DBG_CONTINUE;
        } break;
        default:
          if (exception_debug_info.dwFirstChance == 1) {
          }

          continue_status = DBG_EXCEPTION_NOT_HANDLED;
          break;
      }
    } break;
    default:
      continue_status = DBG_EXCEPTION_NOT_HANDLED;
      return true;
  }

  return true;
}

bool DebuggerRun(Debugger *debugger) {
  DEBUG_EVENT debug_event = {};
  while (true) {
    if (!WaitForDebugEvent(&debug_event, INFINITE)) return 1;

    DWORD continue_status;
    DebuggerProcessEvent(debugger, debug_event, continue_status);

    ContinueDebugEvent(debug_event.dwProcessId, debug_event.dwThreadId,
                       continue_status);
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    LOG(INFO) <<  "Specify process name\n";
    return 1;
  }

  Debugger *debugger = CreateDebugger(argv[1]);

  DebuggerRun(debugger);
}