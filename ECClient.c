#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include "SysToolX.h"
#include "NetUnit.h"
#include "resource/ECClient.h"

// NFO: https://billing.novotelecom.ru/billing/user/widget/api/
// OLD: https://billing.novotelecom.ru/billing/user/api/?method=userInfo&login=$$$&password=###
// NEW: https://api.novotelecom.ru/billing/?method=userInfo&login=$$$&passwordHash=###
// https://billing.novotelecom.ru/billing/user/data/widget/windows7/Novotelecom.gadget
// "https://api.novotelecom.ru/billing/?method=userInfo&login="+login+"&passwordHash="+password+"&clientVersion=2"

static TCHAR strURLGetInfo[] = TEXT("https://api.novotelecom.ru/billing/?method=userInfo&login=%s&passwordHash=%s&clientVersion=2");
static TCHAR strURLTheSite[] = TEXT("http://systools.losthost.org/?misc#ecclient");
static TCHAR strURLBilling[] = TEXT("https://billing.novotelecom.ru");
static TCHAR strTheVersion[] = TEXT("ECClient v2.3");
// filepath must start with ".\\" (current folder)
// or it was moved in the %SystemRoot% (C:\Windows) by default
static TCHAR strConfigFile[] = TEXT(".\\ECClient.ini");
static TCHAR strConfigMain[] = TEXT("General");
static TCHAR strConfigCode[] = TEXT("username");
static TCHAR strConfigPass[] = TEXT("passhash");
static TCHAR strEmptyValue[] = TEXT("");

static DWORD dwip = 0;
static UINT WM_TASKBARCREATED = 0;
static NOTIFYICONDATA nid;

#define ID_TRAY_APP_ICON 5000
#define WM_TRAYICON WM_USER + 1

static TCHAR pass[33];
static TCHAR code[15];
#define CONFIG_OK (code[0] && pass[0])

int MsgBox(HWND wnd, TCHAR *lpText, UINT uType) {
int result;
TCHAR *s;
  s = NULL;
  if (!HIWORD(lpText)) {
    s = LangLoadString(LOWORD(lpText));
    if (!s) { s = StDup(strEmptyValue); }
  }
  result = MessageBox(wnd, s ? s : lpText, strTheVersion, uType);
  if (s) { FreeMem(s); }
  return(result);
}

void ValidateString(TCHAR *st, BOOL bhex) {
int i;
  StTrim(st);
  for (i = 0; st[i]; i++) {
    // a lot of warnings goes here!
    if ((st[i] >= TEXT('0')) && (st[i] <= TEXT('9'))) { continue; }
    if (bhex) {
      pass[i] = (TCHAR) (DWORD) CharLower(MAKEINTRESOURCE(st[i]));
      if ((st[i] >= TEXT('a')) && (st[i] <= TEXT('f'))) { continue; }
    }
    // fallback through all - invalid character
    st[0] = 0;
    break;
  }
}

void ValidateSettings(void) {
  ValidateString(code, FALSE);
  ValidateString(pass, TRUE);
  // something wrong - invalid number or pass or pass less then 32 characters
  if ((!code[0]) || (!pass[0]) || (lstrlen(pass) != 32)) {
    code[0] = 0;
    pass[0] = 0;
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
  code[0] = 0;
  pass[0] = 0;
  GetPrivateProfileString(strConfigMain, strConfigCode, strEmptyValue, code, 15, strConfigFile);
  GetPrivateProfileString(strConfigMain, strConfigPass, strEmptyValue, pass, 33, strConfigFile);
  ValidateSettings();
}

BOOL CALLBACK DlgPrc(HWND wnd, UINT umsg, WPARAM wparm, LPARAM lparm) {
TCHAR *s, *h;
  switch (umsg) {

    case WM_INITDIALOG:
      // set icons
      SendMessage(wnd, WM_SETICON,   ICON_BIG, GetClassLong((HWND) lparm, GCL_HICON));
      SendMessage(wnd, WM_SETICON, ICON_SMALL, GetClassLong((HWND) lparm, GCL_HICON));
      // need to return TRUE to set focus
      return(TRUE);
      break;

    case WM_COMMAND:
      if (wparm == MAKELONG(IDCANCEL, BN_CLICKED)) {
        EndDialog(wnd, 0);
      }
      if (wparm == MAKELONG(IDOK, BN_CLICKED)) {
        code[0] = 0;
        pass[0] = 0;
        GetDlgItemText(wnd, IDC_DLG_USERCODE, code, 15);
        s = GetWndText(GetDlgItem(wnd, IDC_DLG_USERPASS));
        if (s) {
          if (s[0]) {
            h = CalcMD5Hash((BYTE *) s, lstrlen(s));
            if (h) {
              lstrcpyn(pass, h, 33);
              FreeMem(h);
            }
            // secure
            ZeroMemory(s, STR_BYTES(lstrlen(s)));
          }
          FreeMem(s);
        }
        EndDialog(wnd, (IsDlgButtonChecked(wnd, IDC_DLG_SAVEINFO) == BST_CHECKED) ? 2 : 1);
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
    while (fmt[i] && (i < 1024)) {
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
            lstrcpyn(&buf[j], x, 1024 - j);
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

int ShowBalance(HWND wnd) {
TCHAR *fmt, *buf, *page, *err;
DWORD bsz, result;
  fmt = LangLoadString(IDS_TEXT_FORMAT);
  buf = STR_ALLOC(1024);
  result = 3;
  if (fmt && buf) {
    result = 1;
    ValidateSettings();
    if (CONFIG_OK) {
      result = 2;
      bsz = 0;
      wsprintf(buf, strURLGetInfo, code, pass);
      page = (TCHAR *) HTTPSGetContent(buf, &bsz);
#ifdef UNICODE
      so you want an unicode build huh
      well you can get it just dont forget to
      convert page from ANSI to UNICODE
      I dont do it cause it is ANSI build
      and I didnt test UNICODE but should work fine
#endif
      if (page) {
        err = XMLGetValue(TEXT("errorCode"), page);
        if (err) {
          result = 1;
          // check for right login/password
          if (err[0] == TEXT('0') && (!err[1])) {
            buf[0] = 0;
            FillBalanceTemplate(buf, fmt, page);
            MsgBox(wnd, buf, MB_OK | MB_ICONINFORMATION);
            result = 0;
          }
          FreeMem(err);
        }
        FreeMem(page);
      } else {
        // v2.3
        result += bsz;
      }
    }
  }
  if (buf) { FreeMem(buf); }
  if (fmt) { FreeMem(fmt); }
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
  ZeroMemory(code, STR_BYTES(15));
  ZeroMemory(pass, STR_BYTES(33));
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
  nid.hIcon  = (HICON) GetClassLong(wnd, GCL_HICON);
  dwip = GetExternalIPAddr(&strURLBilling[8]);
  UpdateNetworkInfo();
}

LRESULT CALLBACK WndProc(HWND wnd, UINT umsg, WPARAM wparm, LPARAM lparm) {
POINT pt;
  switch (umsg) {

    case WM_CREATE:
      InitNotifyIconData(wnd);
      Shell_NotifyIcon(NIM_ADD, &nid);
      break;

    case WM_TRAYICON:
      // mouseover event - update info
      if (wparm == ID_TRAY_APP_ICON) {
        UpdateNetworkInfo();
        Shell_NotifyIcon(NIM_MODIFY, &nid);
      }
      // redirect this message to the menu handler below
      // so ShowUserBalance() was not called in different places
      if (lparm == WM_LBUTTONDBLCLK) {
        PostMessage(wnd, WM_COMMAND, MAKELONG(IDI_TRAY_CONTEXT_MENU_INFO, 0), 0);
      }
      // show popup menu
      if (lparm == WM_RBUTTONDOWN) {
        SetForegroundWindow(wnd);
        GetCursorPos(&pt);
        TrackPopupMenu(GetSubMenu(GetMenu(wnd), 0), 0, pt.x, pt.y, 0, wnd, NULL);
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
          // if you don't do Co(Un)Initialize - ShellExecute() won't work
          // Windows NT 5.x+ restriction
          CoInitialize(NULL);
          ShellExecute(0, TEXT("open"),
            (LOWORD(wparm) == IDI_TRAY_CONTEXT_MENU_SITE) ? strURLBilling : strURLTheSite,
            NULL, NULL, SW_SHOWNORMAL);
          CoUninitialize();
          break;
        case IDI_TRAY_CONTEXT_MENU_INFO:
          ShowUserBalance(wnd);
          break;
      }
      break;

    case WM_DESTROY:
      Shell_NotifyIcon(NIM_DELETE, &nid);
      PostQuitMessage(0);
      return(0);
      break;

  }
  // Explorer.exe dies and rises like a Phoenix from the ashes again
  // BTW, you can't move this code to above switch(), cause switch() works only with constants
  if (umsg == WM_TASKBARCREATED) {
    Shell_NotifyIcon(NIM_ADD, &nid);
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
  WM_TASKBARCREATED = RegisterWindowMessage(TEXT("TaskbarCreated"));

  // http://msdn.microsoft.com/en-US/library/vstudio/bb384843.aspx
  ZeroMemory(&wcex, sizeof(wcex));
  wcex.cbSize        = sizeof(wcex);
  wcex.lpszClassName = TEXT("ECClientWndClass");
  wcex.hInstance     = GetModuleHandle(NULL);
  wcex.lpfnWndProc   = &WndProc;
  wcex.style         = CS_HREDRAW | CS_VREDRAW;
  wcex.hIcon         = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(1));
  wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
  wcex.lpszMenuName  = MAKEINTRESOURCE(IDM_TRAYMENU);

  // only one instance allowed
  hw = FindWindow(wcex.lpszClassName, NULL);
  if (hw) {
    SendMessage(hw, WM_CLOSE, 0, 0);
  }

  // class registration failed
  if (!RegisterClassEx(&wcex)) {
    ExitProcess(2);
  }

  // create window
  CreateWindowEx(
    0, wcex.lpszClassName, wcex.lpszClassName,
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX
//    | WS_VISIBLE
    ,
    (GetSystemMetrics(SM_CXSCREEN) - 320) / 2,
    (GetSystemMetrics(SM_CYSCREEN) - 240) / 2,
    320, 240,
    0, 0,
    wcex.hInstance, NULL
  );

  while (GetMessage(&wmsg, 0, 0, 0)) {
    TranslateMessage(&wmsg);
    DispatchMessage(&wmsg);
  }

  UnregisterClass(wcex.lpszClassName, wcex.hInstance);

  ExitProcess(0);
  return(0);
}
