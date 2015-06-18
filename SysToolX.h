#ifndef __SYSTOOLX_H
#define __SYSTOOLX_H

#if __GNUC__ == 3
#define memcpy __builtin_memcpy
#define memmove memcpy
#define memset __builtin_memset
#endif

#define STR_SIZE(x) (lstrlen(x) + 1) * sizeof(TCHAR)
#define STR_ALLOC(x) (TCHAR *) GetMem((x + 1) * sizeof(TCHAR))
#define STR_BYTES(x) ((x) * sizeof(TCHAR))
#define STR_CHARS(x) (((x) / sizeof(TCHAR)) + ((x) % sizeof(TCHAR)))

void *GrowMem(void *lpPtr, DWORD dwSize);
void *GetMem(DWORD dwSize);
BOOL FreeMem(void *lpPtr);

TCHAR *StDup(TCHAR *str);
void StTrim(TCHAR *str);
TCHAR *LangLoadString(UINT sid);

TCHAR *GetWndText(HWND wnd);

#endif
