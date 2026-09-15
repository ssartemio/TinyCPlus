# Protobuf y gRPC

`python tools/tiny-protoc.py input.proto -o output.tc` genera tipos de mensaje,
encode/decode/destroy, interfaces de servicio async, clientes y bindings.
`-I RUTA` agrega directorios de importación. No necesita protoc para generar
TinyC+; protoc/grpcio se usan solamente como implementaciones independientes en tests.

Se admite `syntax="proto3"`, escalares, strings y bytes, enums, mensajes,
mensajes anidados/recursivos, repeated, packed/unpacked, optional, maps, imports
y servicios unary. Se implementan varint, zigzag, fixed32/64 y IEEE754.
Los tags desconocidos se saltan, incluidos grupos legacy con límite de
profundidad. Mensajes singulares duplicados se fusionan.
Los campos desconocidos no se conservan al volver a serializar.

El generador rechaza proto2, oneof, RPC streaming y construcciones no soportadas
con un diagnóstico. No es un sustituto completo de protoc. Los nombres de tipos
generados deben ser únicos, debido al modelo actual de módulos del frontend.
Se necesita regenerar los bindings si cambia la versión de runtime/generador.

`Message.decode(bytes)` copia el wire buffer a almacenamiento propio; los campos
string/bytes son vistas dentro de ese buffer. `decodeView` presta el buffer
original. Los arrays y submensajes decodificados son propios. Llame destroy una
vez, incluso si decode reporta error, para limpiar el resultado parcial.
Al construir mensajes manualmente, sus strings/bytes son vistas prestadas;
deben vivir hasta terminar encode. `encode()` devuelve `(OwnedString,Error)`.
Valida UTF-8 para string, permite bytes arbitrarios y limita profundidad a 64.
El codec limita buffers a 64 MiB; el transporte limita mensajes a 4 MiB.

## Transporte

El runtime usa nghttp2 para framing HTTP/2, HPACK, multiplexación y control de
flujo. Encima implementa el prefijo gRPC de 5 bytes, POST con ruta de método,
content-type y trailers grpc-status. El estado se valida en el rango 0..16.
El mensaje es unary y no comprimido. Un status no cero se entrega como error
explícito al cliente generado. Los errores de tareas de servicio generan status 13.

El cliente crea una conexión por llamada. `callAsync` utiliza los dos workers
de I/O, no un hilo nuevo por RPC. El deadline se aplica a conectar e intercambiar
datos, aunque la resolución DNS síncrona del sistema puede superarlo.
No se envía todavía grpc-timeout al servidor; el deadline del cliente no cancela
automáticamente una función de servicio ya iniciada.

El servidor acepta conexiones y atiende cada una en un pool de cuatro workers;
admite hasta 32 conexiones activas y varios streams HTTP/2 por conexión.
`serve(0)` acepta indefinidamente; `serve(N)` acepta N conexiones. Los callbacks
se ejecutan en workers y deben sincronizar el estado compartido. Registre rutas
antes de serve. Llame stop, termine el hilo de serve y después destroy.
Un binding generado y su objeto de servicio deben vivir mientras se atienden RPCs.

Esta entrega usa **h2c, sin TLS**. No incluye compresión, reflexión, autenticación,
interceptores, metadata de aplicación, retry ni RPC streaming. Son ampliaciones
separadas; el roadmap permite posponer streaming y condiciona TLS a la integración
adecuada por FFI. El transporte entregado es apropiado para pruebas y conexiones
locales/controladas; no proporciona confidencialidad de transporte.

La prueba `test_grpc.py` usa grpcio como cliente y servidor externos al runtime:
mensajes vacíos, binarios y de 512 KiB, múltiples RPCs concurrentes sobre una
conexión, estados de error y timeout. `test_protobuf.py` contrasta todos los
tipos wire contra la biblioteca oficial y prueba UTF-8 inválido, overflows,
truncamientos, recursión excesiva y schemas rechazados.

Referencias de diseño: [protocolo gRPC HTTP/2](https://grpc.github.io/grpc/core/md_doc__p_r_o_t_o_c_o_l-_h_t_t_p2.html),
[codificación Protobuf](https://protobuf.dev/programming-guides/encoding/),
[nghttp2](https://nghttp2.org/documentation/programmers-guide.html).
