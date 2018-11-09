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
      len = lstrlen(name);
      tmp = STR_ALLOC(len);
      if (tmp) {
        len++;
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

// v2.6 CCHAR since only ANSI / UTF-8 allowed to escape
DWORD RawURLEncode(CCHAR *p, CCHAR *u, DWORD us) {
DWORD result, i;
BYTE mask[256];
CCHAR *s;
  result = 1;
  if (p && u && us) {
    // fill buffer (no static data)
    ZeroMemory(mask, 256);
    for (i = '0'; i <= '9'; i++) { mask[i] = 1; }
    for (i = 'A'; i <= 'Z'; i++) { mask[i] = 1; }
    for (i = 'a'; i <= 'z'; i++) { mask[i] = 1; }
    mask['-'] = 1;
    mask['.'] = 1;
    mask['_'] = 1;
    mask['~'] = 1;
    // check output buffer size
    i = 0;
    for (s = p; *s; s++) {
      i += mask[(BYTE) *s] ? 1 : 3;
    }
    // output buffer too small
    result = 2;
    if (i < us) {
      for (s = p; *s; s++) {
        if (mask[(BYTE) *s]) {
          *u = *s; u++;
        } else {
          *u = '%'; u++;
          *u = (((BYTE) *s) >> 4);
          *u += (*u < 10) ? '0' : ('A' - 10);
          u++;
          *u = (((BYTE) *s) & 0x0F);
          *u += (*u < 10) ? '0' : ('A' - 10);
          u++;
        }
      }
      // null char
      *u = 0;
      result = 0;
    }
  }
  return(result);
}

// v2.6
// https://rsdn.org/forum/src/2105333.hot
DWORD Base64En(BYTE *p, DWORD ps, TCHAR *u, DWORD us) {
DWORD result, k, i;
TCHAR list[65];
  // invalid arguments
  result = 1;
  if (p && ps && u && us) {
    // output buffer too small
    result = 2;
    if (us >= ((((ps - 1) / 3) + 1) * 4) + 1) {
      // fill buffer (no static data)
      for (i = 0; i < 26; i++) { list[i] = TEXT('A') + i; }
      for (i = 0; i < 26; i++) { list[i + 26] = TEXT('a') + i; }
      for (i = 0; i < 10; i++) { list[i + 52] = TEXT('0') + i; }
      list[62] = TEXT('+');
      list[63] = TEXT('/');
      list[64] = TEXT('=');
      // encode buffer
      while (ps > 2) {
        k = (p[0] << 16) | (p[1] << 8) | (p[2]);
        p += 3;
        ps -= 3;
        *u = list[(k >> 18) & 0x3F]; u++;
        *u = list[(k >> 12) & 0x3F]; u++;
        *u = list[(k >>  6) & 0x3F]; u++;
        *u = list[(k      ) & 0x3F]; u++;
      }
      // tail bytes
      if (ps) {
        k = (*p << 16); p++;
        if (ps > 1) { k |= (*p << 8); }
        *u = list[(k >> 18) & 0x3F]; u++;
        *u = list[(k >> 12) & 0x3F]; u++;
        if (ps > 1) { *u = list[(k >>  6) & 0x3F]; u++; }
        if (ps == 1) { *u = list[64]; u++; }
        *u = list[64]; u++;
      }
      // null terminator
      *u = 0;
      // no errors
      result = 0;
    }
  }
  return(result);
}

#define HASH_LEN 20
DWORD CalcSHAHash(BYTE *buf, DWORD sz, BYTE *hash) {
HCRYPTPROV hProv;
HCRYPTHASH hHash;
DWORD result;
  result = 1;
  if (buf && sz && hash) {
    result = 2;
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
      result = 3;
      if (CryptCreateHash(hProv, CALG_SHA, 0, 0, &hHash)) {
        result = 4;
        if (CryptHashData(hHash, buf, sz, 0)) {
          result = 5;
          sz = HASH_LEN;
          if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &sz, 0)) {
            result = 0;
          }
        }
        CryptDestroyHash(hHash);
      }
      CryptReleaseContext(hProv, 0);
    }
  }
  return(result);
}

#define BASE_LEN (((((HASH_LEN - 1) / 3) + 1) * 4) + 1)
DWORD GetEncodedPassword(TCHAR *p, TCHAR *u) {
BYTE hash[HASH_LEN];
TCHAR s[BASE_LEN];
DWORD result;
  #ifdef UNICODE
  convert to utf8 plz ktx by
  #endif
  result = 1;
  if (p && u && *p) {
    result = 20 + CalcSHAHash((BYTE *) p, lstrlen(p), hash);
    // hash created
    if (result == 20) {
      result = 30 + Base64En(hash, HASH_LEN, s, BASE_LEN);
      // Base64 encoded
      if (result == 30) {
        // (28 * 3) + 1 = worst case: 85 chars long
        result = RawURLEncode(s, u, ((BASE_LEN - 1) * 3) + 1);
        result += result ? 40 : 0;
      }
    }
  }
  return(result);
}

// it's not very optimized code but good for small Internet pages
#define MAX_BLOCK_SIZE 1024
BYTE *HTTPGetContent(TCHAR *url, DWORD *len) {
HINTERNET hOpen, hConn, hReq;
URL_COMPONENTS uc;
BYTE *result, *buf, *r;
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
  if (sz && ((uc.nScheme == INTERNET_SCHEME_HTTPS) || (uc.nScheme == INTERNET_SCHEME_HTTP)) &&
      uc.lpszHostName && uc.lpszUrlPath && uc.lpszHostName[0] && uc.lpszUrlPath[0]
  ) {
    // open
    hOpen = InternetOpen(NULL, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hOpen) {
      // connect
      sz = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
      hConn = InternetConnect(hOpen, uc.lpszHostName, sz, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
      if (hConn) {
        // request
        sz = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? (INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID) : 0;
        hReq = HttpOpenRequest(hConn, NULL, uc.lpszUrlPath, HTTP_VERSION, NULL, NULL,
          INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_COOKIES | // INTERNET_FLAG_NO_AUTO_REDIRECT |
          INTERNET_FLAG_NO_CACHE_WRITE | sz, 0);
        if (hReq) {
          // HttpSendRequest() didn't work with -1 as dwHeadersLength in Unicode build:
          // GetLastError() == 12150 ERROR_HTTP_HEADER_NOT_FOUND
          // https://msdn.microsoft.com/en-us/library/windows/desktop/aa384247.aspx
          sz = HttpSendRequest(hReq, TEXT("Connection: Close"), 17, NULL, 0);
          if (sz) {
            // check for Content-Length
            sz = sizeof(len[0]);
            uc.dwStructSize = 0;
            if (HttpQueryInfo(hReq, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, len, &sz, &uc.dwStructSize)) {
              result = (BYTE *) GetMem(*len + 1);
              if (result) {
                sz = 0;
                InternetReadFile(hReq, result, *len, &sz);
              }
            } else {
              // length unknown
              buf = GetMem(MAX_BLOCK_SIZE);
              if (buf) {
                do {
                  sz = 0;
                  InternetReadFile(hReq, buf, MAX_BLOCK_SIZE, &sz);
                  if (sz) {
                    r = GrowMem(result, *len + sz + 1);
                    if (r) {
                      result = r;
                      CopyMemory(&result[*len], buf, sz);
                    } else {
                      // not enough memory
                      FreeMem(result);
                      result = NULL;
                      *len = 2;
                      break;
                    }
                    *len += sz;
                  }
                } while (sz);
                FreeMem(buf);
              }
            }
            // add 0 at the end for text buffer data
            if (result && *len) {
              result[*len] = 0;
            }
          } else {
            // Windows XP, TLS not enabled in Internet Explorer
            if (uc.nScheme == INTERNET_SCHEME_HTTPS) {
              *len = GetLastError();
              *len = (
                (*len == ERROR_INTERNET_CANNOT_CONNECT) ||
                (*len == ERROR_INTERNET_CONNECTION_RESET)
              ) ? 1 : 0;
            }
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
      result[i] = TEXT('0') + (value % 10);
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
  result = STR_ALLOC(64);
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
