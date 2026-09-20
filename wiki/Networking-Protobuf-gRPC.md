# Networking, Protobuf y gRPC

## Networking

`std.net` cubre TCP, UDP, DNS y operaciones async.

### TCP

El API ofrece connect/listen/accept/read/write/close y variantes asíncronas.

Las lecturas pueden ser parciales. Un retorno cero indica EOF.

El reactor usa sockets no bloqueantes y `select`, con un límite acotado de
operaciones pendientes.

### UDP

`UdpSocket` permite bind, sendTo, receive y receiveAsync.

### DNS y archivos async

DNS y archivos usan workers de I/O separados del pool de continuaciones. La
resolución DNS del sistema no puede cancelarse una vez iniciada.

## Protobuf

Generar bindings:

```bash
python tools/tiny-protoc.py examples/users.proto -o examples/users.tc
```

Se soporta un subconjunto útil de proto3: escalares, strings/bytes, enums,
mensajes, repeated, packed, optional, maps, imports y servicios unary.

No se soportan proto2, oneof ni streaming RPC.

## gRPC

TinyC+ implementa gRPC unary sobre HTTP/2 **h2c** usando nghttp2.

Ejemplo representativo:

```c
import users;

class UserDirectory {
    async (UserResponse, Error) GetUser(UserRequest request) {
        UserResponse response;

        if (request.id != 42)
            return (response, 5);

        response.name = "Ana";
        response.roles.push("developer");
        return (response, 0);
    }
}
```

Cliente:

```c
async int getUser(int port)
{
    var client = UsersClient.connect("127.0.0.1", port);

    UserRequest request;
    request.id = 42;

    var response, error = await client.GetUser(request);
    defer response.destroy();

    if (error != 0)
        return error;

    println(response.name);
    println(response.roles[0]);
    return 0;
}
```

## Qué hace nghttp2

TinyC+ no reimplementa todo HTTP/2.

nghttp2 se encarga de framing, HPACK, multiplexación y flow control. El runtime
TinyC+ implementa encima el framing gRPC, rutas, content-type y grpc-status.

## Límites actuales

La entrega 1.0 RC no incluye:

```text
TLS
compresión
auth
reflection
metadata de aplicación
retry
RPC streaming
```

Por tanto, el transporte actual es adecuado para pruebas y redes
locales/controladas, no para prometer confidencialidad en Internet.

## Validación

Las pruebas contrastan Protobuf contra la biblioteca oficial y gRPC contra
grpcio como cliente y servidor independientes. Esto evita probar únicamente
cliente y servidor escritos con el mismo runtime.
