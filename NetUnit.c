#include <windows.h>
#include <wincrypt.h>
#include <wininet.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include "SysToolX.h"

// very simple code and not optimized but who cares?..
TCHAR *XMLGetValue(TCHAR *name, TCHAR *data) {
TCHAR *result, *tmp;
int i, len;
  result = NULL;
  // sanity check
  if (name && data) {
    if (*name && *data) {
      tmp = StDup(name);
      if (tmp) {
        len = lstrlen(name) + 1;
        i = 0;
        while (data[i]) {
          i++;
          if (data[i - 1] == TEXT('<')) {
            lstrcpyn(tmp, &data[i], len);
            if ((!lstrcmpi(name, tmp)) && (data[i + len - 1] == TEXT('>'))) {
              i += len;
              len = 0;
              while (data[i] && (data[i] != TEXT('<'))) {
                i++;
                len++;
              }
              result = STR_ALLOC(len);
              if (result) {
                lstrcpyn(result, &data[i - len], len + 1);
              }
              break;
            }
          }
        }
        FreeMem(tmp);
      }
    }
  }
  return(result);
}

TCHAR *CalcMD5Hash(BYTE *buf, DWORD sz) {
HCRYPTPROV hProv;
HCRYPTHASH hHash;
TCHAR *result;
BYTE md5[16];
DWORD i;
  result = NULL;
  if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
    if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
      if (CryptHashData(hHash, buf, sz, 0)) {
        i = 16;
        if (CryptGetHashParam(hHash, HP_HASHVAL, md5, &i, 0)) {
          result = STR_ALLOC(32);
          if (result) {
            for (i = 0; i < 32; i++) {
              result[i] = (md5[i/2] >> (4*((i&1)^1))) & 0x0F;
              result[i] += (result[i] < 10) ? TEXT('0') : (TEXT('a') - 10);
            }
            result[32] = 0;
          }
        }
      }
      CryptDestroyHash(hHash);
    }
    CryptReleaseContext(hProv, 0);
  }
  return(result);
}

// it's not very optimized code but good for small internet pages
#define MAX_BLOCK_SIZE 512
BYTE *HTTPSGetContent(TCHAR *url, DWORD *len) {
HINTERNET hOpen, hConn, hReq;
URL_COMPONENTS uc;
BYTE *result, *buf;
DWORD sz;
  // init result
  result = NULL;
  *len = 0;
  // prepare URL string
  ZeroMemory(&uc, sizeof(uc));
  uc.dwStructSize = sizeof(uc);
  sz = lstrlen(url);
  uc.lpszHostName     = STR_ALLOC(sz);
  uc.dwHostNameLength = sz;
  uc.lpszUrlPath      = STR_ALLOC(sz);
  uc.dwUrlPathLength  = sz;
  sz = InternetCrackUrl(url, lstrlen(url), 0, &uc);
  // sanity check
  if (sz && (uc.nScheme == INTERNET_SCHEME_HTTPS) && uc.lpszHostName && uc.lpszUrlPath && uc.lpszHostName[0] && uc.lpszUrlPath[0]) {
    // open
    hOpen = InternetOpen(NULL, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hOpen) {
      // connect
      hConn = InternetConnect(hOpen, uc.lpszHostName, INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
      if (hConn) {
        // request
        hReq = HttpOpenRequest(hConn, NULL, uc.lpszUrlPath, TEXT("1.0"), NULL, NULL,
          INTERNET_FLAG_NO_AUTO_REDIRECT | INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_COOKIES |
          INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE |
          INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID, 0);
        if (hReq) {
          sz = HttpSendRequest(hReq, TEXT(
            // v2.2
            "Accept: */*\r\n"\
            "User-Agent: Mozilla/5.0 (compatible; MSIE 10.0; Windows NT 6.1; WOW64; Trident/6.0)\r\n"\
            "Connection: Close"
          ), (DWORD) -1, NULL, 0);
          // v2.3
          if (sz) {
            buf = GetMem(MAX_BLOCK_SIZE);
            do {
              sz = 0;
              InternetReadFile(hReq, buf, MAX_BLOCK_SIZE, &sz);
              if (sz) {
                result = GrowMem(result, *len + sz + 1);
                CopyMemory(&result[*len], buf, sz);
                *len += sz;
              }
            } while (sz);
            FreeMem(buf);
            // add 0 at the end for ANSI text buffer data
            if (result && *len) {
              result[*len] = 0;
            }
          } else {
            // v2.3
            *len = (GetLastError() == ERROR_INTERNET_CONNECTION_RESET) ? 1 : 0;
            // result == NULL, len == 1 - no TLS support (must be manually enabled)
          }
          InternetCloseHandle(hReq);
        }
        InternetCloseHandle(hConn);
      }
      InternetCloseHandle(hOpen);
    }
  }
  if (uc.lpszHostName) { FreeMem(uc.lpszHostName); }
  if (uc.lpszUrlPath)  { FreeMem(uc.lpszUrlPath);  }
  return(result);
}

DWORD GetHostIPAddr(TCHAR *host) {
WSADATA ws;
HOSTENT *hs;
DWORD result;
  result = INADDR_NONE;
  if (!WSAStartup(MAKEWORD(1, 1), &ws)) {
    result = inet_addr(host);
    if (result == INADDR_NONE) {
      hs = gethostbyname(host);
      if (hs) {
        result = *(DWORD *) hs->h_addr_list[0];
      }
    }
    WSACleanup();
  }
  return(result);
}

ULARGE_INTEGER GetInOutOctetsByIndex(DWORD dwIndex) {
MIB_IFTABLE *mit;
ULONG i, sz;
ULARGE_INTEGER result;
  result.QuadPart = 0;
  sz = 0;
  GetIfTable(NULL, &sz, FALSE);
  if (sz) {
    mit = (MIB_IFTABLE *) GetMem(sz);
    if (mit) {
      GetIfTable(mit, &sz, FALSE);
      for (i = 0; i < mit->dwNumEntries; i++) {
        if (mit->table[i].dwIndex == dwIndex) {
          result.LowPart  = mit->table[i].dwOutOctets;
          result.HighPart = mit->table[i].dwInOctets;
          break;
        }
      }
      FreeMem(mit);
    }
  }
  return(result);
}

void GetIPAddrStat(DWORD *ipaddr, DWORD *bout, DWORD *bin) {
MIB_IPADDRTABLE *ipat;
ULARGE_INTEGER ul;
DWORD tsz, i;
  *ipaddr = 0;
  *bout   = 0;
  *bin    = 0;
  tsz     = 0;
  // first call: get table length
  i = GetIpAddrTable(NULL, &tsz, TRUE);
  if ((i == ERROR_INSUFFICIENT_BUFFER) && (tsz)) {
    ipat = (MIB_IPADDRTABLE *) GetMem(tsz);
    if (ipat) {
      // get table
      i = GetIpAddrTable(ipat, &tsz, TRUE);
      if (i == NO_ERROR) {
        for (i = 0; i < ipat->dwNumEntries; i++) {
          // exclude 0.0.0.0 and 127.0.0.1 and get first one
          if (ipat->table[i].dwAddr && (ipat->table[i].dwAddr != 0x0100007F)) {
            *ipaddr = ipat->table[i].dwAddr;
            ul      = GetInOutOctetsByIndex(ipat->table[i].dwIndex);
            *bout   = ul.LowPart;
            *bin    = ul.HighPart;
            // trying to find what we looking for - this or last used
            // Unused2 (wType) MIB_IPADDR_PRIMARY (1) | MIB_IPADDR_DYNAMIC (4)
            // http://msdn.microsoft.com/en-us/library/windows/desktop/aa366845.aspx
            if ((ipat->table[i].unused2 & 5) == 5) { break; }
          }
        }
      }
      FreeMem(ipat);
    }
  }
}

TCHAR *FmtWithSpaces(DWORD value) {
TCHAR *result;
DWORD i, k;
  // max decimal digits in DWORD value with spaces
  i = 16;
  result = STR_ALLOC(i);
  if (result) {
    result[i] = 0;
    k = 0;
    do {
      i--;
      result[i] = TEXT('0') + (value%10);
      value /= 10;
      k++;
      if (value && i && (k >= 3)) {
        i--;
        result[i] = TEXT(' ');
        k = 0;
      }
    } while (value && i);
    if (i) {
      lstrcpy(result, &result[i]);
    }
  }
  return(result);
}

TCHAR *GetNetStat(DWORD ExtHost) {
DWORD dwip, dwin, dwout;
TCHAR *result, *sin, *sout;
  GetIPAddrStat(&dwip, &dwout, &dwin);
  // replace local IP with the external
  // only for the local address
  // http://en.wikipedia.org/wiki/Private_network
  if (ExtHost && (
       // 192.168.x.x
       (LOWORD(dwip) == 0xA8C0) ||
       // 10.x.x.x
       (LOBYTE(dwip) == 0x0A)   ||
       // 172.16.x.x - 172.31.x.x
      ((LOBYTE(dwip) == 0xAC) && ((HIBYTE(LOWORD(dwip)) ^ 0x10) < 0x10))
    )
  ) {
    dwip = ExtHost;
  }
  result = STR_ALLOC(1024);
  sin = FmtWithSpaces(dwin);
  sout = FmtWithSpaces(dwout);
  if (result && sin && sout) {
    wsprintf(result, TEXT("IP: %s\nSend: %s\nRecv: %s"), inet_ntoa(*(struct in_addr *) &dwip), sout, sin);
  } else {
    // v2.4
    if (result) { FreeMem(result); }
    result = NULL;
  }
  if (sin)  { FreeMem(sin);  }
  if (sout) { FreeMem(sout); }
  return(result);
}

//#if __GNUC__ == 3
#ifndef _ICMP_INCLUDED_
// http://www.mingw.org/wiki/CreateImportLibraries
extern HANDLE WINAPI IcmpCreateFile(void);
extern BOOL   WINAPI IcmpCloseHandle(HANDLE IcmpHandle);
extern DWORD  WINAPI IcmpSendEcho(HANDLE IcmpHandle, IPAddr DestinationAddress, LPVOID RequestData, WORD RequestSize, PIP_OPTION_INFORMATION RequestOptions, LPVOID ReplyBuffer, DWORD ReplySize, DWORD Timeout);
#endif

// https://groups.google.com/forum/#!topic/borland.public.cppbuilder.internet.socket/Jk64RSn4-B4
// http://www.ietf.org/rfc/rfc0791.txt
// Record Route
// Type=7
// this small routine resolves external IP address
// so nobody cares now about disabled UPnP
// yay!
DWORD GetExternalIPAddr(TCHAR *host) {
IP_OPTION_INFORMATION ipopt;
HANDLE hicmp;
DWORD dwip, result;
// buffer must large enough
// to hold ICMP_ECHO_REPLY and data after it
BYTE buf[sizeof(ICMP_ECHO_REPLY) + 8*4];
  result = 0;
  dwip = GetHostIPAddr(host);
  if (dwip != INADDR_NONE) {
    hicmp = IcmpCreateFile();
    if (hicmp != INVALID_HANDLE_VALUE) {
      ZeroMemory(buf, sizeof(ICMP_ECHO_REPLY) + 8*4);
      ZeroMemory(&ipopt, sizeof(ipopt));
      buf[0] = 7; // type - Record route option (IP_RECORD_ROUTE / IP_OPT_RR)
      buf[1] = 7; // len - sizeof(IP_OPTION_HEADER) - MUST BE (3 + N*4) or you'll get WSA_QOS_NO_SENDERS !!!
      buf[2] = 4; // ptr - Point to the first addr offset
      ipopt.Ttl = 128;
      ipopt.OptionsData = buf;
      ipopt.OptionsSize = 8; // must be len + 1
      if (IcmpSendEcho(hicmp, dwip, buf, 0, &ipopt, &buf[8], sizeof(ICMP_ECHO_REPLY) + 8*3, 500)) {
        result = *(DWORD *) &buf[8 + sizeof(ICMP_ECHO_REPLY) + 3];
      }
      IcmpCloseHandle(hicmp);
    }
  }
  return(result);
}
