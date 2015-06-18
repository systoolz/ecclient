@windres -i ECClient.rc -O coff -o ECClient.res
@dlltool -k -d icmp.def -l icmp.a
