static BOOL GetFileNameFromHandle(HANDLE hFile, TCHAR *pszFilename) {
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

// TODO: Fix DWORD64 dword -> DWORD dword
static DWORD64 GetStringDWORDHash(const std::string& string, DWORD64 dword) {
  DWORD64 string_hash = std::hash<std::string>{}(string);
  DWORD64 dword_hash = std::hash<DWORD>{}(dword);

  return string_hash ^ dword_hash;
}

static std::string GetFilenameFromPath(const std::string& path) {
  std::string result;

  if (!path.empty()) {
    const size_t last_slash_index = path.find_last_of("\\/");
    if (last_slash_index != std::string::npos) {
      result = path.substr(last_slash_index + 1, path.length());
    }
  }

  return result;
}