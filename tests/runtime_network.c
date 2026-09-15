#include "../runtime/tiny_runtime.h"
#include "../runtime/concurrent.c"
#include "../runtime/net.c"
#include <assert.h>

int main(void) {
    int32_t error = 0;
    int64_t listener, client, accepted, count, udp_a, udp_b;
    void *accepting, *connecting, *reading, *writing, *token, *timer;
    char received[16] = {0};
    const char payload[] = "a\0binary";
    TinyString address, file;
    uint64_t start;
    tc_scheduler_start(1);
    tc_io_start();
    listener = tc_tcp_listen(TC_STRING("127.0.0.1"), 0, 4, &error);
    assert(error == 0 && listener >= 0);
    accepting = tc_tcp_accept_async(listener, 2000, NULL);
    connecting = tc_tcp_connect_async(TC_STRING("127.0.0.1"), tc_socket_port(listener), 2000, NULL);
    assert(tc_future_get(connecting, &client) == 0);
    assert(tc_future_get(accepting, &accepted) == 0);
    tc_future_release(connecting);
    tc_future_release(accepting);
    reading = tc_socket_read_async(accepted, received, sizeof(received), 2000, NULL);
    writing = tc_socket_write_async(client, (void *)payload, sizeof(payload), 2000, NULL);
    assert(tc_future_get(writing, &count) == 0 && count == sizeof(payload));
    assert(tc_future_get(reading, &count) == 0 && count == sizeof(payload));
    assert(memcmp(received, payload, sizeof(payload)) == 0);
    tc_future_release(writing);
    tc_future_release(reading);
    start = tc_clock_ms();
    reading = tc_socket_read_async(accepted, received, sizeof(received), 20, NULL);
    assert(tc_future_get(reading, &count) == 4);
    assert(tc_clock_ms() - start < 2000);
    tc_future_release(reading);
    token = tc_atomic_create(0);
    reading = tc_socket_read_async(accepted, received, sizeof(received), 2000, token);
    tc_atomic_store(token, 1);
    assert(tc_future_get(reading, &count) == 1);
    tc_future_release(reading);
    tc_atomic_destroy(token);
    assert(tc_socket_close(client) == 0);
    reading = tc_socket_read_async(accepted, received, sizeof(received), 2000, NULL);
    assert(tc_future_get(reading, &count) == 0 && count == 0);
    tc_future_release(reading);
    tc_socket_close(accepted);
    tc_socket_close(listener);
    udp_a = tc_udp_bind(TC_STRING("127.0.0.1"), 0, &error);
    assert(error == 0);
    udp_b = tc_udp_bind(TC_STRING("127.0.0.1"), 0, &error);
    assert(error == 0);
    assert(tc_udp_send(udp_a, TC_STRING("127.0.0.1"), tc_socket_port(udp_b), (void *)payload,
                       sizeof(payload)) == sizeof(payload));
    assert(tc_udp_receive(udp_b, received, sizeof(received), NULL, 0, &error) == sizeof(payload));
    assert(memcmp(received, payload, sizeof(payload)) == 0);
    tc_socket_close(udp_a);
    tc_socket_close(udp_b);
    address = tc_dns_resolve(TC_STRING("localhost"), &error);
    assert(error == 0 && address.length > 0);
    tc_string_free(address);
    reading = tc_file_read_async(TC_STRING("build/does-not-exist-78192"));
    assert(tc_future_get(reading, &file) != 0);
    tc_future_release(reading);
    timer = tc_timer_after(1);
    assert(tc_future_get(timer, NULL) == 0);
    tc_future_release(timer);
    tc_io_shutdown();
    tc_scheduler_shutdown();
    puts("Network verified: TCP binary transfer, deadlines, cancellation, EOF, UDP, DNS, file "
         "errors, timer");
    return 0;
}
