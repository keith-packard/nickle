/* $Header$ */

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

#include	<unistd.h>
#include	<fcntl.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include        <netinet/in.h>
#include	<netdb.h>
#include	<string.h>
#include	"builtin.h"
#include	<errno.h>

#define perror(s) FilePrintf(FileStderr, s ": %s\n", FileGetErrorMessage(errno))
#ifdef HAVE_HSTRERROR
#define herror(s) FilePrintf(FileStderr, s ": %s\n", hstrerror(h_errno))
#else
#define herror(s) FilePrintf(FileStderr, s ": network error %d\n", h_errno);
#endif

NamespacePtr SocketNamespace;
Type	     *typeSockaddr;

Value do_Socket_create (Value type);
Value do_Socket_connect (Value s, Value host, Value port);
Value do_Socket_bind (Value s, Value host, Value port);
Value do_Socket_listen (Value s, Value backlog);
Value do_Socket_accept (Value s);
Value do_Socket_shutdown (Value s, Value how);
Value do_Socket_gethostname (void);
Value do_Socket_getsockname (Value s);

void
import_Socket_namespace()
{
    ENTER ();

    static const struct fbuiltin_0 funcs_0[] = {
	{ do_Socket_gethostname, "gethostname", "s", "" },
	{ 0 },
    };

    static const struct fbuiltin_1 funcs_1[] = {
        { do_Socket_create, "create", "f", "i" },
        { do_Socket_accept, "accept", "f", "f" },
	{ do_Socket_getsockname, "getsockname", "a", "f" },
        { 0 }
    };

    static const struct fbuiltin_2 funcs_2[] = {
        { do_Socket_listen, "listen", "v", "fi" },
        { do_Socket_shutdown, "shutdown", "v", "fi" },
        { 0 }
    };

    static const struct fbuiltin_3 funcs_3[] = {
        { do_Socket_connect, "connect", "v", "fss" },
        { do_Socket_bind, "bind", "v", "fss" },
        { 0 }
    };

    SymbolPtr	sym;
    Type	*type;

    SocketNamespace = BuiltinNamespace (/*parent*/ 0, "Socket")->namespace.namespace;

    type = BuildStructType (2, 
			    typePrim[rep_integer], "addr",
			    typePrim[rep_integer], "port");
    
    sym = NewSymbolType (AtomId ("sockaddr"), type);

    typeSockaddr = NewTypeName (NewExprAtom (AtomId ("sockaddr"), 0, False),
				sym);
    
    NamespaceAddName (SocketNamespace, sym, publish_public);

    BuiltinFuncs0 (&SocketNamespace, funcs_0);
    BuiltinFuncs1 (&SocketNamespace, funcs_1);
    BuiltinFuncs2 (&SocketNamespace, funcs_2);
    BuiltinFuncs3 (&SocketNamespace, funcs_3);

    sym = BuiltinSymbol (&SocketNamespace, "SOCK_STREAM", typePrim[rep_int]);
    BoxValueSet (sym->global.value, 0, NewInt (SOCK_STREAM));
    sym = BuiltinSymbol (&SocketNamespace, "SOCK_DGRAM", typePrim[rep_int]);
    BoxValueSet (sym->global.value, 0, NewInt (SOCK_DGRAM));

    sym = BuiltinSymbol (&SocketNamespace, "SHUT_RD", typePrim[rep_int]);
    BoxValueSet (sym->global.value, 0, NewInt (SHUT_RD));
    sym = BuiltinSymbol (&SocketNamespace, "SHUT_WR", typePrim[rep_int]);
    BoxValueSet (sym->global.value, 0, NewInt (SHUT_WR));
    sym = BuiltinSymbol (&SocketNamespace, "SHUT_RDWR", typePrim[rep_int]);
    BoxValueSet (sym->global.value, 0, NewInt (SHUT_RDWR));

    sym = BuiltinSymbol (&SocketNamespace, "INADDR_ANY", typePrim[rep_string]);
    BoxValueSet (sym->global.value, 0, NewStrString ("0.0.0.0"));
    sym = BuiltinSymbol (&SocketNamespace, "INADDR_LOOPBACK", typePrim[rep_string]);
    BoxValueSet (sym->global.value, 0, NewStrString ("127.0.0.1"));
    sym = BuiltinSymbol (&SocketNamespace, "INADDR_BROADCAST", typePrim[rep_string]);
    BoxValueSet (sym->global.value, 0, NewStrString ("255.255.255.255"));

    EXIT ();
}

/* File::file do_Socket_create ({SOCK_STREAM,SOCK_DGRAM} type); */
Value
do_Socket_create (Value type)
{
    ENTER ();
    int itype, s;
    itype = IntPart (type, "Illegal socket type");
    if (aborting)
	RETURN (Void);
    s = socket (PF_INET, itype, 0);
    if (s == -1)
	RETURN (Void);
    RETURN (FileCreate (s, FileReadable|FileWritable));
}

static Bool address_lookup (Value hostname, Value portname,
			    struct sockaddr_in *addr)
{
    struct hostent *host;
    struct servent *port;
    char *hostchars;
    char *portchars;
    char *endptr = 0;
    long int portnum;

    hostchars = StrzPart (hostname, "invalid hostname");
    if (!hostchars)
	return False;
    portchars = StrzPart (portname, "invalid portname");
    if (!portchars)
	return False;
    
    if (*hostchars == '\0' || *portchars == '\0')
	return False; /* FIXME: more here? */

    addr->sin_family = AF_INET;

    /* host lookup */
    host = gethostbyname (hostchars);
    if (host == 0)
    {
	herror ("address_lookup");
	return False; /* FIXME: more here? */
    }
    memcpy (&addr->sin_addr.s_addr, host->h_addr_list[0], sizeof addr->sin_addr.s_addr);

    /* port lookup */
    portnum = strtol (portchars, &endptr, /* base */ 10);
    if (*endptr != '\0') /* non-numeric port specification */
    {
	/* FIXME: this should not always be "tcp"! */
	port = getservbyname (portchars, "tcp");
	if (port == 0)
	    return False; /* FIXME: more here? */
	addr->sin_port = port->s_port;
    }
    else
    {
	if (portnum <= 0 || portnum >= (1 << 16))
	    return False; /* FIXME: more here? */
	addr->sin_port = htons (portnum);
    }

    return True;
}

/* void do_Socket_connect (File::file s, String host, String port); */
Value
do_Socket_connect (Value s, Value host, Value port)
{
    ENTER ();
    struct sockaddr_in addr;

    if (!address_lookup (host, port, &addr))
	RETURN (Void);

    if (!running->thread.partial)
    {
	int flags = fcntl (s->file.fd, F_GETFL);
	int n, err;
	flags |= O_NONBLOCK;
	fcntl (s->file.fd, F_SETFL, flags);
	n = connect (s->file.fd, (struct sockaddr *) &addr, sizeof addr);
	flags &= ~O_NONBLOCK;
	fcntl (s->file.fd, F_SETFL, flags);
	err = errno;
	if (n == -1)
	{
	    if (err == EWOULDBLOCK || err == EINPROGRESS)
	    {
		FileSetBlocked (s, FileOutputBlocked);
		running->thread.partial = 1;
	    }
	    else
	    {
		RaiseStandardException (exception_io_error,
					FileGetErrorMessage (err),
					2, FileGetError (err),
					s);
		RETURN (Void);
	    }
	}
    }
    if (s->file.flags & FileOutputBlocked)
    {
	ThreadSleep (running, s, PriorityIo);
	RETURN (Void);
    }
    
    complete = True;
    RETURN (Void);
}

/* void do_Socket_bind (File::file s, String host, String port); */
Value
do_Socket_bind (Value s, Value host, Value port)
{
    ENTER ();
    struct sockaddr_in addr;

    if (!address_lookup (host, port, &addr))
	RETURN (Void);

#ifdef SO_REUSEADDR
    {
	int one = 1;
	setsockopt (s->file.fd, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof (int));
    }
#endif
    if (bind (s->file.fd, (struct sockaddr *) &addr, sizeof addr) == -1)
    {
	RaiseStandardException (exception_io_error,
				FileGetErrorMessage (errno),
				2, FileGetError (errno),
				s);
	RETURN (Void);
    }

    RETURN (Void);
}

/* void do_Socket_listen (File::file s, int backlog); */
Value
do_Socket_listen (Value s, Value backlog)
{
    ENTER ();
    int ibacklog;

    ibacklog = IntPart (backlog, "Illegal backlog length");
    if (aborting)
	RETURN (Void);

    if (listen (s->file.fd, ibacklog) == -1)
    {
	RETURN (Void); /* FIXME: more here? */
    }

    RETURN (Void);
}

/* File::file do_Socket_accept (File::file s); */
Value
do_Socket_accept (Value s)
{
    ENTER ();
    int f, err;
    int flags = fcntl (s->file.fd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl (s->file.fd, F_SETFL, flags);
    f = accept (s->file.fd, 0, 0);
    flags &= ~O_NONBLOCK;
    fcntl (s->file.fd, F_SETFL, flags);
    err = errno;
    if (f == -1)
    {
        if (err == EWOULDBLOCK || err == EAGAIN)
        {
	    FileSetBlocked (s, FileInputBlocked);
	    running->thread.partial = 1;
	}
	else
	{
	    RaiseStandardException (exception_io_error,
				    FileGetErrorMessage (err),
				    2, FileGetError (err),
				    s);
	    RETURN (Void);
	}
    }
    if (s->file.flags & FileInputBlocked)
    {
    	ThreadSleep (running, s, PriorityIo);
	RETURN (Void);
    }
	
    complete = True;
    RETURN (FileCreate (f, FileReadable|FileWritable));
}

/* void do_Socket_shutdown (File::file s, {SHUT_RD,SHUT_WR,SHUT_RDWR} how); */
Value
do_Socket_shutdown (Value s, Value how)
{
    ENTER ();
    int ihow;

    ihow = IntPart (how, "Illegal socket shutdown request");
    if (aborting)
	RETURN (Void);

    if (shutdown (s->file.fd, ihow) == -1)
    {
	RETURN (Void); /* FIXME: more here? */
    }

    RETURN (Void);
}

Value
do_Socket_gethostname (void)
{
    ENTER ();
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX	255
#endif
    char    hostname[HOST_NAME_MAX+1];

    if (gethostname (hostname, sizeof (hostname)) == -1)
    {
	RaiseStandardException (exception_io_error,
				FileGetErrorMessage (errno),
				2, FileGetError (errno),
				Void);
	RETURN (Void);
    }
    /* null termination is not promised */
    hostname[HOST_NAME_MAX] = '\0';
    RETURN (NewStrString (hostname));
}

Value
do_Socket_getsockname (Value s)
{
    ENTER ();
    struct sockaddr_in	addr;
    socklen_t		len = sizeof (addr);
    Value		ret;
    BoxPtr		box;

    if (getsockname (s->file.fd, (struct sockaddr *) &addr, &len) == -1)
    {
	RaiseStandardException (exception_io_error,
				FileGetErrorMessage (errno),
				2, FileGetError (errno),
				s);
	RETURN (Void);
    }
    ret = NewStruct (TypeCanon (typeSockaddr)->structs.structs, False);
    box = ret->structs.values;
    BoxValueSet (box, 0, NewInteger (Positive, 
				     NewNatural (ntohl (addr.sin_addr.s_addr))));
    BoxValueSet (box, 1, NewInt (ntohs (addr.sin_port)));
    RETURN (ret);
}
