/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	sockets.c
 *
 *	provide builtin functions for the Sockets namespace
 */

#include	<sys/types.h>
#include	<sys/socket.h>
#include        <netinet/in.h>
#include	<netdb.h>
#include	<string.h>
#include	"builtin.h"
#include	<errno.h>

#define perror(s) FilePrintf(FileStderr, s ": %s\n", strerror(errno))
#define herror(s) FilePrintf(FileStderr, s ": %s\n", hstrerror(h_errno))

NamespacePtr SocketsNamespace;

Value do_Sockets_create (Value type);
Value do_Sockets_connect (Value s, Value host, Value port);
Value do_Sockets_bind (Value s, Value host, Value port);
Value do_Sockets_listen (Value s, Value backlog);
Value do_Sockets_accept (Value s);
Value do_Sockets_shutdown (Value s, Value how);

void
import_Sockets_namespace()
{
    ENTER ();
    static struct fbuiltin_1 funcs_1[] = {
        { do_Sockets_create, "create", "f", "i" },
        { do_Sockets_accept, "accept", "f", "f" },
        { 0 }
    };

    static struct fbuiltin_2 funcs_2[] = {
        { do_Sockets_listen, "listen", "v", "fi" },
        { do_Sockets_shutdown, "shutdown", "v", "fi" },
        { 0 }
    };

    static struct fbuiltin_3 funcs_3[] = {
        { do_Sockets_connect, "connect", "v", "fss" },
        { do_Sockets_bind, "bind", "v", "fss" },
        { 0 }
    };

    SymbolPtr sym;

    SocketsNamespace = BuiltinNamespace (/*parent*/ 0, "Sockets")->namespace.namespace;

    BuiltinFuncs1 (&SocketsNamespace, funcs_1);
    BuiltinFuncs2 (&SocketsNamespace, funcs_2);
    BuiltinFuncs3 (&SocketsNamespace, funcs_3);

    sym = BuiltinSymbol (&SocketsNamespace, "SOCK_STREAM", typesPrim[type_int]);
    BoxValueSet (sym->global.value, 0, NewInt (SOCK_STREAM));
    sym = BuiltinSymbol (&SocketsNamespace, "SOCK_DGRAM", typesPrim[type_int]);
    BoxValueSet (sym->global.value, 0, NewInt (SOCK_DGRAM));

    sym = BuiltinSymbol (&SocketsNamespace, "SHUT_RD", typesPrim[type_int]);
    BoxValueSet (sym->global.value, 0, NewInt (SHUT_RD));
    sym = BuiltinSymbol (&SocketsNamespace, "SHUT_WR", typesPrim[type_int]);
    BoxValueSet (sym->global.value, 0, NewInt (SHUT_WR));
    sym = BuiltinSymbol (&SocketsNamespace, "SHUT_RDWR", typesPrim[type_int]);
    BoxValueSet (sym->global.value, 0, NewInt (SHUT_RDWR));

    sym = BuiltinSymbol (&SocketsNamespace, "INADDR_ANY", typesPrim[type_string]);
    BoxValueSet (sym->global.value, 0, NewStrString ("0.0.0.0"));
    sym = BuiltinSymbol (&SocketsNamespace, "INADDR_LOOPBACK", typesPrim[type_string]);
    BoxValueSet (sym->global.value, 0, NewStrString ("127.0.0.1"));
    sym = BuiltinSymbol (&SocketsNamespace, "INADDR_BROADCAST", typesPrim[type_string]);
    BoxValueSet (sym->global.value, 0, NewStrString ("255.255.255.255"));

    EXIT ();
}

/* File::file do_Sockets_create ({SOCK_STREAM,SOCK_DGRAM} type); */
Value
do_Sockets_create (Value type)
{
    ENTER ();
    int itype, s;
    itype = IntPart (type, "Illegal socket type");
    if (aborting)
	RETURN (Zero);
    s = socket (PF_INET, itype, 0);
    if (s == -1)
	RETURN (Zero);
    RETURN (FileCreate (s));
}

static Bool address_lookup (Value hostname, Value portname,
			    struct sockaddr_in *addr)
{
    struct hostent *host;
    struct servent *port;
    char *hostchars = StringChars (&hostname->string);
    char *portchars = StringChars (&portname->string);
    char *endptr = 0;
    long int portnum;

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
	port = getservbyname (StringChars (&portname->string), "tcp");
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

/* void do_Sockets_connect (File::file s, String host, String port); */
Value
do_Sockets_connect (Value s, Value host, Value port)
{
    ENTER ();
    struct sockaddr_in addr;

    if (!address_lookup (host, port, &addr))
	RETURN (Zero);

    if (!running->thread.partial)
    {
	if (connect (s->file.fd, (struct sockaddr *) &addr, sizeof addr) == -1)
	{
	    if (errno == EWOULDBLOCK || errno == EINPROGRESS)
	    {
		FileSetBlocked (s, FileOutputBlocked);
		running->thread.partial = 1;
	    }
	    else
		RETURN (Zero); /* FIXME: more here? */
	}
    }
    if (s->file.flags & FileOutputBlocked)
    {
	ThreadSleep (running, s, PriorityIo);
	RETURN (Zero);
    }
    
    complete = True;
    RETURN (One);
}

/* void do_Sockets_bind (File::file s, String host, String port); */
Value
do_Sockets_bind (Value s, Value host, Value port)
{
    ENTER ();
    struct sockaddr_in addr;

    if (!address_lookup (host, port, &addr))
	RETURN (Zero);

    if (bind (s->file.fd, (struct sockaddr *) &addr, sizeof addr) == -1)
	RETURN (Zero); /* FIXME: more here? */

    RETURN (One);
}

/* void do_Sockets_listen (File::file s, int backlog); */
Value
do_Sockets_listen (Value s, Value backlog)
{
    ENTER ();
    int ibacklog;

    ibacklog = IntPart (backlog, "Illegal backlog length");
    if (aborting)
	RETURN (Zero);

    if (listen (s->file.fd, ibacklog) == -1)
    {
	RETURN (Zero); /* FIXME: more here? */
    }

    RETURN (One);
}

/* File::file do_Sockets_accept (File::file s); */
Value
do_Sockets_accept (Value s)
{
    ENTER ();
    int f;

    f = accept (s->file.fd, 0, 0);
    if (f == -1)
    {
	if (errno == EWOULDBLOCK)
	{
	    FileSetBlocked (s, FileInputBlocked);
	    ThreadSleep (running, s, PriorityIo);
	}
        RETURN (Zero); /* FIXME: more here? */
    }

    complete = True;
    RETURN (FileCreate (f));
}

/* void do_Sockets_shutdown (File::file s, {SHUT_RD,SHUT_WR,SHUT_RDWR} how); */
Value
do_Sockets_shutdown (Value s, Value how)
{
    ENTER ();
    int ihow;

    ihow = IntPart (how, "Illegal socket shutdown request");
    if (aborting)
	RETURN (Zero);

    if (shutdown (s->file.fd, ihow) == -1)
    {
	RETURN (Zero); /* FIXME: more here? */
    }

    RETURN (One);
}
