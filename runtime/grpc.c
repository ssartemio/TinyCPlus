#ifndef TC_GRPC_IMPLEMENTATION
#define TC_GRPC_IMPLEMENTATION
#include "net.c"
#ifndef NGHTTP2_STATICLIB
#define NGHTTP2_STATICLIB
#endif
#include <nghttp2/nghttp2.h>

#define TC_GRPC_LIMIT (4u * 1024u * 1024u)
typedef struct TcGrpcBuffer {
    unsigned char *data;
    size_t length, capacity;
} TcGrpcBuffer;
typedef struct TcGrpcResponse {
    TinyString data;
    int32_t status;
} TcGrpcResponse;
typedef void *(*TcGrpcHandler)(TinyString, void *);
typedef struct TcGrpcRoute {
    TinyString path;
    TcGrpcHandler handler;
    void *argument;
    struct TcGrpcRoute *next;
} TcGrpcRoute;
typedef struct TcGrpcServer {
    int64_t listener;
    void *stopping, *running, *active;
    TcGrpcRoute *routes;
} TcGrpcServer;
typedef struct TcGrpcStream {
    int32_t id;
    TinyString path;
    TcGrpcBuffer incoming, outgoing;
    size_t sent, header_bytes;
    int status, have_status, http_status, grpc_type;
    struct TcGrpcStream *next;
} TcGrpcStream;
typedef struct TcGrpcConnection {
    nghttp2_session *session;
    TcSocket socket;
    TcGrpcServer *server;
    TcGrpcStream *streams;
    uint64_t deadline;
    int finished, error;
} TcGrpcConnection;
static int tc_grpc_buffer_add(TcGrpcBuffer *b, const void *data, size_t length) {
    size_t capacity;
    unsigned char *p;
    if (length > TC_GRPC_LIMIT + 5 - b->length)
        return -1;
    if (b->length + length > b->capacity) {
        capacity = b->capacity ? b->capacity : 256;
        while (capacity < b->length + length)
            capacity *= 2;
        p = (unsigned char *)realloc(b->data, capacity);
        if (!p)
            return -1;
        b->data = p;
        b->capacity = capacity;
    }
    if (length)
        memcpy(b->data + b->length, data, length);
    b->length += length;
    return 0;
}
static int tc_grpc_pack(TcGrpcBuffer *b, TinyString data) {
    uint32_t size;
    unsigned char header[5];
    if (data.length > TC_GRPC_LIMIT)
        return -1;
    size = (uint32_t)data.length;
    header[0] = 0;
    header[1] = (unsigned char)(size >> 24);
    header[2] = (unsigned char)(size >> 16);
    header[3] = (unsigned char)(size >> 8);
    header[4] = (unsigned char)size;
    return tc_grpc_buffer_add(b, header, 5) || tc_grpc_buffer_add(b, data.data, data.length);
}
static int tc_grpc_unpack(TcGrpcBuffer *b, TinyString *data) {
    uint32_t size;
    if (b->length < 5)
        return 13;
    if (b->data[0])
        return 12;
    size = ((uint32_t)b->data[1] << 24) | ((uint32_t)b->data[2] << 16) |
           ((uint32_t)b->data[3] << 8) | b->data[4];
    if (size > TC_GRPC_LIMIT || b->length != (size_t)size + 5)
        return 13;
    data->data = (const char *)b->data + 5;
    data->length = size;
    return 0;
}
void *tc_grpc_response(TinyString data, int32_t status) {
    TcGrpcResponse *r = (TcGrpcResponse *)calloc(1, sizeof(*r));
    if (!r)
        return NULL;
    r->status = status >= 0 && status <= 16 ? status : 2;
    if (data.length > TC_GRPC_LIMIT)
        r->status = 8;
    else {
        r->data = tc_string_copy(data);
        if (!r->data.data)
            r->status = 8;
    }
    return r;
}
TinyString tc_grpc_response_data(void *response) {
    TcGrpcResponse *r = (TcGrpcResponse *)response;
    TinyString empty = {NULL, 0};
    return r ? r->data : empty;
}
int32_t tc_grpc_response_status(void *response) {
    return response ? ((TcGrpcResponse *)response)->status : 8;
}
void tc_grpc_response_destroy(void *response) {
    TcGrpcResponse *r = (TcGrpcResponse *)response;
    if (r) {
        tc_string_free(r->data);
        free(r);
    }
}
static nghttp2_nv tc_grpc_nv(const char *name, const char *value) {
    nghttp2_nv n;
    n.name = (uint8_t *)name;
    n.value = (uint8_t *)value;
    n.namelen = strlen(name);
    n.valuelen = strlen(value);
    n.flags = NGHTTP2_NV_FLAG_NONE;
    return n;
}
static TcGrpcStream *tc_grpc_stream(TcGrpcConnection *c, int32_t id) {
    TcGrpcStream *s;
    for (s = c->streams; s; s = s->next)
        if (s->id == id)
            return s;
    return NULL;
}
static void tc_grpc_stream_free(TcGrpcStream *s) {
    tc_string_free(s->path);
    free(s->incoming.data);
    free(s->outgoing.data);
    free(s);
}
static int tc_grpc_begin(nghttp2_session *session, const nghttp2_frame *frame, void *opaque) {
    TcGrpcConnection *c = (TcGrpcConnection *)opaque;
    TcGrpcStream *s;
    (void)session;
    if (!c->server || frame->hd.type != NGHTTP2_HEADERS ||
        frame->headers.cat != NGHTTP2_HCAT_REQUEST)
        return 0;
    s = (TcGrpcStream *)calloc(1, sizeof(*s));
    if (!s)
        return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
    s->id = frame->hd.stream_id;
    s->next = c->streams;
    c->streams = s;
    return 0;
}
static int tc_grpc_header(nghttp2_session *session, const nghttp2_frame *frame, const uint8_t *name,
                          size_t namelen, const uint8_t *value, size_t valuelen, uint8_t flags,
                          void *opaque) {
    TcGrpcConnection *c = (TcGrpcConnection *)opaque;
    TcGrpcStream *s = tc_grpc_stream(c, frame->hd.stream_id);
    (void)session;
    (void)flags;
    if (!s)
        return 0;
    s->header_bytes += namelen + valuelen;
    if (s->header_bytes > 16384)
        return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
    if (namelen == 5 && !memcmp(name, ":path", 5)) {
        TinyString path = {(const char *)value, valuelen};
        if (valuelen > 4096)
            return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
        tc_string_free(s->path);
        s->path = tc_string_copy(path);
    } else if (namelen == 7 && !memcmp(name, ":status", 7)) {
        if (valuelen == 3)
            s->http_status = (value[0] - '0') * 100 + (value[1] - '0') * 10 + value[2] - '0';
    } else if (namelen == 12 && !memcmp(name, "content-type", 12)) {
        s->grpc_type = valuelen >= 16 && !memcmp(value, "application/grpc", 16) &&
                       (valuelen == 16 || value[16] == '+' || value[16] == ';');
    } else if (namelen == 11 && !memcmp(name, "grpc-status", 11)) {
        size_t i;
        s->status = 0;
        s->have_status = 1;
        if (!valuelen || valuelen > 2) {
            s->status = 2;
            return 0;
        }
        for (i = 0; i < valuelen; i++) {
            if (value[i] < '0' || value[i] > '9') {
                s->status = 2;
                return 0;
            }
            s->status = s->status * 10 + value[i] - '0';
        }
        if (s->status > 16)
            s->status = 2;
    }
    return 0;
}
static int tc_grpc_data(nghttp2_session *session, uint8_t flags, int32_t stream_id,
                        const uint8_t *data, size_t length, void *opaque) {
    TcGrpcConnection *c = (TcGrpcConnection *)opaque;
    TcGrpcStream *s = tc_grpc_stream(c, stream_id);
    (void)flags;
    if (s && tc_grpc_buffer_add(&s->incoming, data, length)) {
        nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, stream_id, NGHTTP2_ENHANCE_YOUR_CALM);
        s->status = 8;
    }
    return 0;
}
static nghttp2_ssize tc_grpc_read_data(nghttp2_session *session, int32_t stream_id, uint8_t *buffer,
                                       size_t capacity, uint32_t *flags,
                                       nghttp2_data_source *source, void *opaque) {
    TcGrpcConnection *c = (TcGrpcConnection *)opaque;
    TcGrpcStream *s = (TcGrpcStream *)source->ptr;
    size_t n = s->outgoing.length - s->sent;
    if (n > capacity)
        n = capacity;
    if (n)
        memcpy(buffer, s->outgoing.data + s->sent, n);
    s->sent += n;
    if (s->sent == s->outgoing.length) {
        *flags |= NGHTTP2_DATA_FLAG_EOF;
        if (c->server) {
            char status[8];
            nghttp2_nv trailer;
            snprintf(status, sizeof(status), "%d", s->status);
            trailer = tc_grpc_nv("grpc-status", status);
            *flags |= NGHTTP2_DATA_FLAG_NO_END_STREAM;
            if (nghttp2_submit_trailer(session, stream_id, &trailer, 1))
                return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
        }
    }
    return (nghttp2_ssize)n;
}
static void tc_grpc_answer(TcGrpcConnection *c, TcGrpcStream *s) {
    TinyString request = {NULL, 0}, empty = {"", 0};
    TcGrpcRoute *route;
    TcGrpcResponse *response = NULL;
    int status;
    nghttp2_nv headers[2];
    nghttp2_data_provider2 provider;
    status = s->grpc_type ? tc_grpc_unpack(&s->incoming, &request) : 3;
    if (!status) {
        for (route = c->server->routes; route; route = route->next)
            if (tc_string_equal(route->path, s->path))
                break;
        if (route)
            response = (TcGrpcResponse *)route->handler(request, route->argument);
        else
            status = 12;
    }
    if (!response)
        response = (TcGrpcResponse *)tc_grpc_response(empty, status ? status : 13);
    if (!response) {
        nghttp2_submit_rst_stream(c->session, 0, s->id, NGHTTP2_INTERNAL_ERROR);
        return;
    }
    s->status = response->status;
    if (tc_grpc_pack(&s->outgoing, response->data)) {
        s->status = 8;
        s->outgoing.length = 0;
        tc_grpc_pack(&s->outgoing, empty);
    }
    headers[0] = tc_grpc_nv(":status", "200");
    headers[1] = tc_grpc_nv("content-type", "application/grpc");
    memset(&provider, 0, sizeof(provider));
    provider.source.ptr = s;
    provider.read_callback = tc_grpc_read_data;
    if (nghttp2_submit_response2(c->session, s->id, headers, 2, &provider))
        c->error = 13;
    tc_grpc_response_destroy(response);
}
static int tc_grpc_frame(nghttp2_session *session, const nghttp2_frame *frame, void *opaque) {
    TcGrpcConnection *c = (TcGrpcConnection *)opaque;
    (void)session;
    if (c->server && (frame->hd.type == NGHTTP2_DATA || frame->hd.type == NGHTTP2_HEADERS) &&
        (frame->hd.flags & NGHTTP2_FLAG_END_STREAM)) {
        TcGrpcStream *s = tc_grpc_stream(c, frame->hd.stream_id);
        if (s)
            tc_grpc_answer(c, s);
    }
    return 0;
}
static int tc_grpc_closed(nghttp2_session *session, int32_t stream_id, uint32_t error,
                          void *opaque) {
    TcGrpcConnection *c = (TcGrpcConnection *)opaque;
    TcGrpcStream **p = &c->streams;
    (void)session;
    if (!c->server) {
        c->finished = 1;
        if (error)
            c->error = 13;
        return 0;
    }
    while (*p && (*p)->id != stream_id)
        p = &(*p)->next;
    if (*p) {
        TcGrpcStream *s = *p;
        *p = s->next;
        tc_grpc_stream_free(s);
    }
    return 0;
}
static int tc_grpc_session(TcGrpcConnection *c, int server) {
    nghttp2_session_callbacks *callbacks;
    int result;
    nghttp2_settings_entry settings[2];
    if (nghttp2_session_callbacks_new(&callbacks))
        return -1;
    nghttp2_session_callbacks_set_on_begin_headers_callback(callbacks, tc_grpc_begin);
    nghttp2_session_callbacks_set_on_header_callback(callbacks, tc_grpc_header);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, tc_grpc_data);
    nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, tc_grpc_frame);
    nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, tc_grpc_closed);
    result = server ? nghttp2_session_server_new(&c->session, callbacks, c)
                    : nghttp2_session_client_new(&c->session, callbacks, c);
    nghttp2_session_callbacks_del(callbacks);
    if (result)
        return result;
    settings[0].settings_id = NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS;
    settings[0].value = 32;
    settings[1].settings_id = NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE;
    settings[1].value = 16384;
    return nghttp2_submit_settings(c->session, 0, settings, 2);
}
static int tc_grpc_flush(TcGrpcConnection *c) {
    const uint8_t *data;
    nghttp2_ssize length;
    while ((length = nghttp2_session_mem_send2(c->session, &data)) > 0) {
        size_t sent = 0;
        while (sent < (size_t)length) {
            int n = send(c->socket, (const char *)data + sent, (int)((size_t)length - sent),
                         MSG_NOSIGNAL);
            if (n > 0)
                sent += (size_t)n;
            else if (n < 0 && tc_would_block(tc_socket_error())) {
                if (tc_clock_ms() >= c->deadline)
                    return 4;
                tc_wait_socket(c->socket, 1, 10);
            } else
                return 14;
        }
    }
    return length < 0 ? 13 : 0;
}
static void tc_grpc_exchange(TcGrpcConnection *c) {
    unsigned char buffer[16384];
    tc_nonblock(c->socket);
    while (!c->finished && !c->error) {
        int n, ready;
        c->error = tc_grpc_flush(c);
        if (c->error)
            break;
        if (c->server && tc_atomic_load(c->server->stopping))
            break;
        if (tc_clock_ms() >= c->deadline) {
            c->error = 4;
            break;
        }
        ready = tc_wait_socket(c->socket, 0, 10);
        if (ready < 0) {
            c->error = 14;
            break;
        }
        if (!ready)
            continue;
        n = recv(c->socket, (char *)buffer, sizeof(buffer), 0);
        if (n == 0) {
            if (!c->server && !c->finished)
                c->error = 14;
            break;
        }
        if (n < 0) {
            if (tc_would_block(tc_socket_error()))
                continue;
            c->error = 14;
            break;
        }
        if (nghttp2_session_mem_recv2(c->session, buffer, (size_t)n) < 0) {
            c->error = 13;
            break;
        }
    }
    if (!c->error)
        tc_grpc_flush(c);
}
static void tc_grpc_dispose(TcGrpcConnection *c) {
    TcGrpcStream *s = c->streams;
    nghttp2_session_del(c->session);
    while (s) {
        TcGrpcStream *next = s->next;
        tc_grpc_stream_free(s);
        s = next;
    }
    tc_socket_close((int64_t)c->socket);
}
static int64_t tc_grpc_connect(TinyString host, int port, uint64_t deadline, int32_t *error) {
    struct addrinfo *addresses = NULL, *a;
    TcSocket socket_value = TC_BAD_SOCKET;
    *error = tc_network_init();
    if (*error) {
        *error = 14;
        return -1;
    }
    if (tc_resolve(host, port, SOCK_STREAM, 0, &addresses)) {
        tc_network_cleanup();
        *error = 14;
        return -1;
    }
    *error = 14;
    for (a = addresses; a; a = a->ai_next) {
        int status;
        if (tc_clock_ms() >= deadline) {
            *error = 4;
            break;
        }
        socket_value = tc_create_socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (socket_value == TC_BAD_SOCKET)
            continue;
        if (tc_nonblock(socket_value)) {
            tc_close_socket(socket_value);
            socket_value = TC_BAD_SOCKET;
            continue;
        }
        status = connect(socket_value, a->ai_addr, (TcSockLen)a->ai_addrlen);
        if (status && tc_would_block(tc_socket_error())) {
            while (tc_clock_ms() < deadline) {
                int ready = tc_wait_socket(socket_value, 1, 10);
                TcSockLen length = sizeof(status);
                if (ready > 0) {
                    if (getsockopt(socket_value, SOL_SOCKET, SO_ERROR, (char *)&status, &length))
                        status = 1;
                    break;
                }
                if (ready < 0)
                    break;
            }
        }
        if (!status) {
            *error = 0;
            break;
        }
        tc_close_socket(socket_value);
        socket_value = TC_BAD_SOCKET;
    }
    freeaddrinfo(addresses);
    if (*error) {
        if (tc_clock_ms() >= deadline)
            *error = 4;
        tc_network_cleanup();
    }
    return *error ? -1 : (int64_t)socket_value;
}
void *tc_grpc_call(TinyString host, int32_t port, TinyString method, TinyString request,
                   uint64_t timeout) {
    TcGrpcConnection c;
    TcGrpcStream *stream;
    TcGrpcResponse *response;
    nghttp2_nv headers[6];
    nghttp2_data_provider2 provider;
    char authority[512], *hostname = tc_cstring(host), *path = tc_cstring(method);
    int32_t error = 0;
    int64_t socket;
    TinyString empty = {"", 0}, decoded = {NULL, 0};
    memset(&c, 0, sizeof(c));
    {
        uint64_t now = tc_clock_ms();
        if (!timeout)
            timeout = 30000;
        c.deadline = timeout > UINT64_MAX - now ? UINT64_MAX : now + timeout;
    }
    if (!hostname || !path || path[0] != '/' || request.length > TC_GRPC_LIMIT) {
        free(hostname);
        free(path);
        return tc_grpc_response(empty, 3);
    }
    if (snprintf(authority, sizeof(authority), strchr(hostname, ':') ? "[%s]:%d" : "%s:%d",
                 hostname, port) >= (int)sizeof(authority)) {
        free(hostname);
        free(path);
        return tc_grpc_response(empty, 3);
    }
    free(hostname);
    socket = tc_grpc_connect(host, port, c.deadline, &error);
    if (error || socket < 0) {
        free(path);
        return tc_grpc_response(empty, error ? error : 14);
    }
    c.socket = (TcSocket)socket;
    if (tc_grpc_session(&c, 0)) {
        free(path);
        tc_socket_close(socket);
        return tc_grpc_response(empty, 13);
    }
    stream = (TcGrpcStream *)calloc(1, sizeof(*stream));
    if (!stream)
        abort();
    c.streams = stream;
    if (tc_grpc_pack(&stream->outgoing, request)) {
        free(path);
        tc_grpc_dispose(&c);
        return tc_grpc_response(empty, 8);
    }
    headers[0] = tc_grpc_nv(":method", "POST");
    headers[1] = tc_grpc_nv(":scheme", "http");
    headers[2] = tc_grpc_nv(":authority", authority);
    headers[3] = tc_grpc_nv(":path", path);
    headers[4] = tc_grpc_nv("content-type", "application/grpc");
    headers[5] = tc_grpc_nv("te", "trailers");
    memset(&provider, 0, sizeof(provider));
    provider.source.ptr = stream;
    provider.read_callback = tc_grpc_read_data;
    stream->id = nghttp2_submit_request2(c.session, NULL, headers, 6, &provider, stream);
    free(path);
    if (stream->id < 0)
        c.error = 13;
    else
        tc_grpc_exchange(&c);
    error = c.error                                      ? c.error
            : stream->http_status != 200                 ? 14
            : !stream->grpc_type || !stream->have_status ? 2
                                                         : stream->status;
    if (!error)
        error = tc_grpc_unpack(&stream->incoming, &decoded);
    response = (TcGrpcResponse *)tc_grpc_response(error ? empty : decoded, error);
    tc_grpc_dispose(&c);
    return response;
}
typedef struct TcGrpcCallJob {
    void *future;
    TinyString host, path, request;
    int port;
    uint64_t timeout;
} TcGrpcCallJob;
static void tc_grpc_call_job(void *opaque) {
    TcGrpcCallJob *job = (TcGrpcCallJob *)opaque;
    void *response = tc_grpc_call(job->host, job->port, job->path, job->request, job->timeout);
    tc_future_complete(job->future, &response, 0);
    tc_future_release(job->future);
    tc_string_free(job->host);
    tc_string_free(job->path);
    tc_string_free(job->request);
    free(job);
    tc_task_end();
}
void *tc_grpc_call_async(TinyString host, int32_t port, TinyString method, TinyString request,
                         uint64_t timeout) {
    TcGrpcCallJob *job = (TcGrpcCallJob *)calloc(1, sizeof(*job));
    void *future = tc_future_create(sizeof(void *));
    if (!job || !future)
        abort();
    job->future = future;
    job->host = tc_string_copy(host);
    job->path = tc_string_copy(method);
    job->request = tc_string_copy(request);
    job->port = port;
    job->timeout = timeout;
    if (!job->host.data || !job->path.data || !job->request.data)
        abort();
    tc_future_retain(future);
    tc_task_begin();
    if (tc_pool_submit(tc_resolver_pool, tc_grpc_call_job, job))
        abort();
    return future;
}
void *tc_grpc_server_create(TinyString host, int32_t port, int32_t *error) {
    TcGrpcServer *s = (TcGrpcServer *)calloc(1, sizeof(*s));
    if (!s) {
        *error = 8;
        return NULL;
    }
    s->listener = tc_tcp_listen(host, port, 32, error);
    if (*error) {
        free(s);
        return NULL;
    }
    s->stopping = tc_atomic_create(0);
    s->running = tc_atomic_create(0);
    s->active = tc_atomic_create(0);
    if (!s->stopping || !s->running || !s->active)
        abort();
    return s;
}
int32_t tc_grpc_server_port(void *server) {
    return tc_socket_port(((TcGrpcServer *)server)->listener);
}
int32_t tc_grpc_server_add(void *server, TinyString path, TcGrpcHandler handler, void *argument) {
    TcGrpcServer *s = (TcGrpcServer *)server;
    TcGrpcRoute *r;
    if (!handler || !path.length || path.data[0] != '/' || path.length > 4096 ||
        tc_atomic_load(s->running))
        return 1;
    for (r = s->routes; r; r = r->next)
        if (tc_string_equal(r->path, path))
            return 2;
    r = (TcGrpcRoute *)malloc(sizeof(*r));
    if (!r)
        return 8;
    r->path = tc_string_copy(path);
    if (!r->path.data) {
        free(r);
        return 8;
    }
    r->handler = handler;
    r->argument = argument;
    r->next = s->routes;
    s->routes = r;
    return 0;
}
typedef struct TcGrpcConnectionJob {
    TcGrpcServer *server;
    TcSocket socket;
} TcGrpcConnectionJob;
static void tc_grpc_connection_job(void *opaque) {
    TcGrpcConnectionJob *job = (TcGrpcConnectionJob *)opaque;
    TcGrpcConnection c;
    memset(&c, 0, sizeof(c));
    c.server = job->server;
    c.socket = job->socket;
    c.deadline = tc_clock_ms() + 30000;
    free(job);
    if (tc_grpc_session(&c, 1))
        tc_socket_close((int64_t)c.socket);
    else {
        tc_grpc_exchange(&c);
        tc_grpc_dispose(&c);
    }
    tc_atomic_add(c.server->active, -1);
}
int32_t tc_grpc_server_serve(void *server, int32_t maximum_connections) {
    TcGrpcServer *s = (TcGrpcServer *)server;
    void *pool;
    int count = 0, error = 0;
    if (tc_atomic_add(s->running, 1) != 0) {
        tc_atomic_add(s->running, -1);
        return 1;
    }
    pool = tc_pool_create(4);
    if (!pool) {
        tc_atomic_store(s->running, 0);
        return 8;
    }
    while (!tc_atomic_load(s->stopping) &&
           (maximum_connections <= 0 || count < maximum_connections)) {
        TcGrpcConnectionJob *job;
        int ready = tc_wait_socket((TcSocket)s->listener, 0, 100);
        int32_t code = 0;
        int64_t socket;
        if (ready < 0) {
            error = 14;
            break;
        }
        if (!ready)
            continue;
        socket = tc_tcp_accept(s->listener, &code);
        if (code) {
            error = 14;
            break;
        }
        if (tc_atomic_load(s->active) >= 32) {
            tc_socket_close(socket);
            continue;
        }
        job = (TcGrpcConnectionJob *)malloc(sizeof(*job));
        if (!job) {
            tc_socket_close(socket);
            error = 8;
            break;
        }
        job->server = s;
        job->socket = (TcSocket)socket;
        tc_atomic_add(s->active, 1);
        if (tc_pool_submit(pool, tc_grpc_connection_job, job)) {
            tc_atomic_add(s->active, -1);
            tc_socket_close(socket);
            free(job);
            error = 8;
            break;
        }
        count++;
    }
    tc_pool_destroy(pool);
    tc_atomic_store(s->running, 0);
    return error;
}
void tc_grpc_server_stop(void *server) {
    tc_atomic_store(((TcGrpcServer *)server)->stopping, 1);
}
void tc_grpc_server_destroy(void *server) {
    TcGrpcServer *s = (TcGrpcServer *)server;
    TcGrpcRoute *r;
    if (!s)
        return;
    tc_grpc_server_stop(s);
    while (tc_atomic_load(s->running))
        tc_sleep_ms(1);
    tc_socket_close(s->listener);
    r = s->routes;
    while (r) {
        TcGrpcRoute *next = r->next;
        tc_string_free(r->path);
        free(r);
        r = next;
    }
    tc_atomic_destroy(s->stopping);
    tc_atomic_destroy(s->running);
    tc_atomic_destroy(s->active);
    free(s);
}
#endif
