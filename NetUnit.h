#ifndef __NETUNIT_H
#define __NETUNIT_H

TCHAR *XMLGetValue(TCHAR *name, TCHAR *data);
DWORD GetEncodedPassword(TCHAR *p, TCHAR *u);
BYTE *HTTPGetContent(TCHAR *url, DWORD *len);
TCHAR *GetNetStat(DWORD ExtHost);
DWORD GetExternalIPAddr(TCHAR *host);

#endif
