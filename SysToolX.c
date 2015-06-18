#include <windows.h>
#include "SysToolX.h"

/* == MEMORY ROUTINES ====================================================== */

void *GrowMem(void *lpPtr, DWORD dwSize) {
  if (lpPtr == NULL) {
    return(HeapAlloc(GetProcessHeap(), HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY, dwSize));
  } else {
    return(HeapReAlloc(GetProcessHeap(), HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY, lpPtr, dwSize));
  }
}

void *GetMem(DWORD dwSize) {
  return(GrowMem(NULL, dwSize));
}

BOOL FreeMem(void *lpPtr) {
  return(HeapFree(GetProcessHeap(), HEAP_NO_SERIALIZE, lpPtr));
}

/* == STRING ROUTINES ====================================================== */

TCHAR *StDup(TCHAR *str) {
TCHAR *result;
  result = NULL;
  if (str != NULL) {
    result = STR_ALLOC(lstrlen(str));
    if (result != NULL) {
      lstrcpy(result, str);
    }
  }
  return(result);
}

void StTrim(TCHAR *str) {
TCHAR *s;
  if (str != NULL) {
    // trim start
    for (s = str; *s == TEXT(' '); s++);
    MoveMemory(str, s, STR_SIZE(s));
    // trim end
    if (*s != 0) {
      for (s += lstrlen(s) - 1; *s == TEXT(' '); s--);
      s++;
      *s = 0;
    }
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
  GetWindowText(wnd, result, sz + 1);
  result[sz] = 0;
  return(result);
}
