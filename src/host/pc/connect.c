# define INCLUDE_FILE_IO
# include "dgd.h"
# define FD_SETSIZE   1024
# include <winsock.h>
# include "str.h"
# include "array.h"
# include "object.h"
# include "comm.h"

struct _connection_ {
    SOCKET fd;				/* file descriptor */
    struct sockaddr_in addr;		/* internet address of connection */
    struct _connection_ *next;		/* next in list */
};

static int nusers;			/* # of users */
static connection *connections;		/* connections array */
static connection *flist;		/* list of free connections */
static SOCKET telnet;			/* telnet port socket descriptor */
static SOCKET binary;			/* binary port socket descriptor */
static fd_set fds;			/* file descriptor bitmap */
static fd_set waitfds;			/* file descriptor wait-write bitmap */
static fd_set readfds;			/* file descriptor read bitmap */
static fd_set writefds;			/* file descriptor write map */

/*
 * NAME:	conn->init()
 * DESCRIPTION:	initialize connection handling
 */
bool conn_init(int maxusers, unsigned int telnet_port, unsigned int binary_port)
{
    WSADATA wsadata;
    struct sockaddr_in sin;
    struct hostent *host;
    int n;
    connection *conn;
    char buffer[256];
    int on;

    /* initialize winsock */
    if (WSAStartup(MAKEWORD(1, 1), &wsadata) != 0) {
	P_message("WSAStartup failed (no winsock?)\n");
	return FALSE;
    }
    if (LOBYTE(wsadata.wVersion) != 1 || HIBYTE(wsadata.wVersion) != 1) {
	WSACleanup();
	P_message("Winsock 1.1 not supported\n");
	return FALSE;
    }

    gethostname(buffer, sizeof(buffer));
    host = gethostbyname(buffer);
    if (host == (struct hostent *) NULL) {
	P_message("gethostbyname() failed\n");
	return FALSE;
    }

    telnet = socket(host->h_addrtype, SOCK_STREAM, 0);
    binary = socket(host->h_addrtype, SOCK_STREAM, 0);
    if (telnet == INVALID_SOCKET || binary == INVALID_SOCKET) {
	P_message("socket() failed\n");
	return FALSE;
    }
    on = 1;
    if (setsockopt(telnet, SOL_SOCKET, SO_REUSEADDR, (char *) &on,
		   sizeof(on)) != 0) {
	P_message("setsockopt() failed\n");
	return FALSE;
    }
    on = 1;
    if (setsockopt(binary, SOL_SOCKET, SO_REUSEADDR, (char *) &on,
		   sizeof(on)) != 0) {
	P_message("setsockopt() failed\n");
	return FALSE;
    }

    memset(&sin, '\0', sizeof(sin));
    memcpy(&sin.sin_addr, host->h_addr, host->h_length);
    sin.sin_port = htons((u_short) telnet_port);
    sin.sin_family = host->h_addrtype;
    sin.sin_addr.s_addr = INADDR_ANY;
    if (bind(telnet, (struct sockaddr *) &sin, sizeof(sin)) != 0) {
	P_message("telnet bind failed\n");
	return FALSE;
    }
    sin.sin_port = htons((u_short) binary_port);
    if (bind(binary, (struct sockaddr *) &sin, sizeof(sin)) != 0) {
	P_message("binary bind failed\n");
	return FALSE;
    }

    flist = (connection *) NULL;
    connections = ALLOC(connection, nusers = maxusers);
    for (n = nusers, conn = connections; n > 0; --n, conn++) {
	conn->fd = INVALID_SOCKET;
	conn->next = flist;
	flist = conn;
    }

    FD_ZERO(&fds);
    FD_ZERO(&waitfds);
    FD_SET(telnet, &fds);
    FD_SET(binary, &fds);

    return TRUE;
}

/*
 * NAME:	conn->finish()
 * DESCRIPTION:	terminate connections
 */
void conn_finish(void)
{
    WSACleanup();
}

/*
 * NAME:	conn->listen()
 * DESCRIPTION:	start listening on telnet port and binary port
 */
void conn_listen(void)
{
    unsigned long nonblock;

    if (listen(telnet, 64) != 0 || listen(binary, 64) != 0) {
	fatal("listen() failed");
    }

    nonblock = TRUE;
    if (ioctlsocket(telnet, FIONBIO, &nonblock) != 0) {
	fatal("fcntl() failed");
    }
    nonblock = TRUE;
    if (ioctlsocket(binary, FIONBIO, &nonblock) != 0) {
	fatal("fcntl() failed");
    }
}

/*
 * NAME:	conn->tnew()
 * DESCRIPTION:	accept a new telnet connection
 */
connection *conn_tnew(void)
{
    SOCKET fd;
    int len;
    struct sockaddr_in sin;
    connection *conn;

    if (!FD_ISSET(telnet, &readfds)) {
	return (connection *) NULL;
    }
    len = sizeof(sin);
    fd = accept(telnet, (struct sockaddr *) &sin, &len);
    if (fd == INVALID_SOCKET) {
	return (connection *) NULL;
    }

    conn = flist;
    flist = conn->next;
    conn->fd = fd;
    memcpy(&conn->addr, (char *) &sin, len);
    FD_SET(fd, &fds);
    FD_CLR(fd, &readfds);
    FD_SET(fd, &writefds);

    return conn;
}

/*
 * NAME:	conn->bnew()
 * DESCRIPTION:	accept a new binary connection
 */
connection *conn_bnew(void)
{
    SOCKET fd;
    int len;
    struct sockaddr_in sin;
    connection *conn;

    if (!FD_ISSET(binary, &readfds)) {
	return (connection *) NULL;
    }
    len = sizeof(sin);
    fd = accept(binary, (struct sockaddr *) &sin, &len);
    if (fd == INVALID_SOCKET) {
	return (connection *) NULL;
    }

    conn = flist;
    flist = conn->next;
    conn->fd = fd;
    memcpy(&conn->addr, (char *) &sin, len);
    FD_SET(fd, &fds);
    FD_CLR(fd, &readfds);
    FD_SET(fd, &writefds);

    return conn;
}

/*
 * NAME:	conn->del()
 * DESCRIPTION:	delete a connection
 */
void conn_del(connection *conn)
{
    if (conn->fd != INVALID_SOCKET) {
	closesocket(conn->fd);
	FD_CLR(conn->fd, &fds);
	FD_CLR(conn->fd, &waitfds);
	conn->fd = INVALID_SOCKET;
    }
    conn->next = flist;
    flist = conn;
}

/*
 * NAME:	conn->select()
 * DESCRIPTION:	wait for input from connections
 */
int conn_select(int wait)
{
    struct timeval timeout;
    int retval;

    /*
     * First, check readability and writability for binary sockets with pending
     * data only.
     */
    memcpy(&readfds, &fds, sizeof(fd_set));
    memcpy(&writefds, &waitfds, sizeof(fd_set));
    timeout.tv_sec = (int) wait;
    timeout.tv_usec = 0;
    retval = select(0, &readfds, &writefds, (fd_set *) NULL, &timeout);
    /*
     * Now check writability for all sockets in a polling call.
     */
    memcpy(&writefds, &fds, sizeof(fd_set));
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    select(0, (fd_set *) NULL, &writefds, (fd_set *) NULL, &timeout);
    return retval;
}

/*
 * NAME:	conn->read()
 * DESCRIPTION:	read from a connection
 */
int conn_read(connection *conn, char *buf, unsigned int len)
{
    int size;

    if (conn->fd == INVALID_SOCKET) {
	return -1;
    }
    if (!FD_ISSET(conn->fd, &readfds)) {
	return 0;
    }
    size = recv(conn->fd, buf, len, 0);
    return (size == 0) ? -1 : size;
}

/*
 * NAME:	conn->write()
 * DESCRIPTION:	write to a connection; return the amount of bytes written
 */
int conn_write(connection *conn, char *buf, unsigned int len)
{
    int size;

    if (conn->fd != INVALID_SOCKET) {
	FD_CLR(conn->fd, &waitfds);
	if (len == 0) {
	    return 0;	/* send_message("") can be used to flush buffer */
	}
	if (!FD_ISSET(conn->fd, &writefds)) {
	    /* the write would fail */
	    FD_SET(conn->fd, &waitfds);
	    return -1;
	}
	if ((size=send(conn->fd, buf, len, 0)) == SOCKET_ERROR &&
	    WSAGetLastError() != WSAEWOULDBLOCK) {
	    closesocket(conn->fd);
	    FD_CLR(conn->fd, &fds);
	    conn->fd = INVALID_SOCKET;
	} else if ((unsigned int) size != len) {
	    /* waiting for wrdone */
	    FD_SET(conn->fd, &waitfds);
	    FD_CLR(conn->fd, &writefds);
	}
	return size;
    }
    return 0;
}

/*
 * NAME:	conn->wrdone()
 * DESCRIPTION:	return TRUE if a connection is ready for output
 */
bool conn_wrdone(connection *conn)
{
    if (conn->fd == INVALID_SOCKET || !FD_ISSET(conn->fd, &waitfds)) {
	return TRUE;
    }
    if (FD_ISSET(conn->fd, &writefds)) {
	FD_CLR(conn->fd, &waitfds);
	return TRUE;
    }
    return FALSE;
}

/*
 * NAME:	conn->ipnum()
 * DESCRIPTION:	return the ip number of a connection
 */
char *conn_ipnum(connection *conn)
{
    return inet_ntoa(conn->addr.sin_addr);
}