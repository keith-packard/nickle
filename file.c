/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	<unistd.h>
#include	<fcntl.h>
#include	<signal.h>
#include	<sys/time.h>
#include	<sys/types.h>
#include	<sys/wait.h>
#include	<errno.h>
#include	"nickle.h"
#include	"ref.h"
#include	"gram.h"

#ifdef O_NONBLOCK
#define NOBLOCK	O_NONBLOCK
#else
#define NOBLOCK O_NDELAY
#endif

#ifdef O_ASYNC
#define ASYNC O_ASYNC
#else
#ifdef HAVE_STROPTS_H
#define USE_STREAMS_ASYNC
#define ASYNC 0
#include <stropts.h>
#endif
#endif

ReferencePtr	fileBlockedReference;
Value		fileBlocked;
Bool		anyFileWriteBlocked;
Bool		anyFileReadBlocked;
extern Bool	ownTty[3];

typedef struct _FileErrorMap {
    int		value;
    char	*name;
    char	*message;
    Atom	atom;
} FileErrorMap;

FileErrorMap   fileErrorMap[] = {
    { EPERM, "PERM", "Operation not permitted", 0 },
    { ENOENT, "NOENT", "No such file or directory", 0 },
    { ESRCH, "SRCH", "No such process", 0 },
    { EINTR, "INTR", "Interrupted system call", 0 },
    { EIO, "IO", "I/O error", 0 },
    { ENXIO, "NXIO", "No such device or address", 0 },
    { E2BIG, "2BIG", "Arg list too long", 0 },
    { ENOEXEC, "NOEXEC", "Exec format error", 0 },
    { EBADF, "BADF", "Bad file number", 0 },
    { ECHILD, "CHILD", "No child processes", 0 },
    { EAGAIN, "AGAIN", "Try again", 0 },
    { ENOMEM, "NOMEM", "Out of memory", 0 },
    { EACCES, "ACCES", "Permission denied", 0 },
    { EFAULT, "FAULT", "Bad address", 0 },
    { ENOTBLK, "NOTBLK", "Block device required", 0 },
    { EBUSY, "BUSY", "Device or resource busy", 0 },
    { EEXIST, "EXIST", "File exists", 0 },
    { EXDEV, "XDEV", "Cross-device link", 0 },
    { ENODEV, "NODEV", "No such device", 0 },
    { ENOTDIR, "NOTDIR", "Not a directory", 0 },
    { EISDIR, "ISDIR", "Is a directory", 0 },
    { EINVAL, "INVAL", "Invalid argument", 0 },
    { ENFILE, "NFILE", "File table overflow", 0 },
    { EMFILE, "MFILE", "Too many open files", 0 },
    { ENOTTY, "NOTTY", "Not a typewriter", 0 },
    { ETXTBSY, "TXTBSY", "Text file busy", 0 },
    { EFBIG, "FBIG", "File too large", 0 },
    { ENOSPC, "NOSPC", "No space left on device", 0 },
    { ESPIPE, "SPIPE", "Illegal seek", 0 },
    { EROFS, "ROFS", "Read-only file system", 0 },
    { EMLINK, "MLINK", "Too many links", 0 },
    { EPIPE, "PIPE", "Broken pipe", 0 },
    { EDOM, "DOM", "Math argument out of domain of func", 0 },
    { ERANGE, "RANGE", "Math result not representable", 0 },
    { EDEADLK, "DEADLK", "Resource deadlock would occur", 0 },
    { ENAMETOOLONG, "NAMETOOLONG", "File name too long", 0 },
    { ENOLCK, "NOLCK", "No record locks available", 0 },
    { ENOSYS, "NOSYS", "Function not implemented", 0 },
    { ENOTEMPTY, "NOTEMPTY", "Directory not empty", 0 },
    { ELOOP, "LOOP", "Too many symbolic links encountered", 0 },
    { EWOULDBLOCK, "WOULDBLOCK", "Operation would block", 0 },
    { ENOMSG, "NOMSG", "No message of desired type", 0 },
    { EIDRM, "IDRM", "Identifier removed", 0 },
    { ECHRNG, "CHRNG", "Channel number out of range", 0 },
    { EL2NSYNC, "L2NSYNC", "Level 2 not synchronized", 0 },
    { EL3HLT, "L3HLT", "Level 3 halted", 0 },
    { EL3RST, "L3RST", "Level 3 reset", 0 },
    { ELNRNG, "LNRNG", "Link number out of range", 0 },
    { EUNATCH, "UNATCH", "Protocol driver not attached", 0 },
    { ENOCSI, "NOCSI", "No CSI structure available", 0 },
    { EL2HLT, "L2HLT", "Level 2 halted", 0 },
    { EBADE, "BADE", "Invalid exchange", 0 },
    { EBADR, "BADR", "Invalid request descriptor", 0 },
    { EXFULL, "XFULL", "Exchange full", 0 },
    { ENOANO, "NOANO", "No anode", 0 },
    { EBADRQC, "BADRQC", "Invalid request code", 0 },
    { EBADSLT, "BADSLT", "Invalid slot", 0 },
    { EDEADLOCK, "DEADLOCK", "Resource deadlock would occur", 0 },
    { EBFONT, "BFONT", "Bad font file format", 0 },
    { ENOSTR, "NOSTR", "Device not a stream", 0 },
    { ENODATA, "NODATA", "No data available", 0 },
    { ETIME, "TIME", "Timer expired", 0 },
    { ENOSR, "NOSR", "Out of streams resources", 0 },
    { ENONET, "NONET", "Machine is not on the network", 0 },
    { ENOPKG, "NOPKG", "Package not installed", 0 },
    { EREMOTE, "REMOTE", "Object is remote", 0 },
    { ENOLINK, "NOLINK", "Link has been severed", 0 },
    { EADV, "ADV", "Advertise error", 0 },
    { ESRMNT, "SRMNT", "Srmount error", 0 },
    { ECOMM, "COMM", "Communication error on send", 0 },
    { EPROTO, "PROTO", "Protocol error", 0 },
    { EMULTIHOP, "MULTIHOP", "Multihop attempted", 0 },
    { EDOTDOT, "DOTDOT", "RFS specific error", 0 },
    { EBADMSG, "BADMSG", "Not a data message", 0 },
    { EOVERFLOW, "OVERFLOW", "Value too large for defined data type", 0 },
    { ENOTUNIQ, "NOTUNIQ", "Name not unique on network", 0 },
    { EBADFD, "BADFD", "File descriptor in bad state", 0 },
    { EREMCHG, "REMCHG", "Remote address changed", 0 },
    { ELIBACC, "LIBACC", "Can not access a needed shared library", 0 },
    { ELIBBAD, "LIBBAD", "Accessing a corrupted shared library", 0 },
    { ELIBSCN, "LIBSCN", ".lib section in a.out corrupted", 0 },
    { ELIBMAX, "LIBMAX", "Attempting to link in too many shared libraries", 0 },
    { ELIBEXEC, "LIBEXEC", "Cannot exec a shared library directly", 0 },
    { EILSEQ, "ILSEQ", "Illegal byte sequence", 0 },
    { ERESTART, "RESTART", "Interrupted system call should be restarted", 0 },
    { ESTRPIPE, "STRPIPE", "Streams pipe error", 0 },
    { EUSERS, "USERS", "Too many users", 0 },
    { ENOTSOCK, "NOTSOCK", "Socket operation on non-socket", 0 },
    { EDESTADDRREQ, "DESTADDRREQ", "Destination address required", 0 },
    { EMSGSIZE, "MSGSIZE", "Message too long", 0 },
    { EPROTOTYPE, "PROTOTYPE", "Protocol wrong type for socket", 0 },
    { ENOPROTOOPT, "NOPROTOOPT", "Protocol not available", 0 },
    { EPROTONOSUPPORT, "PROTONOSUPPORT", "Protocol not supported", 0 },
    { ESOCKTNOSUPPORT, "SOCKTNOSUPPORT", "Socket type not supported", 0 },
    { EOPNOTSUPP, "OPNOTSUPP", "Operation not supported on transport endpoint", 0 },
    { EPFNOSUPPORT, "PFNOSUPPORT", "Protocol family not supported", 0 },
    { EAFNOSUPPORT, "AFNOSUPPORT", "Address family not supported by protocol", 0 },
    { EADDRINUSE, "ADDRINUSE", "Address already in use", 0 },
    { EADDRNOTAVAIL, "ADDRNOTAVAIL", "Cannot assign requested address", 0 },
    { ENETDOWN, "NETDOWN", "Network is down", 0 },
    { ENETUNREACH, "NETUNREACH", "Network is unreachable", 0 },
    { ENETRESET, "NETRESET", "Network dropped connection because of reset", 0 },
    { ECONNABORTED, "CONNABORTED", "Software caused connection abort", 0 },
    { ECONNRESET, "CONNRESET", "Connection reset by peer", 0 },
    { ENOBUFS, "NOBUFS", "No buffer space available", 0 },
    { EISCONN, "ISCONN", "Transport endpoint is already connected", 0 },
    { ENOTCONN, "NOTCONN", "Transport endpoint is not connected", 0 },
    { ESHUTDOWN, "SHUTDOWN", "Cannot send after transport endpoint shutdown", 0 },
    { ETOOMANYREFS, "TOOMANYREFS", "Too many references: cannot splice", 0 },
    { ETIMEDOUT, "TIMEDOUT", "Connection timed out", 0 },
    { ECONNREFUSED, "CONNREFUSED", "Connection refused", 0 },
    { EHOSTDOWN, "HOSTDOWN", "Host is down", 0 },
    { EHOSTUNREACH, "HOSTUNREACH", "No route to host", 0 },
    { EALREADY, "ALREADY", "Operation already in progress", 0 },
    { EINPROGRESS, "INPROGRESS", "Operation now in progress", 0 },
    { ESTALE, "STALE", "Stale NFS file handle", 0 },
    { EUCLEAN, "UCLEAN", "Structure needs cleaning", 0 },
    { ENOTNAM, "NOTNAM", "Not a XENIX named type file", 0 },
    { ENAVAIL, "NAVAIL", "No XENIX semaphores available", 0 },
    { EISNAM, "ISNAM", "Is a named type file", 0 },
    { EREMOTEIO, "REMOTEIO", "Remote I/O error", 0 },
    { EDQUOT, "DQUOT", "Quota exceeded", 0 },
    { ENOMEDIUM, "NOMEDIUM", "No medium found", 0 },
    { EMEDIUMTYPE, "MEDIUMTYPE", "Wrong medium type", 0 },
};

#define NUM_FILE_ERRORS	(sizeof (fileErrorMap) / sizeof (fileErrorMap[0]))

Types	*typesFileError;

static int
FileInitErrors (void)
{
    ENTER ();
    StructType	    *st;
    StructElement   *se;
    int		    i;

    st = NewStructType (NUM_FILE_ERRORS);
    se = StructTypeElements (st);
    for (i = 0; i < NUM_FILE_ERRORS; i++)
    {
	fileErrorMap[i].atom = AtomId (fileErrorMap[i].name);
	se[i].type = typesEnum;
	se[i].name = fileErrorMap[i].atom;
    }
    typesFileError = NewTypesUnion (st, True);
    MemAddRoot (typesFileError);
    EXIT ();
    return 1;
}

int
FileInit (void)
{
    ENTER ();
    fileBlockedReference = NewReference ((void **) &fileBlocked);
    MemAddRoot (fileBlockedReference);
    FileInitErrors ();
    EXIT ();
    return 1;
}

Value
FileGetError (int err)
{
    ENTER();
    Value   ret;
    int	    i;

    for (i = 0; i < NUM_FILE_ERRORS; i++)
	if (fileErrorMap[i].value == err)
	    break;
    if (i == NUM_FILE_ERRORS)
	i = 0;	    /* XXX weird error */
    ret = NewUnion (typesFileError->structs.structs, True);
    ret->unions.tag = fileErrorMap[i].atom;
    BoxValueSet (ret->unions.value,0,Void);
    RETURN (ret);
}

static void
FileMark (void *object)
{
    File    *file = object;

    MemReference (file->next);
    MemReference (file->input);
    MemReference (file->output);
}

static void
FileFree (void *object)
{
    File    *file = object;

    if (file->fd != -1)
    {
	FileResetFd (file->fd);
	close (file->fd);
	file->fd = -1;
    }
}

static Bool
FilePrint (Value f, Value av, char format, int base, int width, int prec, unsigned char fill)
{
    FilePuts (f, "file");
    return True;
}

ValueType FileType = {
    { FileMark, FileFree },
    type_file, 
    {
	0, 0, 0, 0, 0, 0,
	0, ValueEqual, 0, 0,
    },
    {
	0,
    },
    0,
    0,
    FilePrint,
};

Value
NewFile (int fd)
{
    ENTER ();
    Value   ret;

    ret = ALLOCATE (&FileType.data, sizeof (File));
    ret->file.next = 0;
    ret->file.fd = fd;
    ret->file.pid = 0;
    ret->file.status = 0;
    ret->file.flags = 0;
    ret->file.error = 0;
    ret->file.input = 0;
    ret->file.output = 0;
    RETURN (ret);
}


void
FileSetFd (int fd)
{
    int	flags;
    
    fcntl (fd, F_SETOWN, getpid());
    flags = fcntl (fd, F_GETFL);
    flags |= ASYNC|NOBLOCK;
    (void) fcntl (fd, F_SETFL, flags);
#ifdef USE_STREAMS_ASYNC
    (void) ioctl(fd, I_SETSIG, S_INPUT | S_OUTPUT | S_ERROR | S_HANGUP);
#endif
}

void
FileResetFd (int fd)
{
    int	flags;

    flags = fcntl (fd, F_GETFL);
    flags &= ~(ASYNC|NOBLOCK);
    (void) fcntl (fd, F_SETFL, flags);
#ifdef  USE_STREAMS_ASYNC
    (void) ioctl(fd, I_SETSIG, 0);
#endif
}

static void
FileChainMark (void *object)
{
    FileChainPtr	chain = object;

    MemReference (chain->next);
}

DataType    FileChainType = { FileChainMark, 0 };

static FileChainPtr
NewFileChain (FileChainPtr next, int size)
{
    ENTER ();
    FileChainPtr	ret;

    ret = MemAllocate (&FileChainType, sizeof (FileChain) + size);
    ret->next = next;
    ret->size = size;
    ret->used = 0;
    ret->ptr = 0;
    RETURN (ret);
}

Value
FileCreate (int fd, int flags)
{
    ENTER ();
    Value   file;

    file = NewFile (fd);
    file->file.flags |= flags;
    if (isatty (fd))
	file->file.flags |= FileLineBuf;
    if (fd >= 3)
	FileSetFd (fd);
    RETURN (file);
}

Value
FileFopen (char *name, char *mode, int *errp)
{
    ENTER ();
    int	    oflags = 0;
    int	    flags = 0;
    int	    fd;
    
    switch (mode[0]) {
    case 'r':
	if (mode[1] == '+')
	{
	    flags |= FileWritable;
	    oflags = 2;
	}
	else
	    oflags = 0;
	flags |= FileReadable;
	break;
    case 'w':
	if (mode[1] == '+')
	{
	    oflags = 2;
	    flags |= FileReadable;
	}
	else
	    oflags = 1;
	oflags |= O_TRUNC|O_CREAT;
	flags |= FileWritable;
	break;
    case 'a':
	if (mode[1] == '+')
	{
	    oflags = 2;
	    flags |= FileReadable;
	}
	else
	    oflags = 1;
	oflags |= O_TRUNC|O_CREAT|O_APPEND;
	flags |= FileWritable;
	break;
    }
    fd = open (name, oflags, 0666);
    if (fd < 0)
    {
	*errp = errno;
	RETURN (0);
    }
    RETURN (FileCreate (fd, flags));
}

Value
FilePopen (char *program, char *argv[], char *mode, int *errp)
{
    ENTER ();
    int	    fd, fds[2];
    int	    pid;
    Bool    reading;
    Value   file;

    if (*mode == 'r')
	reading = True;
    else
	reading = False;
    pipe (fds);
    switch ((pid = fork ())) {
    case -1:
	close (fds[0]);
	close (fds[1]);
	*errp = errno;
	fd = -1;
	break;
    case 0:
	if (reading)
	{
	    close (1);
	    dup2 (fds[1], 1);
	    close (fds[0]);
	}
	else
	{
	    close (0);
	    dup2 (fds[0], 0);
	    close (fds[1]);
	}
	execvp (program, argv);
	exit (1);
    default:
	if (reading)
	{
	    close (fds[1]);
	    fd = fds[0];
	}
	else
	{
	    close (fds[0]);
	    fd = fds[1];
	}
	break;
    }
    if (fd >= 0)
    {
	file = FileCreate (fd, reading ? FileReadable : FileWritable);
	file->file.flags |= FilePipe;
	file->file.pid = pid;
    }
    else
    {
	*errp = errno;
	file = 0;
    }
    RETURN (file);
}

int
FileStatus (Value file)
{
    return file->file.status;
}

int
FileClose (Value file)
{
    file->file.flags |= FileClosed;
    return FileFlush (file);
}

Value
FileStringRead (char *string, int len)
{
    ENTER ();
    Value   file;

    file = NewFile (-1);
    file->file.flags |= FileString|FileReadable;
    file->file.input = NewFileChain (0, len);
    memcpy (FileBuffer (file->file.input), string, len);
    file->file.input->used = len;
    RETURN (file);
}

Value
FileStringWrite (void)
{
    ENTER ();
    Value   file;

    file = NewFile (-1);
    file->file.flags |= FileString|FileWritable;
    RETURN (file);
}

Value
FileStringString (Value file)
{
    ENTER ();
    int		    len;
    FileChainPtr    out;
    Value	    str;
    char	    *s;

    if (!(file->file.flags & FileString))
    {
	RaiseStandardException (exception_invalid_argument,
				"string_string: not string file",
				2,
				Zero, file);
	RETURN (Zero);
    }
    len = 0;
    for (out = file->file.output; out; out = out->next)
	len += out->used;
    str = NewString (len);
    s = StringChars (&str->string);
    for (out = file->file.output; out; out = out->next)
    {
	memcpy (s, FileBuffer(out), out->used);
	s += out->used;
    }
    *s = '\0';
    RETURN (str);
}

#define DontBlockIO	(runnable && running)

int
FileInput (Value file)
{
    ENTER ();
    int		    c, n;
    unsigned char   *buf;
    FileChainPtr    ic;
    int		    err;

    if (file->file.flags & FileClosed)
    {
	EXIT ();
	return FileError;
    }
    if (!file->file.input)
    {
	if (!(file->file.flags & FileReadable))
	{
	    file->file.flags |= FileInputError;
	    file->file.input_errno = EBADF;
	    EXIT ();
	    return FileError;
	}
	if (file->file.flags & FileString)
	{
	    EXIT ();
	    return FileEnd;
	}
	file->file.input = NewFileChain (0, FileBufferSize);
    }
    ic = file->file.input;
    for (;;)
    {
	if (ic->ptr < ic->used)
	{
	    c = FileBuffer (ic)[ic->ptr++];
	    break;
	}
	else
	{
	    if (ic->next)
		ic = ic->next;
	    else
	    {
		buf = FileBuffer (ic);
		if (file->file.fd < 3 && !ownTty[file->file.fd])
		{
		    n = -1;
		    err = EWOULDBLOCK;
		}
		else
		{
		    n = ic->size;
		    if (file->file.flags & FileUnBuf)
			n = 1;
		    n = read (file->file.fd, buf, n);
		    err = errno;
		    file->file.flags &= ~FileEnd;
		}
		if (n <= 0)
		{
		    if (n == 0)
		    {
			file->file.flags |= FileEnd;
			c = FileEOF;
		    }
		    else if (err == EWOULDBLOCK)
		    {
			FileSetBlocked (file, FileInputBlocked);
			c = FileBlocked;
		    }
		    else
		    {
			file->file.flags |= FileInputError;
			file->file.input_errno = err;
			c = FileError;
		    }
		    break;
		}
		ic->ptr = 0;
		ic->used = n;
	    }
	}
    }
    EXIT ();
    return c;
}

void
FileUnput (Value file, unsigned char c)
{
    ENTER ();
    FileChainPtr	ic;

    ic = file->file.input;
    if (!ic || ic->ptr == 0)
    {
	ic = file->file.input = NewFileChain (file->file.input, FileBufferSize);
	ic->ptr = ic->used = ic->size;
    }
    FileBuffer(ic)[--ic->ptr] = c;
    EXIT ();
}

static void
FileWaitForWriteable (Value file)
{
    int	    n;
    fd_set  bits;

    FD_ZERO (&bits);
    for (;;) 
    {
	FD_SET (file->file.fd, &bits);
	n = select (file->file.fd + 1, 0, &bits, 0, 0);
	if (n > 0)
	    break;
    }
}

static int
FileFlushChain (Value file, FileChainPtr ic)
{
    int	    n;
    int	    err;

    while (ic->ptr < ic->used)
    {
	n = write (file->file.fd, &FileBuffer(ic)[ic->ptr], ic->used - ic->ptr);
	err = errno;
	if (n > 0)
	    ic->ptr += n;
	else
	{
	    if (n < 0 && err != EWOULDBLOCK)
	    {
		file->file.flags |= FileOutputError;
		file->file.output_errno = err;
		return FileError;
	    }
	    if (!(file->file.flags & FileBlockWrites))
	    {
		FileSetBlocked (file, FileOutputBlocked);
		return FileBlocked;
	    }
	    FileWaitForWriteable (file);
	}
    }
    return 0;
}

int
FileFlush (Value file)
{
    ENTER ();
    FileChainPtr    ic, *prev;
    int		    n = 0;

    if (file->file.output)
    {
	if ((file->file.flags & FileString) == 0)
	{
	    for (;;)
	    {
		for (prev = &file->file.output; (ic = *prev)->next; prev = &ic->next);
		n = FileFlushChain (file, ic);
		if (n)
		    break;
		/*
		 * Leave a chain for new output
		 */
		if (prev == &file->file.output)
		{
		    ic->used = ic->ptr = 0;
		    break;
		}
		*prev = 0;
	    }
	}
	ic = file->file.output;
	if (ic->used == ic->size)
	    ic = file->file.output = NewFileChain (file->file.output, FileBufferSize);
    }
    if (file->file.flags & FileClosed && n != FileBlocked)
    {
	if (file->file.fd != -1)
	{
	    FileResetFd (file->file.fd);
	    close (file->file.fd);
	    file->file.fd = -1;
	}
	/* FIXME -- dont block waiting for child */
	if (file->file.pid != 0)
	{
	    while (waitpid (file->file.pid, &file->file.status, 0) < 0);
	    file->file.pid = 0;
	}
    }
    EXIT ();
    return n;
}

int
FileOutput (Value file, char c)
{
    ENTER ();
    FileChainPtr	ic;

    if (file->file.flags & FileClosed)
    {
	file->file.flags |= FileOutputError;
	file->file.output_errno = EBADF;
	EXIT ();
	return FileError;
    }
    if (!(file->file.flags & FileWritable))
    {
	file->file.flags |= FileOutputError;
	file->file.output_errno = EBADF;
	EXIT ();
	return FileError;
    }
    ic = file->file.output;
    if (!ic)
	ic = file->file.output = NewFileChain (0, FileBufferSize);
    if (ic->used == ic->size)
	if (FileFlush (file) == FileError)
	{
	    EXIT ();
	    return FileError;
	}
    ic = file->file.output;
    FileBuffer(ic)[ic->used++] = c;
    if ((c == '\n' && file->file.flags & FileLineBuf) ||
	file->file.flags & FileUnBuf)
    {
	if (FileFlush (file) == FileError)
	{
	    EXIT ();
	    return FileError;
	}
    }
    EXIT ();
    return 0;
}

void
FilePuts (Value file, char *s)
{
    while (*s)
	FileOutput (file, *s++);
}

void
FilePutUIntBase (Value file, unsigned int a, int base)
{
    int	    digit;
    char    space[64], *s;

    s = space + sizeof (space);
    *--s = '\0';
    if (!a)
	*--s = '0';
    else
    {
	while (a)
	{
	    digit = a % base;
	    if (digit <= 9) 
		digit = '0' + digit;
	    else
		digit = 'a' + digit - 10;
	    *--s = digit;
	    a /= base;
	}
    }
    FilePuts (file, s);
}

void
FilePutIntBase (Value file, int a, int base)
{
    if (a < 0)
    {
	FileOutput (file, '-');
	a = -a;
    }
    FilePutUIntBase (file, a, base);
}

void	FilePutInt (Value file, int a)
{
    FilePutIntBase (file, a, 10);
}

int
FileStringWidth (char *string, char format)
{
    if (format == 's')
	return strlen (string);
    else
    {
	int	    width = 2;
	char    c;
	while ((c = *string++))
	{
	    if (c < ' ' || '~' < c)
		switch (c) {
		case '\n':
		case '\r':
		case '\t':
		case '\b':
		case '\f':
		    width += 2;
		    break;
		default:
		    width += 4;
		    break;
		}
	    else if (c == '"')
		width += 2;
	    else
		width++;
	}
	return width;
    }
}

void
FilePutString (Value f, char *string, char format)
{
    if (format == 's')
	FilePuts (f, string);
    else
    {
	int c;
	FileOutput (f, '"');
	while ((c = *string++ & 0xff)) {
	    if (c < ' ' || '~' < c)
		switch (c) {
		case '\n':
		    FilePuts (f, "\\n");
		    break;
		case '\r':
		    FilePuts (f, "\\r");
		    break;
		case '\b':
		    FilePuts (f, "\\b");
		    break;
		case '\t':
		    FilePuts (f, "\\t");
		    break;
		case '\f':
		    FilePuts (f, "\\f");
		    break;
		default:
		    FileOutput (f, '\\');
		    Print (f, NewInt (c), 'o', 8, 3, -1, '0');
		    break;
		}
	    else if (c == '"')
		FilePuts (f, "\\\"");
	    else
		FileOutput (f, c);
	}
	FileOutput (f, '"');
    }
}

void
FilePutType (Value f, Type tag, Bool minimal)
{
    switch (tag) {
    case type_undef:
	if (!minimal)
	    FilePuts (f, "poly");
	break;
    case type_int:
    case type_integer:
	FilePuts (f, "int");
	break;
    case type_rational:
	FilePuts (f, "rational");
	break;
    case type_float:
	FilePuts (f, "real");
	break;
    case type_string:
	FilePuts (f, "string");
	break;
    case type_file:
	FilePuts (f, "file");
	break;
    case type_thread:
	FilePuts (f, "thread");
	break;
    case type_semaphore:
	FilePuts (f, "semaphore");
	break;
    case type_continuation:
	FilePuts (f, "continuation");
	break;
    case type_void:
	FilePuts (f, "void");
	break;
	
    case type_array:
	FilePuts (f, "array");
	break;
    case type_ref:
	FilePuts (f, "ref");
	break;
    case type_struct:
	FilePuts (f, "struct");
	break;
    case type_func:
	FilePuts (f, "function");
	break;
    default:
	FilePrintf (f, "bad type %d", tag);
	break;
    }
    if (minimal && tag != type_undef)
	FilePuts (f, " ");
}

void
FilePutClass (Value f, Class storage, Bool minimal)
{
    switch (storage) {
    case class_undef:
	if (!minimal)
	    FilePuts (f, "undefined");
	break;
    case class_const:
	FilePuts (f, "const");
	break;
    case class_global:
	FilePuts (f, "global");
	break;
    case class_arg:
	FilePuts (f, "argument");
	break;
    case class_auto:
	FilePuts (f, "auto");
	break;
    case class_static:
	FilePuts (f, "static");
	break;
    case class_typedef:
	FilePuts (f, "typedef");
	break;
    case class_namespace:
	FilePuts (f, "namespace");
	break;
    case class_exception:
	FilePuts (f, "exception");
	break;
    }
    if (minimal && storage != class_undef)
	FilePuts (f, " ");
}

void
FilePutPublish (Value f, Publish publish, Bool minimal)
{
    switch (publish) {
    case publish_private:
	if (!minimal)
	    FilePuts (f, "private");
	break;
    case publish_protected:
	FilePuts (f, "protected");
	break;
    case publish_public:
	FilePuts (f, "public");
	break;
    case publish_extend:
	FilePuts (f, "extend");
	break;
    }
    if (minimal && publish != publish_private)
	FilePuts (f, " ");
}

void
FilePutArgTypes (Value f, ArgType *at)
{
    FilePuts (f, "(");
    while (at)
    {
	if (at->type)
	    FilePutTypes (f, at->type, at->name != 0);
	if (at->name)
	    FilePuts (f, AtomName (at->name));
	if (at->varargs)
	    FilePuts (f, " ...");
	at = at->next;
	if (at)
	    FilePuts (f, ", ");
    }
    FilePuts (f, ")");
}

static void
FilePutDimensions (Value f, ExprPtr dims)
{
    while (dims)
    {
	if (dims->tree.left)
	    PrettyExpr (f, dims->tree.left, -1, 0, False);
	else
	    FilePuts (f, "*");
	if (dims->tree.right)
	    FilePuts (f, ", ");
	dims = dims->tree.right;
    }
}

static void
FilePutTypename (Value f, ExprPtr e)
{
    switch (e->base.tag) {
    case COLONCOLON:
	if (e->tree.left)
	{
	    FilePutTypename (f, e->tree.left);
	    FilePuts (f, "::");
	}
	FilePutTypename (f, e->tree.right);
	break;
    case NAME:
	FilePuts (f, AtomName (e->atom.atom));
	break;
    }
}

void
FilePutTypes (Value f, Types *t, Bool minimal)
{
    int		    i;
    StructType	    *st;
    StructElement   *se;
    Bool	    spaceit = minimal;
    
    if (!t)
    {
	FilePuts (f, "<undefined>");
	return;
    }
    switch (t->base.tag) {
    case types_prim:
	if (t->prim.prim != type_undef || !minimal)
	    FilePutType (f, t->prim.prim, False);
	else
	    spaceit = False;
	break;
    case types_name:
	FilePutTypename (f, t->name.expr);
	break;
    case types_ref:
	FilePuts (f, "*");
	FilePutTypes (f, t->ref.ref, False);
	break;
    case types_func:
	FilePutTypes (f, t->func.ret, False);
	FilePutArgTypes (f, t->func.args);
	break;
    case types_array:
	FilePutTypes (f, t->array.type, False);
	FilePuts (f, "[");
	FilePutDimensions (f, t->array.dimensions);
	FilePuts (f, "]");
	break;
    case types_struct:
    case types_union:
	if (t->structs.enumeration)
	{
	    FilePuts (f, "enum { ");
	    st = t->structs.structs;
	    se = StructTypeElements (st);
	    for (i = 0; i < st->nelements; i++)
	    {
		if (i)
		    FilePuts (f, ", ");
		if (se[i].name)
		    FilePuts (f, AtomName (se[i].name));
	    }
	    FilePuts (f, " }");
	}
	else
	{
	    if (t->base.tag == types_struct)
		FilePuts (f, "struct { ");
	    else
		FilePuts (f, "union { ");
	    st = t->structs.structs;
	    se = StructTypeElements (st);
	    for (i = 0; i < st->nelements; i++)
	    {
		if (se[i].type == typesEnum)
		    FilePuts (f, "enum ");
		else
		    FilePutTypes (f, se[i].type, se[i].name != 0);
		if (se[i].name)
		    FilePuts (f, AtomName (se[i].name));
		FilePuts (f, "; ");
	    }
	    FilePuts (f, "}");
	}
	break;
    }
    if (spaceit)
	FilePuts (f, " ");
}

static void
FilePutBinOp (Value f, BinaryOp o)
{
    switch (o) {
    case PlusOp:
	FilePuts (f, "+");
	break;
    case MinusOp:
	FilePuts (f, "-");
	break;
    case TimesOp:
	FilePuts (f, "*");
	break;
    case DivideOp:
	FilePuts (f, "/");
	break;
    case DivOp:
	FilePuts (f, "//");
	break;
    case ModOp:
	FilePuts (f, "%");
	break;
    case LessOp:
	FilePuts (f, "<");
	break;
    case EqualOp:
	FilePuts (f, "==");
	break;
    case LandOp:
	FilePuts (f, "&");
	break;
    case LorOp:
	FilePuts (f, "|");
	break;
    default:
	break;
    }
}

static void
FilePutUnaryOp (Value f, UnaryOp o)
{
    switch (o) {
    case NegateOp:
	FilePuts (f, "-");
	break;
    case FloorOp:
	FilePuts (f, "floor");
	break;
    case CeilOp:
	FilePuts (f, "ceil");
	break;
    default:
	break;
    }
}

void
FileVPrintf (Value file, char *fmt, va_list args)
{
    Value	v;

    for (;*fmt;) {
	switch (*fmt) {
	case '\0':
	    continue;
	case '%':
	    switch (*++fmt) {
	    case '\0':
		continue;
	    case 'd':
		FilePutIntBase (file, va_arg (args, int), 10);
		break;
	    case 'u':
		FilePutUIntBase (file, va_arg (args, unsigned int), 10);
		break;
	    case 'o':
		FilePutUIntBase (file, va_arg (args, unsigned int), 8);
		break;
	    case 'x':
		FilePutUIntBase (file, va_arg (args, unsigned int), 16);
		break;
	    case 'v':
	    case 'g':
		v = va_arg (args, Value);
		if (!v)
		    (void) FilePuts (file, "<uninit>");
		else
		    Print (file, v, *fmt, 0, 0, DEFAULT_OUTPUT_PRECISION, ' ');
		break;
	    case 'n':
		FilePuts (file, NaturalSprint (0, va_arg (args, Natural *), 10, 0));
		break;
	    case 'N':
		FilePuts (file, NaturalSprint (0, va_arg (args, Natural *), 16, 0));
		break;
	    case 's':
		(void) FilePuts (file, va_arg (args, char *));
		break;
	    case 'S':
		FilePutString (file, va_arg (args, char *), 'v');
		break;
	    case 'A':
		(void) FilePuts (file, AtomName (va_arg (args, Atom)));
		break;
	    case 't':
		FilePutTypes (file, va_arg (args, Types *), True);
		break;
	    case 'T':
		FilePutTypes (file, va_arg (args, Types *), False);
		break;
	    case 'k':	/* sic */
		FilePutClass (file, (Class) (va_arg (args, int)), True);
		break;
	    case 'C':
		FilePutClass (file, (Class) (va_arg (args, int)), False);
		break;
	    case 'p':
		FilePutPublish (file, (Publish) (va_arg (args, int)), True);
		break;
	    case 'P':
		FilePutPublish (file, (Publish) (va_arg (args, int)), False);
		break;
	    case 'O':
		FilePutBinOp (file, va_arg (args, BinaryOp));
		break;
	    case 'U':
		FilePutUnaryOp (file, va_arg (args, UnaryOp));
		break;
	    case 'c':
		(void) FileOutput (file, va_arg (args, int));
		break;
	    default:
		(void) FileOutput (file, *fmt);
		break;
	    }
	    break;
	default:
	    (void) FileOutput (file, *fmt);
	    break;
	}
	++fmt;
    }
}

void
FilePrintf (Value file, char *fmt, ...)
{
    va_list args;
    
    va_start (args, fmt);
    FileVPrintf (file, fmt, args);
    va_end (args);
}

void
FileCheckBlocked (void)
{
    ENTER ();
    fd_set	    readable, writable;
    int		    n, fd;
    struct timeval  tv;
    Value	    blocked, *prev;
    Bool	    ready;
    Bool	    writeBlocked;
    Bool	    readBlocked;
    
    FD_ZERO (&readable);
    FD_ZERO (&writable);
    n = 0;
    for (blocked = fileBlocked; blocked; blocked = blocked->file.next)
    {
	fd = blocked->file.fd;
	if (fd < 3 && !ownTty[fd])
	    continue;
	if (blocked->file.flags & FileInputBlocked)
	    FD_SET (fd, &readable);
	if (blocked->file.flags & FileOutputBlocked)
	    FD_SET (fd, &writable);
	if (fd >= n)
	    n = fd + 1;
    }
    tv.tv_usec = 0;
    tv.tv_sec = 0;
    if (n > 0)
	n = select (n, &readable, &writable, 0, &tv);
    if (n > 0)
    {
	writeBlocked = False;
	readBlocked = False;
	for (prev = &fileBlocked; (blocked = *prev); )
	{
	    fd = blocked->file.fd;
	    ready = False;
	    if (FD_ISSET (fd, &readable))
	    {
		ready = True;
		blocked->file.flags &= ~FileInputBlocked;
	    }
	    if (FD_ISSET (fd, &writable))
	    {
		if (FileFlush (blocked) != FileBlocked)
		{
		    blocked->file.flags &= ~FileOutputBlocked;
		    ready = True;
		}
	    }
	    if (blocked->file.flags & FileOutputBlocked)
		writeBlocked = True;
	    if (blocked->file.flags & FileInputBlocked)
		readBlocked = True;
	    if (ready)
		ThreadsWakeup (blocked, WakeAll);
	    if ((blocked->file.flags & (FileOutputBlocked|FileInputBlocked)) == 0)
		*prev = blocked->file.next;
	    else
		prev = &blocked->file.next;
	}
	anyFileWriteBlocked = writeBlocked;
	anyFileReadBlocked = readBlocked;
    }
    EXIT ();
}

void
FileSetBlocked (Value file, int flag)
{
    if (flag == FileOutputBlocked && !anyFileWriteBlocked)
    {
	anyFileWriteBlocked = True;
	IoNoticeWriteBlocked ();
    }
    if (flag == FileInputBlocked && !anyFileReadBlocked)
    {
	anyFileReadBlocked = True;
	IoNoticeReadBlocked ();
    }
    if (file->file.flags & (FileOutputBlocked|FileInputBlocked))
    {
	file->file.flags |= flag;
	return;
    }
    file->file.flags |= flag;
    file->file.next = fileBlocked;
    fileBlocked = file;
}

void
FileSetBuffer (Value file, int mode)
{
    file->file.flags &= ~(FileLineBuf|FileUnBuf);
    switch (mode) {
    case 0:
	break;
    case 1:
	file->file.flags |= FileLineBuf;
	break;
    case 2:
	file->file.flags |= FileUnBuf;
	break;
    }
}
