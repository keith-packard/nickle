/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	sockets.c
 *
 *	provide builtin functions for the Socket namespace
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#ifndef SUN_LEN
#define SUN_LEN(ptr) (sizeof(*(ptr)) - sizeof((ptr)->sun_path) + strlen((ptr)->sun_path) + 1)
#endif
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include "builtin.h"
#include <errno.h>

static NamespacePtr SocketNamespace;
Type               *typeSockaddr;

Value               do_Socket_create(ptrdiff_t num, Value *args);
Value               do_Socket_connect(ptrdiff_t num, Value *args);
Value               do_Socket_bind(ptrdiff_t num, Value *args);
Value               do_Socket_listen(Value s, Value backlog);
Value               do_Socket_accept(Value s);
Value               do_Socket_shutdown(Value s, Value how);
Value               do_Socket_gethostname(void);
Value               do_Socket_getsockname(Value s);

void
import_Socket_namespace(void)
{
    ENTER();

    static const struct fbuiltin_0 funcs_0[] = {
        { do_Socket_gethostname, "gethostname", "s", "",
         "\n"
          " string gethostname ()\n"
          "\n"
          " Get the current hostname.\n" },
        { 0 },
    };

    static const struct fbuiltin_1 funcs_1[] = {
        { do_Socket_accept, "accept", "f", "f",
         "\n"
          " file accept (file listen)\n"
          "\n"
          " Return a socket for the next connection on 'listen'.\n" },
        { do_Socket_getsockname, "getsockname", "a", "f",
         "\n"
          " sockaddr getsockname (file socket)\n"
          "\n"
          " Returns the address and port of 'socket'.\n" },
        { 0 }
    };

    static const struct fbuiltin_2 funcs_2[] = {
        { do_Socket_listen, "listen", "v", "fi",
         "\n"
          " void listen (file socket, int length)\n"
          "\n"
          " Establish a listen queue on 'f' of length 'i' (max 5).\n" },
        { do_Socket_shutdown, "shutdown", "v", "fi",
         "\n"
          " void shutdown (file socket, int dir)\n"
          "\n"
          " Shut communication in 'dir' direction:\n"
          "   SHUT_RD:    shut down reading.\n"
          "   SHUT_WR:    shut down writing.\n"
          "   SHUT_RDWR:  shut down reading and writing.\n" },
        { 0 }
    };

    static const struct fbuiltin_v funcs_v[] = {
        { do_Socket_create, "create", "f", ".i",
         "\n"
          " file create ([int family], int type)\n"
          "\n"
          " Create a socket where the optional 'family' is one of:\n"
          "   AF_UNIX:		Local communication.\n"
          "   AF_INET (default):	IPv4 Internet protocols.\n"
          "   AF_INET6:		IPv6 Internet protocols.\n"
          " and where 'type' is one of:\n"
          "   SOCK_STREAM:		a stream socket.\n"
          "   SOCK_DGRAM:		a datagram socket.\n" },
        { do_Socket_bind, "bind", "v", "f.p",
         "\n"
          " void bind (file socket, string host, string port)\n"
          " void bind (file socket, string host, int port)\n"
          "\n"
          " Bind AF_INET 'socket' to 'host', 'port'.\n"
          "\n"
          " void bind (file socket, string local_socket)\n"
          "\n"
          " Bind AF_UNIX 'socket' to 'localhost'.\n" },
        { do_Socket_connect, "connect", "v", "f.p",
         "\n"
          " void connect (file socket, string host, string port)\n"
          " void connect (file socket, string host, int port)\n"
          "\n"
          " Connect AF_INET 'socket' to 'host', 'port'.\n"
          "\n"
          " void connect (file socket, string local_socket)\n"
          "\n"
          " Connect AF_UNIX 'socket' to 'local_socket'.\n" },
        { 0 }
    };

    static const struct ibuiltin ivars[] = {
        { AF_UNIX, "AF_UNIX", &SocketNamespace },
        { AF_INET, "AF_INET", &SocketNamespace },
#ifdef AF_INET6
        { AF_INET6, "AF_INET6", &SocketNamespace },
#endif
        { SOCK_STREAM, "SOCK_STREAM", &SocketNamespace },
        { SOCK_DGRAM, "SOCK_DGRAM", &SocketNamespace },
        { SHUT_RD, "SHUT_RD", &SocketNamespace },
        { SHUT_WR, "SHUT_WR", &SocketNamespace },
        { SHUT_RDWR, "SHUT_RDWR", &SocketNamespace },
        { 0 }
    };

    static const struct sbuiltin svars[] = {
        { "0.0.0.0", "INADDR_ANY", &SocketNamespace },
        { "127.0.0.1", "INADDR_LOOPBACK", &SocketNamespace },
        { "255.255.255.255", "INADDR_BROADCAST", &SocketNamespace },
        { 0 }
    };
    SymbolPtr sym;
    Type     *type;

    SocketNamespace = BuiltinNamespace(/*parent*/ 0, "Socket")->namespace.namespace;

    type = BuildStructType(2, typePrim[rep_integer], "addr", typePrim[rep_integer], "port");

    sym = NewSymbolType(AtomId("sockaddr"), type);

    typeSockaddr = NewTypeName(NewExprAtom(AtomId("sockaddr"), 0, False), sym);

    NamespaceAddName(SocketNamespace, sym, publish_public);

    BuiltinFuncs0(&SocketNamespace, funcs_0);
    BuiltinFuncs1(&SocketNamespace, funcs_1);
    BuiltinFuncs2(&SocketNamespace, funcs_2);
    BuiltinFuncsV(&SocketNamespace, funcs_v);

    BuiltinIntegers(ivars);
    BuiltinStrings(svars);

    EXIT();
}

/* File::file do_Socket_create ({SOCK_STREAM,SOCK_DGRAM} type); */
Value
do_Socket_create(ptrdiff_t num, Value *args)
{
    ENTER();
    int   ifamily, itype, type_index, s;
    Value ret;

    if (num == 0 || num > 2) {
        RaiseStandardException(exception_invalid_argument, 3,
                               NewStrString("create must have one or two arguments"), NewInt(0),
                               NewPtrdiffInteger(num));
        RETURN(Void);
    }

    if (num > 1) {
        ifamily = IntPart(args[0], "Illegal address family");
        type_index = 1;
    } else {
        ifamily = AF_INET;
        type_index = 0;
    }
    itype = IntPart(args[type_index], "Illegal socket type");
    if (aborting)
        RETURN(Void);
    s = socket(ifamily, itype, 0);
    if (s == -1)
        RETURN(Void);
    ret = FileCreate(s, FileReadable | FileWritable);
    ret->file.sock_family = ifamily;
    RETURN(ret);
}

#ifdef PATH_MAX
#define UN_SOCK_MAX PATH_MAX
#else
#define UN_SOCK_MAX 4096
#endif

typedef union {
    struct sockaddr_in in;
    struct sockaddr_un un;
    struct sockaddr    addr;
    struct {
        struct sockaddr_un un;
        char               path[UN_SOCK_MAX];
    } align;
} sockaddr_all_t;

#define VerifyArgumentCount(arg, condition, error)                                            \
    if (!(condition)) {                                                                       \
        RaiseStandardException(exception_invalid_argument, 3, NewStrString(error), NewInt(0), \
                               NewPtrdiffInteger(arg));                                       \
    }

/*
 * Raise invalid_argument for an address bind or connect can't use
 */
static Bool
address_error(char *message, Value value)
{
    RaiseStandardException(exception_invalid_argument, 3, NewStrString(message), NewInt(0), value);
    return False;
}

/* Supports the following args from both bind and connect:
 *	(File::file s, String local_socket)
 */
static Bool
address_lookup_af_unix(ptrdiff_t num, Value *args, struct sockaddr_un *addr, socklen_t *len)
{
    char *local_socket;
    Value s = args[0];

    VerifyArgumentCount(num, num == 2, "must have 2 arguments for an AF_UNIX socket");
    if (aborting)
        return False;

    local_socket = StrzPart(args[1], "invalid local_socket");
    if (!local_socket)
        return False;
    if (*local_socket == '\0')
        return address_error("empty local_socket", args[1]);
    if (strlen(local_socket) > UN_SOCK_MAX)
        return address_error("local_socket too long", args[1]);

    addr->sun_family = (sa_family_t)s->file.sock_family;
    strcpy(addr->sun_path, local_socket);
    *len = (socklen_t)SUN_LEN(addr);

    return True;
}

/* Supports the following args from both bind and connect:
 *	(File::file s, String host, String port)
 *	(File::file s, String host, int port)
 * Port 0 asks bind for any free port, so only bind passes any_port
 */
static Bool
address_lookup_af_inet(ptrdiff_t num, Value *args, struct sockaddr_in *addr, socklen_t *len,
                       Bool any_port)
{
    Value           host, port;
    char           *hostchars;
    struct hostent *hostent;
    long int        portnum;
    struct servent *portent;
    char           *endptr;
    Value           s = args[0];

    VerifyArgumentCount(num, num == 3, "must have 3 arguments for an AF_INET socket");
    if (aborting)
        return False;

    host = args[1];
    port = args[2];

    hostchars = StrzPart(host, "invalid hostname");
    if (!hostchars)
        return False;
    if (*hostchars == '\0')
        return address_error("empty hostname", host);

    if (ValueIsString(port)) {
        char *portchars = StrzPart(port, "invalid port string");
        if (!portchars)
            return False;
        if (*portchars == '\0')
            return address_error("empty port", port);
        portnum = strtol(portchars, &endptr, /* base */ 10);
        if (*endptr != '\0') /* non-numeric port specification */
        {
            /* FIXME: this should not always be "tcp"! */
            portent = getservbyname(portchars, "tcp");
            if (portent == 0)
                return address_error("unknown service", port);
            /* s_port holds the port in network byte order */
            portnum = ntohs((uint16_t)portent->s_port);
        }
    } else {
        portnum = IntPart(port, "invalid port value");
        if (aborting)
            return False;
    }
    if (portnum < 0 || portnum >= (1 << 16) || (portnum == 0 && !any_port))
        return address_error("port out of range", port);

    addr->sin_family = (sa_family_t)s->file.sock_family;
    addr->sin_port = htons((uint16_t)portnum);

    /* host lookup */
    hostent = gethostbyname(hostchars);
    if (hostent == 0)
        return address_error("unknown host", host);

    *len = sizeof(*addr);
    memcpy(&addr->sin_addr.s_addr, hostent->h_addr_list[0], sizeof(addr->sin_addr.s_addr));

    return True;
}

/* Supports the following args from both bind and connect:
 *
 *	(File::file s, String host, String port)
 *	(File::file s, String host, int port)
 *	(File::file s, String local_socket)
 */
static Bool
address_lookup(ptrdiff_t num, Value *args, sockaddr_all_t *addr, socklen_t *len, Bool any_port)
{
    Value s = args[0];

    switch (s->file.sock_family) {
    case AF_UNIX:
        return address_lookup_af_unix(num, args, &addr->un, len);
    case AF_INET:
        return address_lookup_af_inet(num, args, &addr->in, len, any_port);
#ifdef AF_INET6
    case AF_INET6:
        /* FIXME */
#endif
    default:
        return address_error("unsupported address family", s);
    }
}

/* void do_Socket_connect (File::file s, String host, String port);
 * void do_Socket_connect (File::file s, String host, int port);
 * void do_Socket_connect (File::file s, String local_socket;
 */
Value
do_Socket_connect(ptrdiff_t num, Value *args)
{
    ENTER();
    sockaddr_all_t addr;
    socklen_t      len;
    Value          s = args[0];

    if (!address_lookup(num, args, &addr, &len, False))
        RETURN(Void);

    if (!running->thread.partial) {
        int flags = fcntl(s->file.fd, F_GETFL);
        int n, err;
        flags |= O_NONBLOCK;
        fcntl(s->file.fd, F_SETFL, flags);
#ifdef SO_BROADCAST
        {
            int one = 1;
            setsockopt(s->file.fd, SOL_SOCKET, SO_BROADCAST, (char *)&one, sizeof(int));
        }
#endif
        n = connect(s->file.fd, &addr.addr, len);
        err = errno;
        flags &= ~O_NONBLOCK;
        fcntl(s->file.fd, F_SETFL, flags);
        if (n == -1) {
            if (err == EWOULDBLOCK || err == EINPROGRESS) {
                FileSetBlocked(s, FileOutputBlocked);
                running->thread.partial = 1;
            } else {
                RaiseStandardException(exception_io_error, 3, FileGetErrorMessage(err),
                                       FileGetError(err), s);
                RETURN(Void);
            }
        }
    }
    if (s->file.flags & FileOutputBlocked) {
        ThreadSleep(running, s);
        RETURN(Void);
    }

    /* A connection that had to wait reports how it ended through SO_ERROR */
    if (running->thread.partial) {
        int       err = 0;
        socklen_t errlen = sizeof(err);

        if (getsockopt(s->file.fd, SOL_SOCKET, SO_ERROR, &err, &errlen) < 0)
            err = errno;
        if (err) {
            /* Only a completed instruction clears partial */
            running->thread.partial = 0;
            RaiseStandardException(exception_io_error, 3, FileGetErrorMessage(err),
                                   FileGetError(err), s);
            RETURN(Void);
        }
    }

    complete = True;
    RETURN(Void);
}

/* void do_Socket_bind (File::file s, String host, String port);
 * void do_Socket_bind (File::file s, String host, int port);
 * void do_Socket_bind (File::file s, String local_socket;
 */
Value
do_Socket_bind(ptrdiff_t num, Value *args)
{
    ENTER();
    sockaddr_all_t addr;
    socklen_t      len;
    Value          s = args[0];

    if (!address_lookup(num, args, &addr, &len, True))
        RETURN(Void);

#ifdef SO_REUSEADDR
    {
        int one = 1;
        setsockopt(s->file.fd, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(int));
    }
#endif
    if (bind(s->file.fd, &addr.addr, len) == -1) {
        RaiseStandardException(exception_io_error, 3, FileGetErrorMessage(errno),
                               FileGetError(errno), s);
        RETURN(Void);
    }

    RETURN(Void);
}

/* void do_Socket_listen (File::file s, int backlog); */
Value
do_Socket_listen(Value s, Value backlog)
{
    ENTER();
    int ibacklog;

    ibacklog = IntPart(backlog, "Illegal backlog length");
    if (aborting)
        RETURN(Void);

    if (listen(s->file.fd, ibacklog) == -1)
        RaiseStandardException(exception_io_error, 3, FileGetErrorMessage(errno),
                               FileGetError(errno), s);

    RETURN(Void);
}

/* File::file do_Socket_accept (File::file s); */
Value
do_Socket_accept(Value s)
{
    ENTER();
    int f, err;
    int flags = fcntl(s->file.fd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(s->file.fd, F_SETFL, flags);
    f = accept(s->file.fd, 0, 0);
    flags &= ~O_NONBLOCK;
    fcntl(s->file.fd, F_SETFL, flags);
    err = errno;
    if (f == -1) {
        if (err == EWOULDBLOCK || err == EAGAIN) {
            FileSetBlocked(s, FileInputBlocked);
            running->thread.partial = 1;
        } else {
            RaiseStandardException(exception_io_error, 3, FileGetErrorMessage(err),
                                   FileGetError(err), s);
            RETURN(Void);
        }
    }
    if (s->file.flags & FileInputBlocked) {
        ThreadSleep(running, s);
        RETURN(Void);
    }

    complete = True;
    RETURN(FileCreate(f, FileReadable | FileWritable));
}

/* void do_Socket_shutdown (File::file s, {SHUT_RD,SHUT_WR,SHUT_RDWR} how); */
Value
do_Socket_shutdown(Value s, Value how)
{
    ENTER();
    int ihow;

    ihow = IntPart(how, "Illegal socket shutdown request");
    if (aborting)
        RETURN(Void);

    if (shutdown(s->file.fd, ihow) == -1)
        RaiseStandardException(exception_io_error, 3, FileGetErrorMessage(errno),
                               FileGetError(errno), s);

    RETURN(Void);
}

Value
do_Socket_gethostname(void)
{
    ENTER();
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif
    char hostname[HOST_NAME_MAX + 1];

    if (gethostname(hostname, sizeof(hostname)) == -1) {
        RaiseStandardException(exception_io_error, 3, FileGetErrorMessage(errno),
                               FileGetError(errno), Void);
        RETURN(Void);
    }
    /* null termination is not promised */
    hostname[HOST_NAME_MAX] = '\0';
    RETURN(NewStrString(hostname));
}

Value
do_Socket_getsockname(Value s)
{
    ENTER();
    struct sockaddr_in addr;
    socklen_t          len = sizeof(addr);
    Value              ret;
    BoxPtr             box;

    if (getsockname(s->file.fd, (struct sockaddr *)&addr, &len) == -1) {
        RaiseStandardException(exception_io_error, 3, FileGetErrorMessage(errno),
                               FileGetError(errno), s);
        RETURN(Void);
    }
    ret = NewStruct(TypeCanon(typeSockaddr)->structs.structs, False);
    box = ret->structs.values;
    BoxValueSet(box, 0, NewInteger(Positive, NewNatural(ntohl(addr.sin_addr.s_addr))));
    BoxValueSet(box, 1, NewInt(ntohs(addr.sin_port)));
    RETURN(ret);
}
