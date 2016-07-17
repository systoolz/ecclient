#ifndef __SYSTOOLX_H
#define __SYSTOOLX_H

#define STR_SIZE(x) (lstrlen(x) + 1) * sizeof(TCHAR)
#define STR_ALLOC(x) (TCHAR *) GetMem((x + 1) * sizeof(TCHAR))
#define STR_BYTES(x) ((x) * sizeof(TCHAR))
#define STR_CHARS(x) (((x) / sizeof(TCHAR)) + ((x) % sizeof(TCHAR)))

void FreeMem(void *block);
void *GetMem(DWORD dwSize);
void *GrowMem(void *block, DWORD dwSize);

TCHAR *StDup(TCHAR *str);
void StTrim(TCHAR *str);
TCHAR *LangLoadString(UINT sid);

TCHAR *GetWndText(HWND wnd);

#endif
