# API reference

Source: `std/net.tc`

Module: `std.net`

## tc_tcp_connect

```c
i64 tc_tcp_connect(string host, i32 port, i32* error);
```

## tc_tcp_listen

```c
i64 tc_tcp_listen(string host, i32 port, i32 backlog, i32* error);
```

## tc_tcp_accept

```c
i64 tc_tcp_accept(i64 listener, i32* error);
```

## tc_udp_bind

```c
i64 tc_udp_bind(string host, i32 port, i32* error);
```

## tc_udp_send

```c
i64 tc_udp_send(i64 handle, string host, i32 port, void* data, u64 size);
```

## tc_udp_receive

```c
i64 tc_udp_receive(i64 handle, void* data, u64 capacity, void* peer, u64 peer_capacity, i32* port);
```

## tc_socket_read

```c
i64 tc_socket_read(i64 handle, void* data, u64 capacity);
```

## tc_socket_write

```c
i64 tc_socket_write(i64 handle, void* data, u64 length);
```

## tc_socket_port

```c
i32 tc_socket_port(i64 handle);
```

## tc_socket_close

```c
i32 tc_socket_close(i64 handle);
```

## tc_dns_resolve

```c
string tc_dns_resolve(string host, i32* error);
```

## tc_socket_read_async

```c
void* tc_socket_read_async(i64 handle, void* data, u64 capacity, u64 timeout, void* token);
```

## tc_socket_write_async

```c
void* tc_socket_write_async(i64 handle, void* data, u64 length, u64 timeout, void* token);
```

## tc_tcp_accept_async

```c
void* tc_tcp_accept_async(i64 handle, u64 timeout, void* token);
```

## tc_tcp_connect_async

```c
void* tc_tcp_connect_async(string host, i32 port, u64 timeout, void* token);
```

## tc_file_read_async

```c
void* tc_file_read_async(string path);
```

## tc_file_write_async

```c
void* tc_file_write_async(string path, string contents);
```

## tc_dns_resolve_async

```c
void* tc_dns_resolve_async(string host);
```

## tc_timer_after

```c
void* tc_timer_after(u64 milliseconds);
```

## Address

```c
class Address
```

### Address.host

```c
string host;
```

### Address.port

```c
i32 port;
```

## Dns

```c
class Dns
```

### Dns.resolveAsync

Result owns its bytes. Wrap it in OwnedString and destroy once.

```c
static Task<string> resolveAsync(string host);
```

### Dns.resolve

```c
static (OwnedString, i32) resolve(string host);
```

## TcpSocket

A manually owned socket. Finish outstanding operations before close.

```c
class TcpSocket
```

### TcpSocket.handle

```c
i64 handle;
```

### TcpSocket.connect

```c
static (TcpSocket, i32) connect(string host, i32 port);
```

### TcpSocket.listen

```c
static (TcpSocket, i32) listen(string host, i32 port, i32 backlog = 32);
```

### TcpSocket.accept

```c
(TcpSocket, i32) accept();
```

### TcpSocket.port

```c
i32 port();
```

### TcpSocket.read

```c
i64 read(Slice<u8> destination);
```

### TcpSocket.write

```c
i64 write(Slice<u8> source);
```

### TcpSocket.close

```c
i32 close();
```

### TcpSocket.readAsync

```c
Task<i64> readAsync(Slice<u8> destination, u64 timeout = 30000, CancellationToken* token = null);
```

### TcpSocket.writeAsync

```c
Task<i64> writeAsync(Slice<u8> source, u64 timeout = 30000, CancellationToken* token = null);
```

### TcpSocket.acceptAsync

```c
async TcpSocket acceptAsync(u64 timeout = 30000, CancellationToken* token = null);
```

### TcpSocket.connectAsync

```c
static async TcpSocket connectAsync(string host, i32 port, u64 timeout = 30000, CancellationToken* token = null);
```

## UdpSocket

```c
class UdpSocket
```

### UdpSocket.handle

```c
i64 handle;
```

### UdpSocket.bind

```c
static (UdpSocket, i32) bind(string host, i32 port);
```

### UdpSocket.port

```c
i32 port();
```

### UdpSocket.sendTo

```c
i64 sendTo(Address peer, Slice<u8> source);
```

### UdpSocket.receive

```c
i64 receive(Slice<u8> destination);
```

### UdpSocket.receiveAsync

```c
Task<i64> receiveAsync(Slice<u8> destination, u64 timeout = 30000, CancellationToken* token = null);
```

### UdpSocket.close

```c
i32 close();
```

## AsyncFile

```c
class AsyncFile
```

### AsyncFile.writeAll

Copies the path and contents before submitting to the bounded I/O worker pool.

```c
static Task<void> writeAll(string path, string contents);
```

### AsyncFile.readAll

Result owns its bytes; wrap it in OwnedString and call destroy.

```c
static Task<string> readAll(string path);
```

