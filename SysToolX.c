#include <windows.h>
#include "SysToolX.h"

/* == MEMORY ROUTINES ====================================================== */

void FreeMem(void *block) {
  if (block) {
    LocalFree(block);
  }
}

void *GetMem(DWORD dwSize) {
  return((void *) LocalAlloc(LPTR, dwSize));
}

void *GrowMem(void *block, DWORD dwSize) {
  if (!dwSize) {
    if (block) {
      FreeMem(block);
    }
    block = NULL;
  } else {
    if (block) {
      block = (void *) LocalReAlloc(block, dwSize, LMEM_MOVEABLE | LMEM_ZEROINIT);
    } else {
      block = GetMem(dwSize);
    }
  }
  return(block);
}

/* == STRING ROUTINES ====================================================== */

TCHAR *StDup(TCHAR *str) {
TCHAR *result;
  result = NULL;
  if (str) {
    result = STR_ALLOC(lstrlen(str));
    if (result) {
      lstrcpy(result, str);
    }
  }
  return(result);
}

void StTrim(TCHAR *str) {
DWORD i, s, e;
  if (str) {
    s = 0;
    e = 0;
    for (i = 0; str[i]; i++) {
      if (str[i] != TEXT(' ')) {
        if ((!s) && (str[s] == TEXT(' '))) {
          s = i;
        }
        e = i + 1;
      }
      str[i - s] = str[i];
    }
    str[e - s] = 0;
  }
}

TCHAR *LangLoadString(UINT sid) {
TCHAR *res;
WORD *p;
HRSRC hr;
int i;
  res = NULL;
  hr = FindResource(NULL, MAKEINTRESOURCE(sid / 16 + 1), RT_STRING);
  p = hr ? (WORD *) LockResource(LoadResource(NULL, hr)) : NULL;
  if (p != NULL) {
    for (i = 0; i < (sid & 15); i++) {
      p += 1 + *p;
    }
    res = STR_ALLOC(*p);
    LoadString(NULL, sid, res, *p + 1);
  }
  return(res);
}

/* == WINDOWS ROUTINES ===================================================== */

TCHAR *GetWndText(HWND wnd) {
TCHAR *result;
int sz;
  sz = GetWindowTextLength(wnd);
  result = STR_ALLOC(sz);
  if (result) {
    GetWindowText(wnd, result, sz + 1);
    result[sz] = 0;
  }
  return(result);
}
