
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "socketWrap.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>

//----------------------------------------------------------------------------------
struct _sw_socket
{
	int s;
};

//----------------------------------------------------------------------------------
sw_socket * sw_connect(const char *host, int port) // creates and returns a connected socket, or null if err
{
	sw_socket *s = (sw_socket *) malloc(sizeof(sw_socket));
	if (!s) return 0;
	
	s->s = socket(AF_INET, SOCK_STREAM, 0);
	if (s->s == -1)
	{
		free(s);
		return 0;
	}
	
	struct sockaddr_in serv_addr;
    struct hostent *server;

	server = gethostbyname(host);
	if (!server)
	{
		printf("No such server!\n");
		close(s->s);
		free(s);
		return 0;
	}
	
	memset((void*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy((void *)&serv_addr.sin_addr.s_addr, (void *)server->h_addr, server->h_length);
	serv_addr.sin_port = htons(port);
	
	int err = connect(s->s, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (err)
	{
		printf("Could not connect (%i)!\n", err);
		close(s->s);
		free(s);
		return 0;
	}
	
	return s;
}

//----------------------------------------------------------------------------------
sw_socket * sw_listen(int port)
{
	sw_socket *s = (sw_socket *) malloc(sizeof(sw_socket));
	if (!s) return 0;

	s->s = socket(AF_INET, SOCK_STREAM, 0);
	if (s->s == -1)
	{
		free(s);
		return 0;
	}
	
	struct sockaddr_in serv_addr;
	memset((void*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	//serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	int err = bind(s->s, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
	if (err)
	{
		printf("Could not bind (%i %s %d)!\n", err, strerror(errno), errno);
		close(s->s);
		free(s);
		return 0;
	}
	
	err = listen(s->s, 5);
	if (err)
	{
		printf("Could not listen (%i)!\n", err);
		close(s->s);
		free(s);
		return 0;
	}

	return s;
}

//----------------------------------------------------------------------------------
sw_socket * sw_accept(sw_socket *s)
{
	if (!s->s) return 0;
	
	int newSocket = accept(s->s, 0, 0);
	if (newSocket == -1) return 0;

	sw_socket *ns = (sw_socket *) malloc(sizeof(sw_socket));
	if (!ns) return 0;
	
	ns->s = newSocket;
	return ns;
}

//----------------------------------------------------------------------------------
int sw_send(sw_socket *s, const void *msg, int len, int flags) // return length sent
{
	return send(s->s, msg, len, flags);
}

//----------------------------------------------------------------------------------
int sw_recv(sw_socket *s, void *buf, int len, int flags) // return length received
{
	return recv(s->s, buf, len, flags);
}

//----------------------------------------------------------------------------------
int sw_close(sw_socket *s) // return 0 if no error
{
	int err = close(s->s);
	free(s);
	return err;
}
