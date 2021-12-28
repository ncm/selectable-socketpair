typedef void* SOCKET;
typedef unsigned int DWORD;
typedef unsigned int GROUP;
#define WSA_FLAG_OVERLAPPED 1
extern void WSASetLastError(int);
extern int WSAGetLastError();
#define WSAEINVAL 22
#define SOCKET_ERROR -1
#define AF_INET 33
#define SOCK_STREAM 44
#define IPPROTO_TCP 55
#define INVALID_SOCKET (SOCKET)(~0)
extern void* socket(int, int, int);
struct sockaddr_in {
    int sin_family;
    struct { unsigned s_addr; } sin_addr;
    unsigned short sin_port;
};
struct sockaddr { int dummy; };
#define SOL_SOCKET 66
#define SO_REUSEADDR 77
#define INADDR_LOOPBACK 88
extern int setsockopt(SOCKET, int, int, const char*, int);
extern int bind(SOCKET, const struct sockaddr*, int);
extern int getsockname(SOCKET, struct sockaddr*, int*);
extern int listen(SOCKET, int);
extern SOCKET WSASocket(int, int, int, void*, GROUP, DWORD);
extern int connect(SOCKET, const struct sockaddr*, int);
extern SOCKET accept(SOCKET, struct sockaddr*, int*);
extern int closesocket(SOCKET);
extern unsigned long htonl(unsigned long);
