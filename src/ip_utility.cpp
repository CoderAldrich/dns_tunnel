#include "ip_utility.h"
#include <string>
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
// For some reason these are missing from the mingw windows headers
extern "C" WINSOCK_API_LINKAGE const char* WSAAPI inet_pton(int af, const char* src, void* dest);
extern "C" WINSOCK_API_LINKAGE const char* WSAAPI inet_ntop(int af, const void* src,
char *dst, socklen_t size);
#else
#include <arpa/inet.h>
#endif

uint32_t ipton(const std::string& ipstr) {
    struct sockaddr_in sa;
    inet_pton(AF_INET,ipstr.c_str(), &(sa.sin_addr));
    return sa.sin_addr.s_addr;
    // TODO ipv6
}
std::string iptonstr(const std::string& ipstr) {
    struct sockaddr_in sa;
    inet_pton(AF_INET,ipstr.c_str(), &(sa.sin_addr));
    uint32_t ip = sa.sin_addr.s_addr;
    std::string ret((char*)&ip,sizeof(ip));
    return ret;
    // TODO ipv6
}
