
#ifndef SOCKET_WRAP_H
#define SOCKET_WRAP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _sw_socket sw_socket;

sw_socket * sw_connect(const char *host, int port); // creates and returns an outbount connected socket, or null if err
sw_socket * sw_listen(int port); // creates and returns a listening socket, or null if err
sw_socket * sw_accept(sw_socket *s); // creates and returns an inbound connected socket from a listening socket, or null if err
int sw_send(sw_socket *s, const void *msg, int len, int flags); // return length sent
int sw_recv(sw_socket *s, void *buf, int len, int flags); // return length received
int sw_close(sw_socket *s); // return 0 if no error

#ifdef __cplusplus
}
#endif

#endif /* SOCKET_WRAP_H  */