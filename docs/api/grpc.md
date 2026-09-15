# API reference

Source: `std/grpc.tc`

Module: `std.grpc`

## tc_grpc_response

```c
void* tc_grpc_response(string data, i32 status);
```

## tc_grpc_response_data

```c
string tc_grpc_response_data(void* response);
```

## tc_grpc_response_status

```c
i32 tc_grpc_response_status(void* response);
```

## tc_grpc_response_destroy

```c
void tc_grpc_response_destroy(void* response);
```

## tc_grpc_call

```c
void* tc_grpc_call(string host, i32 port, string method, string request, u64 timeout);
```

## tc_grpc_call_async

```c
void* tc_grpc_call_async(string host, i32 port, string method, string request, u64 timeout);
```

## tc_grpc_server_create

```c
void* tc_grpc_server_create(string host, i32 port, i32* error);
```

## tc_grpc_server_port

```c
i32 tc_grpc_server_port(void* server);
```

## tc_grpc_server_add

```c
i32 tc_grpc_server_add(void* server, string path, func<void*(string, void*)> handler, void* argument);
```

## tc_grpc_server_serve

```c
i32 tc_grpc_server_serve(void* server, i32 maximum_connections);
```

## tc_grpc_server_stop

```c
void tc_grpc_server_stop(void* server);
```

## tc_grpc_server_destroy

```c
void tc_grpc_server_destroy(void* server);
```

## GrpcResponse

Owns its response bytes. Destroy once; data() is a borrowed view.

```c
class GrpcResponse
```

### GrpcResponse.handle

```c
void* handle;
```

### GrpcResponse.create

```c
static GrpcResponse create(string data, i32 status = 0);
```

### GrpcResponse.data

```c
string data();
```

### GrpcResponse.status

```c
i32 status();
```

### GrpcResponse.destroy

```c
void destroy();
```

## GrpcClient

Unary, uncompressed gRPC over cleartext HTTP/2. Host is borrowed.

```c
class GrpcClient
```

### GrpcClient.host

```c
string host;
```

### GrpcClient.port

```c
i32 port;
```

### GrpcClient.call

```c
GrpcResponse call(string method, string request, u64 timeout = 30000);
```

### GrpcClient.callAsync

```c
async GrpcResponse callAsync(string method, string request, u64 timeout = 30000);
```

## GrpcServer

Register routes before serve(). A handler transfers its returned response.

```c
class GrpcServer
```

### GrpcServer.handle

```c
void* handle;
```

### GrpcServer.listen

```c
static (GrpcServer, i32) listen(string host, i32 port);
```

### GrpcServer.port

```c
i32 port();
```

### GrpcServer.add

```c
i32 add(string path, func<void*(string, void*)> handler, void* argument = null);
```

### GrpcServer.serve

```c
i32 serve(i32 maximumConnections = 0);
```

### GrpcServer.stop

```c
void stop();
```

### GrpcServer.destroy

```c
void destroy();
```

