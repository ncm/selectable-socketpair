/* socketpair.c
Copyright 2007, 2010 by Nathan C. Myers <ncm@cantrip.org>
Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.

    Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

    The name of the author must not be used to endorse or promote products
    derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Changes:
 * 2014-02-12: merge David Woodhouse, Ger Hobbelt improvements
 *     git.infradead.org/users/dwmw2/openconnect.git/commitdiff/bdeefa54
 *     github.com/GerHobbelt/selectable-socketpair
 *   always init the socks[] to -1/INVALID_SOCKET on error, both on Win32/64
 *   and UNIX/other platforms
 * 2013-07-18: Change to BSD 3-clause license
 * 2010-03-31:
 *   set addr to 127.0.0.1 because win32 getsockname does not always set it.
 * 2010-02-25:
 *   set SO_REUSEADDR option to avoid leaking some windows resource.
 *   Windows System Error 10049, "Event ID 4226 TCP/IP has reached
 *   the security limit imposed on the number of concurrent TCP connect
 *   attempts."  Bleah.
 * 2007-04-25:
 *   preserve value of WSAGetLastError() on all error returns.
 * 2007-04-22:  (Thanks to Matthew Gregan <kinetik@flim.org>)
 *   s/EINVAL/WSAEINVAL/ fix trivial compile failure
 *   s/socket/WSASocket/ enable creation of sockets suitable as stdin/stdout
 *     of a child process.
 *   add argument make_overlapped
 */

#include <string.h>
#include <inttypes.h>

#ifdef WIN32
# include <ws2tcpip.h>  /* socklen_t, et al (MSVC20xx) */
# include <windows.h>
# include <io.h>
# include <afunix.h>
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <errno.h>
#endif

#ifdef WIN32

/* dumb_socketpair:
 *   If make_overlapped is nonzero, both sockets created will be usable for
 *   "overlapped" operations via WSASend etc.  If make_overlapped is zero,
 *   socks[0] (only) will be usable with regular ReadFile etc., and thus
 *   suitable for use as stdin or stdout of a child process.  Note that the
 *   sockets must be closed with closesocket() regardless.
 */

int dumb_socketpair(SOCKET socks[2], int make_overlapped)
{
    union {
       struct sockaddr_un unaddr;
       struct sockaddr_in inaddr;
       struct sockaddr addr;
    } a;
    SOCKET listener;
    int e, ii;
    int domain = AF_UNIX;
    socklen_t addrlen = sizeof(a.unaddr);
    DWORD flags = (make_overlapped ? WSA_FLAG_OVERLAPPED : 0);
    int reuse = 1;

    if (socks == 0) {
      WSASetLastError(WSAEINVAL);
      return SOCKET_ERROR;
    }
    socks[0] = socks[1] = -1;

    /* AF_UNIX/SOCK_STREAM became available in Windows 10
     * ( https://devblogs.microsoft.com/commandline/af_unix-comes-to-windows )
     *
     * We will attempt to use AF_UNIX, but fallback to using AF_INET if
     * setting up AF_UNIX socket fails in any other way, which it surely will
     * on earlier versions of Windows.
     */
    for (ii = 0; ii < 2; ii++) {
        listener = socket(domain, SOCK_STREAM, domain == AF_INET ? IPPROTO_TCP : 0);
        if (listener == INVALID_SOCKET)
            goto fallback;

        memset(&a, 0, sizeof(a));
        if (domain == AF_UNIX) {
            /* Abstract sockets (filesystem-independent) don't work, contrary to
             * the claims of the aforementioned blog post:
             * ( https://github.com/microsoft/WSL/issues/4240#issuecomment-549663217 )
             *
             * So we must use a named path, and that comes with all the attendant
             * problems of permissions and collisions. Trying various temporary
             * directories and putting high-res time and PID in the filename, that
             * seems like a less-bad option.
             */
            LARGE_INTEGER ticks;
            DWORD n;
            int bind_try = 0;

            for (;;) {
                switch (bind_try++) {
                case 0:
                    /* "The returned string ends with a backslash" */
                    n = GetTempPath(UNIX_PATH_MAX, a.unaddr.sun_path);
                    break;
                case 1:
                    /* Heckuva job with API consistency, Microsoft! Reversed argument order, and
                     * "This path does not end with a backslash unless the Windows directory is the root directory.."
                     */
                    n = GetWindowsDirectory(a.unaddr.sun_path, UNIX_PATH_MAX);
                    n += snprintf(a.unaddr.sun_path + n, UNIX_PATH_MAX - n, "\\Temp\\");
                    break;
                case 2:
                    n = snprintf(a.unaddr.sun_path, UNIX_PATH_MAX, "C:\\Temp\\");
                    break;
                case 3:
                    n = 0; /* Current directory */
                    break;
                case 4:
                    goto fallback;
                }

                /* GetTempFileName could be used here.
                 * ( https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-gettempfilenamea )
                 * However it only adds 16 bits of time-based random bits,
                 * fails if there isn't room for a 14-character filename, and
                 * seems to offers no other apparent advantages. So we will
                 * use high-res timer ticks and PID for filename.
                 */
                QueryPerformanceCounter(&ticks);
                snprintf(a.unaddr.sun_path + n, UNIX_PATH_MAX - n,
                         "%"PRIx64"-%"PRId32".$$$", ticks.QuadPart, GetCurrentProcessId());
                a.unaddr.sun_family = AF_UNIX;

                if (bind(listener, &a.addr, addrlen) != SOCKET_ERROR)
                    break;
            }
        } else {
            a.inaddr.sin_family = AF_INET;
            a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            a.inaddr.sin_port = 0;

            if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                           (char *) &reuse, (socklen_t) sizeof(reuse)) == -1)
                goto fallback;;

            if (bind(listener, &a.addr, addrlen) == SOCKET_ERROR)
                goto fallback;

            memset(&a, 0, sizeof(a));
            if (getsockname(listener, &a.addr, &addrlen) == SOCKET_ERROR)
                goto fallback;

            // win32 getsockname may only set the port number, p=0.0005.
            // ( https://docs.microsoft.com/windows/win32/api/winsock/nf-winsock-getsockname ):
            a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            a.inaddr.sin_family = AF_INET;
        }

        if (listen(listener, 1) == SOCKET_ERROR)
            goto fallback;

        socks[0] = WSASocket(domain, SOCK_STREAM, 0, NULL, 0, flags);
        if (socks[0] == INVALID_SOCKET)
            goto fallback;
        if (connect(socks[0], &a.addr, addrlen) == SOCKET_ERROR)
            goto fallback;

        socks[1] = accept(listener, NULL, NULL);
        if (socks[1] == INVALID_SOCKET)
            goto fallback;

        closesocket(listener);
        return 0;

    fallback:
        domain = AF_INET;
        addrlen = sizeof(a.inaddr);

        e = WSAGetLastError();
        closesocket(listener);
        closesocket(socks[0]);
        closesocket(socks[1]);
        WSASetLastError(e);
    }

    socks[0] = socks[1] = -1;
    return SOCKET_ERROR;
}
#else
int dumb_socketpair(int socks[2], int dummy)
{
    if (socks == 0) {
        errno = EINVAL;
        return -1;
    }
    dummy = socketpair(AF_LOCAL, SOCK_STREAM, 0, socks);
    if (dummy)
        socks[0] = socks[1] = -1;
    return dummy;
}
#endif
