typedef void* SOCKET;
typedef unsigned int DWORD;
#define WSA_FLAG_OVERLAPPED 1
extern void WSASetLastError(int);
extern int WSAGetLastError();
#define WSAEINVAL 22
#define SOCKET_ERROR -1
#define AF_INET 33
#define SOCK_STREAM 44
#define IPPROTO_TCP 55
#define INVALID_SOCKET 0
extern void* socket(int, int, int);
struct sockaddr_in {
    int sin_family;
    struct { unsigned s_addr; } sin_addr;
    unsigned short sin_port;
};
struct sockaddr { int dummy; };
typedef unsigned socklen_t;
#define SOL_SOCKET 66
#define SO_REUSEADDR 77
#define INADDR_LOOPBACK 88
extern int setsockopt(SOCKET, int, int, char*, unsigned);
extern int bind(SOCKET, struct sockaddr*, unsigned);
extern int getsockname(SOCKET, struct sockaddr*, socklen_t*);
extern int listen(SOCKET, int);
extern SOCKET WSASocket(int, int, int, void*, int, int);
extern int connect(SOCKET, struct sockaddr*, unsigned);
extern SOCKET accept(SOCKET, void*, void*);
extern int closesocket(SOCKET);
extern unsigned htonl(unsigned);
