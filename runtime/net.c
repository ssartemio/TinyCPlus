/*
 * Sockets TCP/UDP, DNS y operaciones asíncronas de red y archivos.
 * Adapta errores y handles entre Winsock y POSIX. Las peticiones asíncronas
 * se coordinan con futures y el reactor; conservar los buffers prestados
 * hasta que finalice la operación. Véase docs/CONCURRENCY.md.
 */
#ifndef TC_NET_IMPLEMENTATION
#define TC_NET_IMPLEMENTATION
#include "tiny_runtime.h"
#include "library.c"
#include "concurrent.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET TcSocket;
typedef int TcSockLen;
#define TC_BAD_SOCKET INVALID_SOCKET
#define tc_close_socket closesocket
static int tc_socket_error(void) {
    return WSAGetLastError();
}
static int tc_would_block(int e) {
    return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS;
}
static int tc_network_init(void) {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data);
}
static void tc_network_cleanup(void) {
    WSACleanup();
}
static int tc_nonblock(TcSocket s) {
    u_long enabled = 1;
    return ioctlsocket(s, FIONBIO, &enabled);
}
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
typedef int TcSocket;
typedef socklen_t TcSockLen;
#define TC_BAD_SOCKET (-1)
#define tc_close_socket close
static int tc_socket_error(void) {
    return errno;
}
static int tc_would_block(int e) {
    return e == EAGAIN || e == EWOULDBLOCK || e == EINPROGRESS;
}
static int tc_network_init(void) {
    return 0;
}
static void tc_network_cleanup(void) {}
static int tc_nonblock(TcSocket s) {
    int flags = fcntl(s, F_GETFL, 0);
    return flags < 0 ? -1 : fcntl(s, F_SETFL, flags | O_NONBLOCK);
}
#endif
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
static TcSocket tc_create_socket(int family, int type, int protocol) {
    TcSocket s = socket(family, type, protocol);
#ifdef SO_NOSIGPIPE
    if (s != TC_BAD_SOCKET) {
        int enabled = 1;
        setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, (const char *)&enabled, sizeof(enabled));
    }
#endif
    return s;
}

static int tc_resolve(TinyString host, int port, int type, int passive, struct addrinfo **result) {
    struct addrinfo hint;
    char service[16], *name = tc_cstring(host);
    int error;
    if (port < 0 || port > 65535 || !name) {
        free(name);
        return 1;
    }
    memset(&hint, 0, sizeof(hint));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = type;
    hint.ai_flags = passive ? AI_PASSIVE : 0;
    snprintf(service, sizeof(service), "%d", port);
    error = getaddrinfo(*name ? name : NULL, service, &hint, result);
    free(name);
    return error;
}
int64_t tc_tcp_connect(TinyString host, int32_t port, int32_t *error) {
    struct addrinfo *addresses = NULL, *a;
    TcSocket s = TC_BAD_SOCKET;
    int init = tc_network_init();
    *error = init;
    if (init)
        return -1;
    *error = tc_resolve(host, port, SOCK_STREAM, 0, &addresses);
    if (*error) {
        tc_network_cleanup();
        return -1;
    }
    for (a = addresses; a; a = a->ai_next) {
        s = tc_create_socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (s == TC_BAD_SOCKET)
            continue;
        if (connect(s, a->ai_addr, (TcSockLen)a->ai_addrlen) == 0)
            break;
        tc_close_socket(s);
        s = TC_BAD_SOCKET;
    }
    freeaddrinfo(addresses);
    *error = s == TC_BAD_SOCKET ? tc_socket_error() : 0;
    if (s == TC_BAD_SOCKET)
        tc_network_cleanup();
    return s == TC_BAD_SOCKET ? -1 : (int64_t)s;
}
static int64_t tc_bind_socket(TinyString host, int32_t port, int type, int backlog,
                              int32_t *error) {
    struct addrinfo *addresses = NULL, *a;
    TcSocket s = TC_BAD_SOCKET;
    int yes = 1;
    *error = tc_network_init();
    if (*error)
        return -1;
    *error = tc_resolve(host, port, type, 1, &addresses);
    if (*error) {
        tc_network_cleanup();
        return -1;
    }
    for (a = addresses; a; a = a->ai_next) {
        s = tc_create_socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (s == TC_BAD_SOCKET)
            continue;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
        if (bind(s, a->ai_addr, (TcSockLen)a->ai_addrlen) == 0 &&
            (type != SOCK_STREAM || listen(s, backlog) == 0))
            break;
        tc_close_socket(s);
        s = TC_BAD_SOCKET;
    }
    freeaddrinfo(addresses);
    *error = s == TC_BAD_SOCKET ? tc_socket_error() : 0;
    if (s == TC_BAD_SOCKET)
        tc_network_cleanup();
    return s == TC_BAD_SOCKET ? -1 : (int64_t)s;
}
int64_t tc_tcp_listen(TinyString host, int32_t port, int32_t backlog, int32_t *error) {
    return tc_bind_socket(host, port, SOCK_STREAM, backlog, error);
}
int64_t tc_udp_bind(TinyString host, int32_t port, int32_t *error) {
    return tc_bind_socket(host, port, SOCK_DGRAM, 0, error);
}
int32_t tc_socket_port(int64_t handle) {
    struct sockaddr_storage addr;
    TcSockLen n = sizeof(addr);
    if (getsockname((TcSocket)handle, (struct sockaddr *)&addr, &n))
        return -1;
    if (addr.ss_family == AF_INET)
        return ntohs(((struct sockaddr_in *)&addr)->sin_port);
    if (addr.ss_family == AF_INET6)
        return ntohs(((struct sockaddr_in6 *)&addr)->sin6_port);
    return -1;
}
static int tc_wait_socket(TcSocket s, int writing, int timeout_ms) {
    fd_set set;
    struct timeval timeout, *ptr = NULL;
#ifndef _WIN32
    if (s < 0 || s >= FD_SETSIZE)
        return -1;
#endif
    memset(&set, 0, sizeof(set));
    FD_SET(s, &set);
    if (timeout_ms >= 0) {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        ptr = &timeout;
    }
    return select((int)(s + 1), writing ? NULL : &set, writing ? &set : NULL, NULL, ptr);
}
int64_t tc_tcp_accept(int64_t listener, int32_t *error) {
    TcSocket s;
    for (;;) {
        s = accept((TcSocket)listener, NULL, NULL);
        if (s != TC_BAD_SOCKET) {
            tc_network_init();
            *error = 0;
            return (int64_t)s;
        }
        *error = tc_socket_error();
        if (!tc_would_block(*error) || tc_wait_socket((TcSocket)listener, 0, -1) <= 0)
            return -1;
    }
}
int64_t tc_socket_read(int64_t handle, void *data, uint64_t capacity) {
    int n;
    if (capacity > INT_MAX)
        capacity = INT_MAX;
    for (;;) {
        n = recv((TcSocket)handle, (char *)data, (int)capacity, 0);
        if (n >= 0)
            return n;
        if (!tc_would_block(tc_socket_error()) || tc_wait_socket((TcSocket)handle, 0, -1) <= 0)
            return -1;
    }
}
int64_t tc_socket_write(int64_t handle, void *data, uint64_t length) {
    int n;
    if (length > INT_MAX)
        length = INT_MAX;
    for (;;) {
        n = send((TcSocket)handle, (const char *)data, (int)length, MSG_NOSIGNAL);
        if (n >= 0)
            return n;
        if (!tc_would_block(tc_socket_error()) || tc_wait_socket((TcSocket)handle, 1, -1) <= 0)
            return -1;
    }
}
/* Transfer results always preserve bytes completed before an OS error or EOF. */
int64_t tc_socket_transfer(int64_t handle, void *data, uint64_t length,
                            int32_t writing, int32_t exactly, int32_t *error) {
    uint64_t offset = 0;
    *error = 0;
    if (length > SIZE_MAX || length > INT64_MAX || (!data && length)) {
        *error = EINVAL; return 0;
    }
    while (offset < length) {
        int64_t count = writing ? tc_socket_write(handle, (char *)data + offset, length - offset)
                                : tc_socket_read(handle, (char *)data + offset, length - offset);
        if (count < 0) { *error = tc_socket_error(); break; }
        if (count == 0) break;
        offset += (uint64_t)count;
        if (!exactly) break;
    }
    return (int64_t)offset;
}
int32_t tc_socket_close(int64_t handle) {
    int result;
    if (handle < 0)
        return 0;
    result = tc_close_socket((TcSocket)handle);
    result = result ? tc_socket_error() : 0;
    tc_network_cleanup();
    return result;
}
int64_t tc_udp_send(int64_t handle, TinyString host, int32_t port, void *data, uint64_t size) {
    struct addrinfo *addresses = NULL, *a;
    struct sockaddr_storage local;
    TcSockLen local_size = sizeof(local);
    int result = -1;
    if (size > 65507 || getsockname((TcSocket)handle, (struct sockaddr *)&local, &local_size) ||
        tc_resolve(host, port, SOCK_DGRAM, 0, &addresses))
        return -1;
    for (a = addresses; a; a = a->ai_next) {
        if (a->ai_family != local.ss_family) continue;
        result = sendto((TcSocket)handle, (const char *)data, (int)size, 0, a->ai_addr,
                        (TcSockLen)a->ai_addrlen);
        if (result >= 0) break;
    }
    freeaddrinfo(addresses);
    return result;
}
int64_t tc_udp_receive(int64_t handle, void *data, uint64_t capacity, void *peer,
                       uint64_t peer_capacity, int32_t *port) {
    struct sockaddr_storage addr;
    TcSockLen n = sizeof(addr);
    int got;
    char service[16];
    if (capacity > INT_MAX)
        capacity = INT_MAX;
    got = recvfrom((TcSocket)handle, (char *)data, (int)capacity, 0, (struct sockaddr *)&addr, &n);
    if (got < 0)
        return -1;
    if (peer && peer_capacity &&
        getnameinfo((struct sockaddr *)&addr, n, (char *)peer, (TcSockLen)peer_capacity, service,
                    sizeof(service), NI_NUMERICHOST | NI_NUMERICSERV) == 0)
        *port = atoi(service);
    return got;
}
TinyString tc_dns_resolve(TinyString host, int32_t *error) {
    struct addrinfo *addresses = NULL;
    char name[128];
    TinyString result = {NULL, 0};
    *error = tc_network_init();
    if (*error)
        return result;
    *error = tc_resolve(host, 0, SOCK_STREAM, 0, &addresses);
    if (!*error) {
        *error = getnameinfo(addresses->ai_addr, (TcSockLen)addresses->ai_addrlen, name,
                             sizeof(name), NULL, 0, NI_NUMERICHOST);
        if (!*error) {
            result.data = name;
            result.length = strlen(name);
            result = tc_string_copy(result);
            if (!result.data)
                *error = 2;
        }
        freeaddrinfo(addresses);
    }
    tc_network_cleanup();
    return result;
}

enum { TC_IO_READ, TC_IO_WRITE, TC_IO_ACCEPT, TC_IO_CONNECT, TC_IO_TIMER, TC_IO_READ_EXACT, TC_IO_WRITE_SOME };
typedef struct TcIoRequest {
    int kind, error, done, native_error;
    TcSocket socket;
    void *data, *token, *future;
    size_t length, offset;
    uint64_t deadline;
    int64_t result;
    struct addrinfo *addresses, *candidate;
    struct TcIoRequest *next;
} TcIoRequest;
static void *tc_io_lock, *tc_io_thread, *tc_resolver_pool;
static TcIoRequest *tc_io_requests;
static int tc_io_stopping, tc_io_count;
static void tc_io_complete(TcIoRequest *r) {
    tc_future_set_native_error(r->future, r->native_error);
    tc_future_complete(r->future, &r->result, r->error);
    tc_future_release(r->future);
    tc_task_end();
    if (r->addresses) freeaddrinfo(r->addresses);
    free(r);
}
/* Return 1 for pending, 2 for connected, 0 when every address failed. */
static int tc_connect_next(TcIoRequest *r) {
    struct addrinfo *a;
    while ((a = r->candidate) != NULL) {
        TcSocket socket;
        int result;
        r->candidate = a->ai_next;
        socket = tc_create_socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (socket == TC_BAD_SOCKET) continue;
        if (tc_nonblock(socket)
#ifndef _WIN32
            || socket >= FD_SETSIZE
#endif
        ) { tc_close_socket(socket); continue; }
        result = connect(socket, a->ai_addr, (TcSockLen)a->ai_addrlen);
        if (result == 0 || tc_would_block(tc_socket_error())) {
            r->socket = socket;
            r->result = (int64_t)socket;
            return result == 0 ? 2 : 1;
        }
        r->error = r->native_error = tc_socket_error();
        tc_close_socket(socket);
    }
    r->socket = TC_BAD_SOCKET;
    if (!r->error) r->error = 13;
    return 0;
}
static void tc_io_enqueue(TcIoRequest *r) {
    tc_mutex_lock(tc_io_lock);
    if (tc_io_stopping || tc_io_count >= 32) {
        tc_mutex_unlock(tc_io_lock);
        r->error = 8;
        if (r->kind == TC_IO_CONNECT)
            tc_socket_close((int64_t)r->socket);
        tc_io_complete(r);
        return;
    }
    r->next = tc_io_requests;
    tc_io_requests = r;
    tc_io_count++;
    tc_mutex_unlock(tc_io_lock);
}
static void tc_io_loop(void *unused) {
    (void)unused;
    for (;;) {
        fd_set reads, writes, errors;
        TcSocket maximum = 0;
        struct timeval timeout;
        TcIoRequest *r, *complete = NULL, **cursor;
        int sockets = 0;
        uint64_t now;
        memset(&reads, 0, sizeof(reads));
        memset(&writes, 0, sizeof(writes));
        memset(&errors, 0, sizeof(errors));
        tc_mutex_lock(tc_io_lock);
        if (tc_io_stopping && !tc_io_requests) {
            tc_mutex_unlock(tc_io_lock);
            return;
        }
        for (r = tc_io_requests; r; r = r->next)
            if (r->kind != TC_IO_TIMER) {
                if (r->kind == TC_IO_WRITE || r->kind == TC_IO_WRITE_SOME || r->kind == TC_IO_CONNECT)
                    FD_SET(r->socket, &writes);
                else
                    FD_SET(r->socket, &reads);
                FD_SET(r->socket, &errors);
                if (r->socket > maximum)
                    maximum = r->socket;
                sockets++;
            }
        tc_mutex_unlock(tc_io_lock);
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000;
        if (sockets)
            select((int)(maximum + 1), &reads, &writes, &errors, &timeout);
        else
            tc_sleep_ms(1);
        now = tc_clock_ms();
        tc_mutex_lock(tc_io_lock);
        cursor = &tc_io_requests;
        while ((r = *cursor) != NULL) {
            if (tc_io_stopping || (r->token && tc_atomic_load(r->token))) {
                r->error = 1;
                r->done = 1;
            } else if (r->deadline && now >= r->deadline) {
                r->error = r->kind == TC_IO_TIMER ? 0 : 4;
                r->done = 1;
            } else if (r->kind != TC_IO_TIMER &&
                       (FD_ISSET(r->socket, &reads) || FD_ISSET(r->socket, &writes) ||
                        FD_ISSET(r->socket, &errors))) {
                int n = -1, error = 0;
                if (r->kind == TC_IO_CONNECT) {
                    TcSockLen length = sizeof(error);
                    if (getsockopt(r->socket, SOL_SOCKET, SO_ERROR, (char *)&error, &length))
                        error = tc_socket_error();
                    r->result = (int64_t)r->socket;
                    r->error = error; r->native_error = error;
                    r->done = 1;
                    if (error && r->candidate) {
                        int state;
                        tc_close_socket(r->socket);
                        state = tc_connect_next(r);
                        if (state) r->error = r->native_error = 0;
                        r->done = state != 1;
                    }
                } else if (r->kind == TC_IO_ACCEPT) {
                    TcSocket accepted = accept(r->socket, NULL, NULL);
                    if (accepted != TC_BAD_SOCKET) {
                        tc_network_init();
                        r->result = (int64_t)accepted;
                        r->done = 1;
                    } else if (!tc_would_block(tc_socket_error())) {
                        r->native_error = tc_socket_error();
                        r->error = 13;
                        r->done = 1;
                    }
                } else {
                    size_t remaining = r->length - r->offset;
                    int chunk = (int)(remaining > INT_MAX ? INT_MAX : remaining);
                    if (r->kind == TC_IO_READ || r->kind == TC_IO_READ_EXACT)
                        n = recv(r->socket, (char *)r->data + r->offset, chunk, 0);
                    else
                        n = send(r->socket, (char *)r->data + r->offset, chunk, MSG_NOSIGNAL);
                    if (n >= 0) {
                        r->offset += (size_t)n;
                        if (r->kind == TC_IO_READ || r->kind == TC_IO_WRITE_SOME || r->offset == r->length || n == 0) {
                            r->result = (int64_t)r->offset;
                            r->done = 1;
                        }
                    } else if (!tc_would_block(tc_socket_error())) {
                        r->native_error = tc_socket_error();
                        r->error = 13;
                        r->done = 1;
                    }
                }
            }
            if (r->done) {
                if (r->kind == TC_IO_READ || r->kind == TC_IO_READ_EXACT ||
                    r->kind == TC_IO_WRITE || r->kind == TC_IO_WRITE_SOME)
                    r->result = (int64_t)r->offset;
                *cursor = r->next;
                r->next = complete;
                complete = r;
                tc_io_count--;
            } else
                cursor = &r->next;
        }
        tc_mutex_unlock(tc_io_lock);
        while (complete) {
            r = complete;
            complete = r->next;
            if (r->kind == TC_IO_CONNECT && r->error) {
                if (r->socket != TC_BAD_SOCKET) tc_close_socket(r->socket);
                tc_network_cleanup();
                r->result = -1;
            }
            tc_io_complete(r);
        }
    }
}
void tc_io_start(void) {
    tc_io_lock = tc_mutex_create();
    if (!tc_io_lock)
        abort();
    tc_io_stopping = 0;
    tc_io_count = 0;
    tc_io_requests = NULL;
    tc_resolver_pool = tc_pool_create(2);
    tc_io_thread = tc_thread_start(tc_io_loop, NULL);
    if (!tc_resolver_pool || !tc_io_thread)
        abort();
}
void tc_io_cancel(void) {
    tc_mutex_lock(tc_io_lock);
    tc_io_stopping = 1;
    tc_mutex_unlock(tc_io_lock);
}
void tc_io_shutdown(void) {
    tc_pool_destroy(tc_resolver_pool);
    tc_mutex_lock(tc_io_lock);
    tc_io_stopping = 1;
    tc_mutex_unlock(tc_io_lock);
    tc_thread_destroy(tc_io_thread);
    tc_mutex_destroy(tc_io_lock);
    tc_io_lock = NULL;
}
static TcIoRequest *tc_io_new(int kind, int64_t socket, void *data, uint64_t length,
                              uint64_t timeout, void *token) {
    TcIoRequest *r = (TcIoRequest *)calloc(1, sizeof(*r));
    if (!r)
        abort();
    r->kind = kind;
    r->socket = (TcSocket)socket;
    r->data = data;
    r->length = (size_t)length;
    r->token = token;
    r->future = tc_future_create(kind == TC_IO_TIMER ? 0 : sizeof(int64_t));
    if (!r->future)
        abort();
    tc_future_retain(r->future);
    tc_task_begin();
    if (timeout) {
        uint64_t now = tc_clock_ms();
        r->deadline = timeout > UINT64_MAX - now ? UINT64_MAX : now + timeout;
    }
    return r;
}
static void *tc_socket_async(int kind, int64_t handle, void *data, uint64_t length,
                             uint64_t timeout, void *token) {
    TcIoRequest *r = tc_io_new(kind, handle, data, length, timeout, token);
    void *future = r->future;
    if (handle < 0 || length > SIZE_MAX || tc_nonblock((TcSocket)handle)
#ifndef _WIN32
        || handle >= FD_SETSIZE
#endif
    ) {
        r->error = 13;
        tc_io_complete(r);
    } else if (!length && (kind == TC_IO_READ || kind == TC_IO_READ_EXACT || kind == TC_IO_WRITE || kind == TC_IO_WRITE_SOME))
        tc_io_complete(r);
    else
        tc_io_enqueue(r);
    return future;
}
void *tc_socket_read_async(int64_t handle, void *data, uint64_t capacity, uint64_t timeout,
                           void *token) {
    return tc_socket_async(TC_IO_READ, handle, data, capacity, timeout, token);
}
void *tc_socket_read_exact_async(int64_t handle, void *data, uint64_t length, uint64_t timeout, void *token) {
    return tc_socket_async(TC_IO_READ_EXACT, handle, data, length, timeout, token);
}
void *tc_socket_write_some_async(int64_t handle, void *data, uint64_t length, uint64_t timeout, void *token) {
    return tc_socket_async(TC_IO_WRITE_SOME, handle, data, length, timeout, token);
}
void *tc_socket_write_async(int64_t handle, void *data, uint64_t length, uint64_t timeout,
                            void *token) {
    return tc_socket_async(TC_IO_WRITE, handle, data, length, timeout, token);
}
void *tc_tcp_accept_async(int64_t handle, uint64_t timeout, void *token) {
    return tc_socket_async(TC_IO_ACCEPT, handle, NULL, 0, timeout, token);
}
void *tc_timer_after(uint64_t milliseconds) {
    TcIoRequest *r = tc_io_new(TC_IO_TIMER, -1, NULL, 0, milliseconds ? milliseconds : 1, NULL);
    void *f = r->future;
    tc_io_enqueue(r);
    return f;
}
typedef struct TcConnectJob {
    TcIoRequest *request;
    TinyString host;
    int port;
} TcConnectJob;
static void tc_connect_resolve(void *opaque) {
    TcConnectJob *job = (TcConnectJob *)opaque;
    TcIoRequest *r = job->request;
    int queued = 0;
    r->error = tc_network_init();
    if (!r->error)
        r->error = tc_resolve(job->host, job->port, SOCK_STREAM, 0, &r->addresses);
    if (r->error) r->native_error = r->error;
    if (!r->error) {
        r->candidate = r->addresses;
        queued = tc_connect_next(r);
        if (queued) r->error = r->native_error = 0;
    }
    tc_string_free(job->host);
    free(job);
    if (queued == 1)
        tc_io_enqueue(r);
    else {
        if (r->error)
            tc_network_cleanup();
        tc_io_complete(r);
    }
}
void *tc_tcp_connect_async(TinyString host, int32_t port, uint64_t timeout, void *token) {
    TcIoRequest *r = tc_io_new(TC_IO_CONNECT, -1, NULL, 0, timeout, token);
    TcConnectJob *job = (TcConnectJob *)malloc(sizeof(*job));
    void *future = r->future;
    if (!job)
        abort();
    job->request = r;
    job->host = tc_string_copy(host);
    job->port = port;
    if (!job->host.data || tc_pool_submit_sized(tc_resolver_pool, tc_connect_resolve, job, host.length)) {
        tc_string_free(job->host); free(job);
        r->error = 8; r->result = -1; tc_io_complete(r);
    }
    return future;
}
typedef struct TcFileJob {
    void *future;
    TinyString path;
} TcFileJob;
static void tc_file_job(void *opaque) {
    TcFileJob *job = (TcFileJob *)opaque;
    int32_t error;
    TinyString data = tc_file_read_all(job->path, &error);
    tc_future_set_native_error(job->future, error);
    tc_future_complete(job->future, &data, error);
    tc_future_release(job->future);
    tc_string_free(job->path);
    free(job);
    tc_task_end();
}
void *tc_file_read_async(TinyString path) {
    TcFileJob *job = (TcFileJob *)malloc(sizeof(*job));
    void *future = tc_future_create(sizeof(TinyString));
    if (!job || !future)
        abort();
    tc_future_retain(future);
    job->future = future;
    job->path = tc_string_copy(path);
    tc_task_begin();
    if (!job->path.data || tc_pool_submit_sized(tc_resolver_pool, tc_file_job, job, path.length)) {
        tc_future_complete(future, NULL, 8); tc_future_release(future);
        tc_string_free(job->path); free(job); tc_task_end();
    }
    return future;
}

typedef struct TcBlockingIoJob {
    void *future;
    int operation;
    TinyString name, contents;
} TcBlockingIoJob;
static void tc_blocking_io_job(void *opaque) {
    TcBlockingIoJob *job = (TcBlockingIoJob *)opaque;
    int32_t error = 0;
    TinyString result = {0};
    if (job->operation == 1)
        result = tc_dns_resolve(job->name, &error);
    else
        error = tc_file_write_all(job->name, job->contents);
    tc_future_set_native_error(job->future, error);
    tc_future_complete(job->future, job->operation == 1 ? &result : NULL, error);
    tc_future_release(job->future);
    tc_string_free(job->name);
    tc_string_free(job->contents);
    free(job);
    tc_task_end();
}
static void *tc_blocking_io(int operation, TinyString name, TinyString contents) {
    TcBlockingIoJob *job = (TcBlockingIoJob *)calloc(1, sizeof(*job));
    void *future = tc_future_create(operation == 1 ? sizeof(TinyString) : 0);
    if (!job || !future)
        abort();
    job->operation = operation;
    job->name = tc_string_copy(name);
    job->contents = tc_string_copy(contents);
    job->future = future;
    tc_future_retain(future);
    tc_task_begin();
    if (!job->name.data || !job->contents.data ||
        name.length > UINT64_MAX - contents.length ||
        tc_pool_submit_sized(tc_resolver_pool, tc_blocking_io_job, job, name.length + contents.length)) {
        tc_future_complete(future, NULL, 8); tc_future_release(future);
        tc_string_free(job->name); tc_string_free(job->contents); free(job); tc_task_end();
    }
    return future;
}
void *tc_dns_resolve_async(TinyString host) {
    return tc_blocking_io(1, host, TC_STRING(""));
}
void *tc_file_write_async(TinyString path, TinyString contents) {
    return tc_blocking_io(2, path, contents);
}
#endif
