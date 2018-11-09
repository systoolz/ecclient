#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include "SysToolX.h"
#include "NetUnit.h"
#include "resource/ECClient.h"

/*
  https://api.novotelecom.ru/user/v1/login?userName=$$$&password=##
    $$$ - contractId
    ### = rawurlencode(base64(sha1bin(password)))

  https://api.novotelecom.ru/user/v1/getDynamicInfo?token=@@@
    @@@ - token value returned by /login method above
*/

// path must starts with the ".\\" (current folder)
// or %SystemRoot% (C:\Windows) will be used as default path
static TCHAR strConfigFile[] = TEXT(".\\ECClient.ini");
static TCHAR strConfigMain[] = TEXT("General");
static TCHAR strConfigCode[] = TEXT("username");
static TCHAR strConfigPass[] = TEXT("passhash");
static TCHAR strTaskbarCreated[] = TEXT("TaskbarCreated");
static TCHAR strECCWindowClass[] = TEXT("ECClientWndClass");

static DWORD dwip;
static UINT WM_TASKBARCREATED;
static NOTIFYICONDATA nid;

#define ID_TRAY_APP_ICON 5000
#define WM_TRAYICON (WM_USER + 1)

#define CODE_LEN 15
#define PASS_LEN 85
static TCHAR code[CODE_LEN];
static TCHAR pass[PASS_LEN];
#define CONFIG_OK (*code && *pass)

void ValidateString(TCHAR *st, BOOL bpass) {
DWORD i;
  StTrim(st);
  for (i = 0; st[i]; i++) {
    if ((st[i] >= TEXT('0')) && (st[i] <= TEXT('9'))) { continue; }
    if (bpass) {
      if ((st[i] >= TEXT('A')) && (st[i] <= TEXT('Z'))) { continue; }
      if ((st[i] >= TEXT('a')) && (st[i] <= TEXT('z'))) { continue; }
      // characters allowed in base64ed and rawurlencoded string
      if (st[i] == TEXT('%')) { continue; }
      if (st[i] == TEXT('-')) { continue; }
      if (st[i] == TEXT('.')) { continue; }
      if (st[i] == TEXT('_')) { continue; }
      if (st[i] == TEXT('~')) { continue; }
    }
    // fallback through all - invalid character
    *st = 0;
    break;
  }
}

void ValidateSettings(void) {
  ValidateString(code, FALSE);
  ValidateString(pass, TRUE);
  // something wrong - invalid number or pass or pass less then 32 characters
  if ((!*code) || (!*pass)) {
    *code = 0;
    *pass = 0;
  }
}

void SaveConfigSettings(void) {
  ValidateSettings();
  if (CONFIG_OK) {
    WritePrivateProfileString(strConfigMain, strConfigCode, code, strConfigFile);
    WritePrivateProfileString(strConfigMain, strConfigPass, pass, strConfigFile);
  }
}

void ReadConfigSettings(void) {
TCHAR x;
  x = 0;
  *code = 0;
  *pass = 0;
  GetPrivateProfileString(strConfigMain, strConfigCode, (TCHAR *) &x, code, CODE_LEN, strConfigFile);
  GetPrivateProfileString(strConfigMain, strConfigPass, (TCHAR *) &x, pass, PASS_LEN, strConfigFile);
  ValidateSettings();
}

BOOL CALLBACK DlgPrc(HWND wnd, UINT umsg, WPARAM wparm, LPARAM lparm) {
TCHAR *s, b[1025];
DWORD d;
  switch (umsg) {

    case WM_INITDIALOG:
      // set icons
      SendMessage(wnd, WM_SETICON,   ICON_BIG, GetClassLong((HWND) lparm, GCL_HICON));
      SendMessage(wnd, WM_SETICON, ICON_SMALL, GetClassLong((HWND) lparm, GCL_HICONSM));
      // need to return TRUE to set focus
      return(TRUE);
      break;

    case WM_COMMAND:
      if (wparm == MAKELONG(IDCANCEL, BN_CLICKED)) {
        EndDialog(wnd, 0);
      }
      if (wparm == MAKELONG(IDOK, BN_CLICKED)) {
        *code = 0;
        *pass = 0;
        GetDlgItemText(wnd, IDC_DLG_USERCODE, code, CODE_LEN);
        StTrim(code);
        s = GetWndText(GetDlgItem(wnd, IDC_DLG_USERPASS));
        if (s) {
          // v2.6
          if ((!*code) || (!*s)) {
            d = 100;
          } else {
            d = GetEncodedPassword(s, pass);
          }
          // secure
          ZeroMemory(s, STR_BYTES(lstrlen(s)));
          FreeMem(s);
          // check
          if (*code && *pass && (!d)) {
            EndDialog(wnd, (IsDlgButtonChecked(wnd, IDC_DLG_SAVEINFO) == BST_CHECKED) ? 2 : 1);
          } else {
            *code = 0;
            *pass = 0;
            if (d == 100) {
              MsgBox(wnd, MAKEINTRESOURCE(IDS_ERROR_INPUT), MB_OK | MB_ICONERROR);
            } else {
              s = LangLoadString(IDS_ERROR_NOHSH);
              if (s) {
                wsprintf(b, s, d);
                FreeMem(s);
                MsgBox(wnd, b, MB_OK | MB_ICONERROR);
              }
            }
          }
          SetFocus(GetDlgItem(wnd, IDC_DLG_USERCODE));
        }
      }
      break;

  }
  return(FALSE);
}

void FillBalanceTemplate(TCHAR *buf, TCHAR *fmt, TCHAR *page) {
DWORD i, j, k;
TCHAR *x;
  // sanity check
  if (buf && fmt && page) {
    i = 0;
    j = 0;
    while ((i < 1025) && fmt[i]) {
      i++;
      if (fmt[i - 1] == TEXT('#')) {
        k = 0;
        while (fmt[i] && (fmt[i] != TEXT('$'))) {
          i++;
          k++;
        }
        if (fmt[i] == TEXT('$')) {
          fmt[i] = 0;
          x = XMLGetValue(&fmt[i - k], page);
          if (x) {
            lstrcpyn(&buf[j], x, 1025 - j);
            FreeMem(x);
            j = lstrlen(buf);
          }
          fmt[i] = TEXT('$');
        }
      } else {
        if (fmt[i - 1] != TEXT('$')) {
          buf[j] = fmt[i - 1];
          j++;
        }
      }
    }
    buf[j] = 0;
  }
}

// v2.6
void FromUTF8(TCHAR *s) {
WCHAR *t;
int sz;
  if (s && *s) {
    sz = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    // sz - size in characters including null terminator
    if (sz > 0) {
      t = (WCHAR *) GetMem(sz * sizeof(t[0]));
      if (t) {
        MultiByteToWideChar(CP_UTF8, 0, s, -1, t, sz);
        // can be converted inplace since UTF-8 string longer than ANSI
        WideCharToMultiByte(CP_ACP, 0, t, -1, s, lstrlen(s) + 1, NULL, NULL);
        FreeMem(t);
      }
    }
  }
}

DWORD ShowBalance(HWND wnd) {
TCHAR *fmt, *page, *s;
// wsprintf() has limit in 1024 characters in Windows 98
// and 1025 since Windows XP (NT4?, 2000?)
// because we have two formats - from buf[0] and buf[bp]
// doubling string buffer size should be enough for anything
TCHAR buf[1025 * 2];
DWORD bp, bsz, result;
  result = 1;
  ValidateSettings();
  if (CONFIG_OK) {
    *buf = 0;
    // load root part like
    fmt = LangLoadString(IDS_API_SITEROOT);
    if (fmt) {
      lstrcpyn(buf, fmt, 1025);
      FreeMem(fmt);
      bp = lstrlen(buf);
      // load token part
      fmt = LangLoadString(IDS_API_LOGINADR);
      if (fmt) {
        // merge
        wsprintf(&buf[bp], fmt, code, pass);
        FreeMem(fmt);
        #ifdef UNICODE
        so you want an unicode build huh
        well you can get it just dont forget to
        convert page from ANSI to UNICODE
        I dont do it cause it is ANSI build
        and I didnt test UNICODE but should work fine
        #endif
        result = 2;
        page = (TCHAR *) HTTPGetContent(buf, &bsz);
        if (page) {
          result = 1;
          s = XMLGetValue(TEXT("token"), page);
          FreeMem(page);
          if (s) {
            result = 2;
            // load info part
            fmt = LangLoadString(IDS_API_DINFOADR);
            if (fmt) {
              wsprintf(&buf[bp], fmt, s);
              FreeMem(fmt);
            }
            FreeMem(s);
            page = (TCHAR *) HTTPGetContent(buf, &bsz);
            if (page) {
              FromUTF8(page);
              *buf = 0;
              fmt = LangLoadString(IDS_TEXT_FORMAT);
              if (fmt) {
                FillBalanceTemplate(buf, fmt, page);
                FreeMem(fmt);
              }
              FreeMem(page);
              if (*buf) {
                MsgBox(wnd, buf, MB_OK | MB_ICONINFORMATION);
                result = 0;
              }
            }
          }
        } else {
          // v2.3
          result += bsz;
        }
      }
    }
  }
  return(result);
}

void ShowUserBalance(HWND wnd) {
int res;
  ReadConfigSettings();
  // invalid settings or no config file - prompt user
  res = 3;
  if (!CONFIG_OK) {
    res = DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_LOGINDLG), wnd, &DlgPrc, (LPARAM) wnd);
  }
  // res == 0 if clicked Cancel
  if (res) {
    switch (ShowBalance(wnd)) {
      case 0:
        // res == 2 - save settings
        if (res == 2) {
          SaveConfigSettings();
        }
        break;
      case 1:
        MsgBox(wnd, MAKEINTRESOURCE(IDS_ERROR_INPUT), MB_OK | MB_ICONERROR);
        break;
      case 2:
        MsgBox(wnd, MAKEINTRESOURCE(IDS_ERROR_NOSRV), MB_OK | MB_ICONERROR);
        break;
      // v2.3 - TLS not enabled
      case 3:
        MsgBox(wnd, MAKEINTRESOURCE(IDS_ERROR_NOTLS), MB_OK | MB_ICONERROR);
        break;
    }
  }
  // secure
  ZeroMemory(code, STR_BYTES(CODE_LEN));
  ZeroMemory(pass, STR_BYTES(PASS_LEN));
}

void UpdateNetworkInfo(void) {
TCHAR *s;
  s = GetNetStat(dwip);
  if (s) {
    lstrcpyn(nid.szTip, s, 64);
    FreeMem(s);
  }
}

void InitNotifyIconData(HWND wnd) {
  ZeroMemory(&nid, sizeof(nid));
  nid.cbSize = sizeof(nid);
  nid.hWnd   = wnd;
  nid.uID    = ID_TRAY_APP_ICON;
  nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  nid.uCallbackMessage = WM_TRAYICON;
  nid.hIcon  = (HICON) GetClassLong(wnd, GCL_HICONSM);
}

// v2.4
// PATCH: after working with the main program window
// it will be still visible for short time by Alt+Tab
void HideWnd(HWND wnd) {
  // v2.6
  SetWindowPos(wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
}

// v2.4
void DoNotifyIcon(DWORD dwMessage, PNOTIFYICONDATA lpdata) {
DWORD i, tk;
  // PATCH: sometimes icon didn't appears due to Windows timeout bug
  // http://www.geoffchappell.com/notes/windows/shell/missingicons.htm
  // 20*3 = 1 minute
  // if after one minute the icon still can't be created
  // it looks like something really wrong with the system
  // and so we must stop trying
  // NOTE: deleting icon may stuck as well!
  for (i = 0; i < 20; i++) {
    // you can't add more than one icon with the same uID
    // that's why no need to call NIM_DELETE in this loop
    tk = GetTickCount();
    Shell_NotifyIcon(dwMessage, lpdata);
    tk = GetTickCount() - tk;
    // "First is a timeout of 4 seconds, raised to 7 in Windows Vista."
    // (c) link above
    // 3 seconds should be enough
    if (tk <= 3000) {
      break;
    }
  }
}

LRESULT CALLBACK WndProc(HWND wnd, UINT umsg, WPARAM wparm, LPARAM lparm) {
POINT pt;
TCHAR *s;
  switch (umsg) {

    case WM_CREATE:
      // v2.6
      s = LangLoadString(IDS_PROGRAM_VER);
      if (s) {
        SetWindowText(wnd, s);
        FreeMem(s);
      }
      s = LangLoadString(IDS_LNK_STATPAGE);
      if (s) {
        dwip = GetExternalIPAddr(&s[8]);
        FreeMem(s);
      }
      InitNotifyIconData(wnd);
      UpdateNetworkInfo();
      DoNotifyIcon(NIM_ADD, &nid);
      // v2.6
      SetWindowLong(wnd, GWL_USERDATA, 0);
      break;

    case WM_TRAYICON:
      if (wparm == ID_TRAY_APP_ICON) {
        // mouseover event - update info
        UpdateNetworkInfo();
        DoNotifyIcon(NIM_MODIFY, &nid);
        // click
        switch (lparm) {
          case WM_LBUTTONDBLCLK:
            // redirect this message to the menu handler below
            // so ShowUserBalance() was not called in different places
            PostMessage(wnd, WM_COMMAND, MAKELONG(IDI_TRAY_CONTEXT_MENU_INFO, 0), 0);
            break;
          case WM_RBUTTONDOWN:
            // show popup menu
            GetCursorPos(&pt);
            SetForegroundWindow(wnd);
            TrackPopupMenu(GetSubMenu(GetMenu(wnd), 0), 0, pt.x, pt.y, 0, wnd, NULL);
            HideWnd(wnd); // v2.4
            break;
        }
      }
      // do not fallback this message to the system tray
      return(0);
      break;

    case WM_COMMAND:
      switch (LOWORD(wparm)) {
        case IDI_TRAY_CONTEXT_MENU_EXIT:
          PostMessage(wnd, WM_CLOSE, 0, 0);
          break;
        // open site
        case IDI_TRAY_CONTEXT_MENU_SITE:
        case IDI_TRAY_CONTEXT_MENU_ROOT:
          // v2.6
          s = LangLoadString((LOWORD(wparm) == IDI_TRAY_CONTEXT_MENU_SITE) ? IDS_LNK_STATPAGE : IDS_LNK_HOMEPAGE);
          if (s) {
            URLOpenLink(0, s);
            FreeMem(s);
          }
          break;
        case IDI_TRAY_CONTEXT_MENU_INFO:
          // v2.6 only one balance dialog
          SetForegroundWindow(wnd);
          if (!GetWindowLong(wnd, GWL_USERDATA)) {
            SetWindowLong(wnd, GWL_USERDATA, 1);
            ShowUserBalance(wnd);
            HideWnd(wnd); // v2.4
            SetWindowLong(wnd, GWL_USERDATA, 0);
          }
          break;
      }
      break;

    case WM_DESTROY:
      DoNotifyIcon(NIM_DELETE, &nid);
      PostQuitMessage(0);
      return(0);
      break;

  }
  // Explorer.exe dies and rises like a Phoenix from the ashes again
  // BTW, you can't move this code to above switch(), cause switch() works only with constants
  if (umsg == WM_TASKBARCREATED) {
    DoNotifyIcon(NIM_ADD, &nid);
  }
  return(DefWindowProc(wnd, umsg, wparm, lparm));
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
WNDCLASSEX wcex;
MSG wmsg;
HWND hw;
  // DialogBox*() won't work with manifest.xml in resources if this never called
  // or comctl32.dll not linked to the executable file
  // or not called DLLinit through LoadLibrary(TEXT("comctl32.dll"))
  InitCommonControls();

  // need this to restore tray icon when Explorer.exe crashes
  WM_TASKBARCREATED = RegisterWindowMessage(strTaskbarCreated);

  // http://msdn.microsoft.com/en-US/library/vstudio/bb384843.aspx
  ZeroMemory(&wcex, sizeof(wcex));
  wcex.cbSize        = sizeof(wcex);
  wcex.lpfnWndProc   = &WndProc;
  wcex.hInstance     = GetModuleHandle(NULL);
  wcex.lpszClassName = strECCWindowClass;
  wcex.lpszMenuName  = MAKEINTRESOURCE(IDM_TRAYMENU);
  wcex.hIcon         = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(1));
#ifdef SHOW_MAIN_WND
  wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
  wcex.style         = CS_HREDRAW | CS_VREDRAW;
#endif

  // only one instance allowed
  hw = FindWindow(wcex.lpszClassName, NULL);
  if (hw) {
    SendMessage(hw, WM_CLOSE, 0, 0);
  }

  // class registration failed
  if (!RegisterClassEx(&wcex)) {
    ExitProcess(1);
  }

  // create window
  hw = CreateWindowEx(
    0, wcex.lpszClassName, wcex.lpszClassName,
#ifdef SHOW_MAIN_WND
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
    (GetSystemMetrics(SM_CXSCREEN) - 320) / 2,
    (GetSystemMetrics(SM_CYSCREEN) - 240) / 2,
    320, 240,
#else
   0, 0, 0, 0, 0,
#endif
    0, 0,
    wcex.hInstance, NULL
  );

  // v2.6 window created
  if (hw) {
    while (GetMessage(&wmsg, 0, 0, 0)) {
      TranslateMessage(&wmsg);
      DispatchMessage(&wmsg);
    }
  }

  UnregisterClass(wcex.lpszClassName, wcex.hInstance);

  ExitProcess(0);
  return(0);
}
