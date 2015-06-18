#ifndef __NETUNIT_H
#define __NETUNIT_H

TCHAR *XMLGetValue(TCHAR *name, TCHAR *data);
TCHAR *CalcMD5Hash(BYTE *buf, DWORD sz);
BYTE *HTTPSGetContent(TCHAR *url, DWORD *len);
TCHAR *GetNetStat(DWORD ExtHost);
DWORD GetExternalIPAddr(TCHAR *host);

#endif
