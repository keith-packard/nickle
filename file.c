/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
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
#include	<sys/socket.h>
#include        <assert.h>
#include	"nickle.h"
#include	"ref.h"
#include	"gram.h"

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
extern Bool	stdinOwned, stdinPolling;
#ifdef NO_PIPE_SIGIO
Bool		anyPipeReadBlocked;
#endif
extern Bool	ownTty[3];

typedef struct _FileErrorMap {
    int		value;
    char	*name;
    char	*message;
} FileErrorMap;

#define EUTF8    -128

const FileErrorMap   fileErrorMap[] = {
#ifdef EPERM
    { EPERM, "PERM", "Operation not permitted" },
#endif
#ifdef ENOENT
    { ENOENT, "NOENT", "No such file or directory" },
#endif
#ifdef ESRCH
    { ESRCH, "SRCH", "No such process" },
#endif
#ifdef EINTR
    { EINTR, "INTR", "Interrupted system call" },
#endif
#ifdef EIO
    { EIO, "IO", "I/O error" },
#endif
#ifdef ENXIO
    { ENXIO, "NXIO", "No such device or address" },
#endif
#ifdef E2BIG
    { E2BIG, "2BIG", "Arg list too long" },
#endif
#ifdef ENOEXEC
    { ENOEXEC, "NOEXEC", "Exec format error" },
#endif
#ifdef EBADF
    { EBADF, "BADF", "Bad file number" },
#endif
#ifdef ECHILD
    { ECHILD, "CHILD", "No child processes" },
#endif
#ifdef EAGAIN
    { EAGAIN, "AGAIN", "Try again" },
#endif
#ifdef ENOMEM
    { ENOMEM, "NOMEM", "Out of memory" },
#endif
#ifdef EACCES
    { EACCES, "ACCES", "Permission denied" },
#endif
#ifdef EFAULT
    { EFAULT, "FAULT", "Bad address" },
#endif
#ifdef ENOTBLK
    { ENOTBLK, "NOTBLK", "Block device required" },
#endif
#ifdef EBUSY
    { EBUSY, "BUSY", "Device or resource busy" },
#endif
#ifdef EEXIST
    { EEXIST, "EXIST", "File exists" },
#endif
#ifdef EXDEV
    { EXDEV, "XDEV", "Cross-device link" },
#endif
#ifdef ENODEV
    { ENODEV, "NODEV", "No such device" },
#endif
#ifdef ENOTDIR
    { ENOTDIR, "NOTDIR", "Not a directory" },
#endif
#ifdef EISDIR
    { EISDIR, "ISDIR", "Is a directory" },
#endif
#ifdef EINVAL
    { EINVAL, "INVAL", "Invalid argument" },
#endif
#ifdef ENFILE
    { ENFILE, "NFILE", "File table overflow" },
#endif
#ifdef EMFILE
    { EMFILE, "MFILE", "Too many open files" },
#endif
#ifdef ENOTTY
    { ENOTTY, "NOTTY", "Not a typewriter" },
#endif
#ifdef ETXTBSY
    { ETXTBSY, "TXTBSY", "Text file busy" },
#endif
#ifdef EFBIG
    { EFBIG, "FBIG", "File too large" },
#endif
#ifdef ENOSPC
    { ENOSPC, "NOSPC", "No space left on device" },
#endif
#ifdef ESPIPE
    { ESPIPE, "SPIPE", "Illegal seek" },
#endif
#ifdef EROFS
    { EROFS, "ROFS", "Read-only file system" },
#endif
#ifdef EMLINK
    { EMLINK, "MLINK", "Too many links" },
#endif
#ifdef EPIPE
    { EPIPE, "PIPE", "Broken pipe" },
#endif
#ifdef EDOM
    { EDOM, "DOM", "Math argument out of domain of func" },
#endif
#ifdef ERANGE
    { ERANGE, "RANGE", "Math result not representable" },
#endif
#ifdef EDEADLK
    { EDEADLK, "DEADLK", "Resource deadlock would occur" },
#endif
#ifdef ENAMETOOLONG
    { ENAMETOOLONG, "NAMETOOLONG", "File name too long" },
#endif
#ifdef ENOLCK
    { ENOLCK, "NOLCK", "No record locks available" },
#endif
#ifdef ENOSYS
    { ENOSYS, "NOSYS", "Function not implemented" },
#endif
#ifdef ENOTEMPTY
    { ENOTEMPTY, "NOTEMPTY", "Directory not empty" },
#endif
#ifdef ELOOP
    { ELOOP, "LOOP", "Too many symbolic links encountered" },
#endif
#ifdef EWOULDBLOCK
    { EWOULDBLOCK, "WOULDBLOCK", "Operation would block" },
#endif
#ifdef ENOMSG
    { ENOMSG, "NOMSG", "No message of desired type" },
#endif
#ifdef EIDRM
    { EIDRM, "IDRM", "Identifier removed" },
#endif
#ifdef ECHRNG
    { ECHRNG, "CHRNG", "Channel number out of range" },
#endif
#ifdef EL2NSYNC
    { EL2NSYNC, "L2NSYNC", "Level 2 not synchronized" },
#endif
#ifdef EL3HLT
    { EL3HLT, "L3HLT", "Level 3 halted" },
#endif
#ifdef EL3RST
    { EL3RST, "L3RST", "Level 3 reset" },
#endif
#ifdef ELNRNG
    { ELNRNG, "LNRNG", "Link number out of range" },
#endif
#ifdef EUNATCH
    { EUNATCH, "UNATCH", "Protocol driver not attached" },
#endif
#ifdef ENOCSI
    { ENOCSI, "NOCSI", "No CSI structure available" },
#endif
#ifdef EL2HLT
    { EL2HLT, "L2HLT", "Level 2 halted" },
#endif
#ifdef EBADE
    { EBADE, "BADE", "Invalid exchange" },
#endif
#ifdef EBADR
    { EBADR, "BADR", "Invalid request descriptor" },
#endif
#ifdef EXFULL
    { EXFULL, "XFULL", "Exchange full" },
#endif
#ifdef ENOANO
    { ENOANO, "NOANO", "No anode" },
#endif
#ifdef EBADRQC
    { EBADRQC, "BADRQC", "Invalid request code" },
#endif
#ifdef EBADSLT
    { EBADSLT, "BADSLT", "Invalid slot" },
#endif
#ifdef EDEADLOCK
    { EDEADLOCK, "DEADLOCK", "Resource deadlock would occur" },
#endif
#ifdef EBFONT
    { EBFONT, "BFONT", "Bad font file format" },
#endif
#ifdef ENOSTR
    { ENOSTR, "NOSTR", "Device not a stream" },
#endif
#ifdef ENODATA
    { ENODATA, "NODATA", "No data available" },
#endif
#ifdef ETIME
    { ETIME, "TIME", "Timer expired" },
#endif
#ifdef ENOSR
    { ENOSR, "NOSR", "Out of streams resources" },
#endif
#ifdef ENONET
    { ENONET, "NONET", "Machine is not on the network" },
#endif
#ifdef ENOPKG
    { ENOPKG, "NOPKG", "Package not installed" },
#endif
#ifdef EREMOTE
    { EREMOTE, "REMOTE", "Object is remote" },
#endif
#ifdef ENOLINK
    { ENOLINK, "NOLINK", "Link has been severed" },
#endif
#ifdef EADV
    { EADV, "ADV", "Advertise error" },
#endif
#ifdef ESRMNT
    { ESRMNT, "SRMNT", "Srmount error" },
#endif
#ifdef ECOMM
    { ECOMM, "COMM", "Communication error on send" },
#endif
#ifdef EPROTO
    { EPROTO, "PROTO", "Protocol error" },
#endif
#ifdef EMULTIHOP
    { EMULTIHOP, "MULTIHOP", "Multihop attempted" },
#endif
#ifdef EDOTDOT
    { EDOTDOT, "DOTDOT", "RFS specific error" },
#endif
#ifdef EBADMSG
    { EBADMSG, "BADMSG", "Not a data message" },
#endif
#ifdef EOVERFLOW
    { EOVERFLOW, "OVERFLOW", "Value too large for defined data type" },
#endif
#ifdef ENOTUNIQ
    { ENOTUNIQ, "NOTUNIQ", "Name not unique on network" },
#endif
#ifdef EBADFD
    { EBADFD, "BADFD", "File descriptor in bad state" },
#endif
#ifdef EREMCHG
    { EREMCHG, "REMCHG", "Remote address changed" },
#endif
#ifdef ELIBACC
    { ELIBACC, "LIBACC", "Can not access a needed shared library" },
#endif
#ifdef ELIBBAD
    { ELIBBAD, "LIBBAD", "Accessing a corrupted shared library" },
#endif
#ifdef ELIBSCN
    { ELIBSCN, "LIBSCN", ".lib section in a.out corrupted" },
#endif
#ifdef ELIBMAX
    { ELIBMAX, "LIBMAX", "Attempting to link in too many shared libraries" },
#endif
#ifdef ELIBEXEC
    { ELIBEXEC, "LIBEXEC", "Cannot exec a shared library directly" },
#endif
#ifdef EILSEQ
    { EILSEQ, "ILSEQ", "Illegal byte sequence" },
#endif
#ifdef ERESTART
    { ERESTART, "RESTART", "Interrupted system call should be restarted" },
#endif
#ifdef ESTRPIPE
    { ESTRPIPE, "STRPIPE", "Streams pipe error" },
#endif
#ifdef EUSERS
    { EUSERS, "USERS", "Too many users" },
#endif
#ifdef ENOTSOCK
    { ENOTSOCK, "NOTSOCK", "Socket operation on non-socket" },
#endif
#ifdef EDESTADDRREQ
    { EDESTADDRREQ, "DESTADDRREQ", "Destination address required" },
#endif
#ifdef EMSGSIZE
    { EMSGSIZE, "MSGSIZE", "Message too long" },
#endif
#ifdef EPROTOTYPE
    { EPROTOTYPE, "PROTOTYPE", "Protocol wrong type for socket" },
#endif
#ifdef ENOPROTOOPT
    { ENOPROTOOPT, "NOPROTOOPT", "Protocol not available" },
#endif
#ifdef EPROTONOSUPPORT
    { EPROTONOSUPPORT, "PROTONOSUPPORT", "Protocol not supported" },
#endif
#ifdef ESOCKTNOSUPPORT
    { ESOCKTNOSUPPORT, "SOCKTNOSUPPORT", "Socket type not supported" },
#endif
#ifdef EOPNOTSUPP
    { EOPNOTSUPP, "OPNOTSUPP", "Operation not supported on transport endpoint" },
#endif
#ifdef EPFNOSUPPORT
    { EPFNOSUPPORT, "PFNOSUPPORT", "Protocol family not supported" },
#endif
#ifdef EAFNOSUPPORT
    { EAFNOSUPPORT, "AFNOSUPPORT", "Address family not supported by protocol" },
#endif
#ifdef EADDRINUSE
    { EADDRINUSE, "ADDRINUSE", "Address already in use" },
#endif
#ifdef EADDRNOTAVAIL
    { EADDRNOTAVAIL, "ADDRNOTAVAIL", "Cannot assign requested address" },
#endif
#ifdef ENETDOWN
    { ENETDOWN, "NETDOWN", "Network is down" },
#endif
#ifdef ENETUNREACH
    { ENETUNREACH, "NETUNREACH", "Network is unreachable" },
#endif
#ifdef ENETRESET
    { ENETRESET, "NETRESET", "Network dropped connection because of reset" },
#endif
#ifdef ECONNABORTED
    { ECONNABORTED, "CONNABORTED", "Software caused connection abort" },
#endif
#ifdef ECONNRESET
    { ECONNRESET, "CONNRESET", "Connection reset by peer" },
#endif
#ifdef ENOBUFS
    { ENOBUFS, "NOBUFS", "No buffer space available" },
#endif
#ifdef EISCONN
    { EISCONN, "ISCONN", "Transport endpoint is already connected" },
#endif
#ifdef ENOTCONN
    { ENOTCONN, "NOTCONN", "Transport endpoint is not connected" },
#endif
#ifdef ESHUTDOWN
    { ESHUTDOWN, "SHUTDOWN", "Cannot send after transport endpoint shutdown" },
#endif
#ifdef ETOOMANYREFS
    { ETOOMANYREFS, "TOOMANYREFS", "Too many references: cannot splice" },
#endif
#ifdef ETIMEDOUT
    { ETIMEDOUT, "TIMEDOUT", "Connection timed out" },
#endif
#ifdef ECONNREFUSED
    { ECONNREFUSED, "CONNREFUSED", "Connection refused" },
#endif
#ifdef EHOSTDOWN
    { EHOSTDOWN, "HOSTDOWN", "Host is down" },
#endif
#ifdef EHOSTUNREACH
    { EHOSTUNREACH, "HOSTUNREACH", "No route to host" },
#endif
#ifdef EALREADY
    { EALREADY, "ALREADY", "Operation already in progress" },
#endif
#ifdef EINPROGRESS
    { EINPROGRESS, "INPROGRESS", "Operation now in progress" },
#endif
#ifdef ESTALE
    { ESTALE, "STALE", "Stale NFS file handle" },
#endif
#ifdef EUCLEAN
    { EUCLEAN, "UCLEAN", "Structure needs cleaning" },
#endif
#ifdef ENOTNAM
    { ENOTNAM, "NOTNAM", "Not a XENIX named type file" },
#endif
#ifdef ENAVAIL
    { ENAVAIL, "NAVAIL", "No XENIX semaphores available" },
#endif
#ifdef EISNAM
    { EISNAM, "ISNAM", "Is a named type file" },
#endif
#ifdef EREMOTEIO
    { EREMOTEIO, "REMOTEIO", "Remote I/O error" },
#endif
#ifdef EDQUOT
    { EDQUOT, "DQUOT", "Quota exceeded" },
#endif
#ifdef ENOMEDIUM
    { ENOMEDIUM, "NOMEDIUM", "No medium found" },
#endif
#ifdef EMEDIUMTYPE
    { EMEDIUMTYPE, "MEDIUMTYPE", "Wrong medium type" },
#endif
#ifdef EUTF8
    { EUTF8, "UTF8", "Invalid UTF-8 byte sequence" },
#endif
};

#define NUM_FILE_ERRORS	(sizeof (fileErrorMap) / sizeof (fileErrorMap[0]))

Type    *typeFileError;

static int
FileInitErrors (void)
{
    ENTER ();
    StructType	    *st;
    Atom	    *atoms;
    int		    i;
    SymbolPtr	    error_type;

    error_type = NewSymbolType (AtomId("error_type"), 0);
    st = NewStructType (NUM_FILE_ERRORS);
    atoms = StructTypeAtoms (st);
    for (i = 0; i < NUM_FILE_ERRORS; i++)
    {
	AddBoxType (&st->types, typePrim[rep_void]);
	atoms[i] = AtomId (fileErrorMap[i].name);
    }
    error_type->symbol.type = NewTypeUnion (st, True);
    typeFileError = NewTypeName (NewExprAtom (AtomId ("error_type"), 0, False),
				 error_type);
    MemAddRoot (typeFileError);
    EXIT ();
    return 1;
}

volatile Bool	signalChild;

static void
sigchld (int sig)
{
    resetSignal (SIGCHLD, sigchld);
    SetSignalChild ();
}

void
ProcessInterrupt ()
{
    for (;;) {
	pid_t		pid;
	int		status;

	pid = wait3 (&status, WNOHANG, NULL);
	if (pid == 0)
	    break;
	if (pid < 0 && errno == ECHILD)
	    break;
    }
}

static FileChainPtr
FileChainAlloc (FileChainPtr next, int size)
{
    FileChainPtr	ret;

    ret = malloc (sizeof (FileChain) + size);
    ret->next = next;
    ret->size = size;
    ret->used = 0;
    ret->ptr = 0;
    return ret;
}

static void
FileChainFree (FileChainPtr ic)
{
    while (ic)
    {
	FileChainPtr next = ic->next;
	free (ic);
	ic = next;
    }
}

int
FileInit (void)
{
    ENTER ();
    catchSignal (SIGCHLD, sigchld);
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
    Value	    ret;
    int		    i;
    StructType	    *st;

    for (i = 0; i < NUM_FILE_ERRORS; i++)
	if (fileErrorMap[i].value == err)
	    break;
    if (i == NUM_FILE_ERRORS)
	i = 0;	    /* XXX weird error */
    st = TypeCanon (typeFileError)->structs.structs;
    ret = NewUnion (st, True);
    ret->unions.tag = StructTypeAtoms(st)[i];
    BoxValueSet (ret->unions.value,0,Void);
    RETURN (ret);
}

Value
FileGetErrorMessage (int err)
{
    int i;
    for (i = 0; i < NUM_FILE_ERRORS; i++)
	if (fileErrorMap[i].value == err)
	    return NewStrString (fileErrorMap[i].message);
    return NewStrString ("Unknown error");
}

static void
FileMark (void *object)
{
    File    *file = object;

    FileFlush ((Value) file, False);
    MemReference (file->next);
}

void
FileFini (void)
{
    MemCollect ();
    while (anyFileWriteBlocked)
	FileCheckBlocked (True);
}

static int
FileFree (void *object)
{
    File    *file = object;

    if (file->fd == -1 || FileClose ((Value) file) != FileBlocked)
    {
	FileChainFree (file->input);
	file->input = NULL;
	FileChainFree (file->output);
	file->output = NULL;
	return 1;
    }
    return 0;
}

static Bool
FilePrint (Value f, Value av, char format, int base, int width, int prec, int fill)
{
    FilePuts (f, "file");
    return True;
}

ValueRep FileRep = {
    { FileMark, FileFree, "FileRep" },
    rep_file, 
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

    ret = ALLOCATE (&FileRep.data, sizeof (File));
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
    flags |= ASYNC;
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
    flags &= ~ASYNC;
    (void) fcntl (fd, F_SETFL, flags);
#ifdef  USE_STREAMS_ASYNC
    (void) ioctl(fd, I_SETSIG, 0);
#endif
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
    else if (lseek (fd, 0, 1) < 0)
	file->file.flags |= FileIsPipe;
    if (fd >= 3)
	FileSetFd (fd);
    file->file.sock_family = 0;
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
	oflags |= O_CREAT|O_APPEND;
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
FileReopen (char *name, char *mode, Value file, int *errp)
{
    ENTER ();
    int	    oflags = 0;
    int	    flags = 0;
    int	    fd;

    if (file->file.flags & FileString)
    {
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("Reopen: string file"),
				NewInt (0), file);
	RETURN (Void);
    }
	
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
    if (dup2 (fd, file->file.fd) < 0)
    {
	*errp = errno;
	close (fd);
	RETURN (0);
    }
    close (fd);
    RETURN (file);
}

Value
FileFilter (char *program, char *args[], Value filev, int *errp)
{
    ENTER ();
    int	    pid;
    int     errcode, nread;
    int	    i;
    int     errpipe[2];
    int     fdlimit;
    int     fds[3];

    /* set up process files */
    for (i = 0; i < 3; i++) {
	Value f = ArrayValue (&filev->array, i);
	if (i == 0 && !(f->file.flags & FileReadable)) {
	    RaiseStandardException (exception_invalid_argument, 3,
				    NewStrString ("File::filter: process input not readable"),
				    NewInt (i), f);
	    RETURN (Void);
	}
	if (i == 1 && !(f->file.flags & FileWritable)) {
	    RaiseStandardException (exception_invalid_argument, 3,
				    NewStrString ("File::filter: process output not writable"),
				    NewInt (i), f);
	    RETURN (Void);
	}
	if (i == 2 && !(f->file.flags & FileWritable)) {
	    RaiseStandardException (exception_invalid_argument, 3,
				    NewStrString ("File::filter: process error not writable"),
				    NewInt (i), f);
	    RETURN (Void);
	}
	fds[i] = f->file.fd;
    }

    if (pipe (errpipe) < 0) {
	*errp = errno;
	RETURN (0);
    }
    pid = fork ();
    if (pid == -1) {
	close (errpipe[0]);
	close (errpipe[1]);
	*errp = errno;
	RETURN (0);
    }
    if (pid == 0) {
	/* child */
	for (i = 0; i < 3; i++)
	    dup2 (fds[i], i);
	fdlimit = sysconf(_SC_OPEN_MAX);
	for (; i < fdlimit; i++)
	    if (i != errpipe[1])
		close (i);
	fcntl (errpipe[1], F_SETFD, FD_CLOEXEC);
	execvp (program, args);
	errcode = errno & 0xff;
	errcode = write (errpipe[1], &errcode, 1);
	(void) errcode;
	exit (1);
    }
    /* parent */
    close (errpipe[1]);
    nread = read(errpipe[0], &errcode, 1);
    close (errpipe[0]);
    if (nread != 0) {
	*errp = errcode;
	assert (nread == 1);
	RETURN(0);
    }
    for (i = 0; i < 3; i++) {
	Value f = ArrayValue (&filev->array, i);
	if (f->file.flags & FilePipe)
	    f->file.pid = pid;
    }
    RETURN (NewInt(pid));
}

Value
FileMakePipe (int *errp)
{
    ENTER ();
    Value   file, files;
    int	    two = 2;
    int     fds[2];

    if (pipe (fds) < 0) {
	*errp = errno;
	RETURN (0);
    }

    /* gather and return results */
    files = NewArray (False, False, typePrim[rep_file], 1, &two);
    file = FileCreate (fds[0], FileReadable);
    file->file.flags |= FilePipe;
    ArrayValueSet (&files->array, 0, file);
    file = FileCreate (fds[1], FileWritable);
    file->file.flags |= FilePipe;
    ArrayValueSet (&files->array, 1, file);
    RETURN (files);
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
    return FileFlush (file, False);
}

Value
FileStringRead (char *string, int len)
{
    ENTER ();
    Value   file;

    file = NewFile (-1);
    file->file.flags |= FileString|FileReadable;
    file->file.input = FileChainAlloc (0, len);
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

static char *
write_chain(char *s, FileChainPtr out)
{
    if (!out)
	return s;
    s = write_chain(s, out->next);
    memcpy(s, FileBuffer(out), out->used);
    return s + out->used;
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
	RaiseStandardException (exception_invalid_argument, 3,
				NewStrString ("string_string: not string file"),
				NewInt (0), file);
	RETURN (Void);
    }
    len = 0;
    for (out = file->file.output; out; out = out->next)
	len += out->used;
    str = NewString (len);
    StringChars (&str->string);
    s = write_chain(StringChars(&str->string), file->file.output);
    *s = '\0';
    RETURN (str);
}

#define DontBlockIO	(runnable && running)

Bool
FileIsReadable (int fd)
{
    fd_set	    bits;
    int		    n;
    struct timeval  tv;

    if (fd == 0 && !stdinOwned)
    {
	if (!stdinPolling)
	    IoNoticeTtyUnowned ();
	return False;
    }
    do
    {
	FD_ZERO (&bits);
	FD_SET (fd, &bits);
	tv.tv_usec = 0;
	tv.tv_sec = 0;
	n = select (fd + 1, &bits, 0, 0, &tv);
    } while (n < 0 && errno == EINTR);
    return n > 0;
}

Bool
FileIsWritable (int fd)
{
    fd_set	    bits;
    int		    n;
    struct timeval  tv;

    do
    {
	FD_ZERO (&bits);
	FD_SET (fd, &bits);
	tv.tv_usec = 0;
	tv.tv_sec = 0;
	n = select (fd + 1, 0, &bits, 0, &tv);
    } while (n < 0 && errno == EINTR);
    return n > 0;
}

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
	    file->file.flags |= FileEnd;
	    EXIT ();
	    return FileEOF;
	}
	file->file.input = FileChainAlloc (NULL, FileBufferSize);
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
	    {
		file->file.input = ic->next;
		ic->next = NULL;
		FileChainFree (ic);
		ic = file->file.input;
	    }
	    else if (file->file.flags & FileString)
	    {
		file->file.flags |= FileEnd;
		c = FileEOF;
		break;
	    }
	    else
	    {
		buf = FileBuffer (ic);
		if (FileIsReadable (file->file.fd))
		{
		    n = ic->size;
		    if (file->file.flags & FileUnBuf)
			n = 1;
		    n = read (file->file.fd, buf, n);
		    err = errno;
		    file->file.flags &= ~FileEnd;
		}
		else
		{
		    n = -1;
		    err = EWOULDBLOCK;
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
	ic = file->file.input = FileChainAlloc (file->file.input, FileBufferSize);
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
FileFlushChain (Value file, FileChainPtr ic, Bool block)
{
    int	    n;
    int	    err;

    while (ic->ptr < ic->used)
    {
	if (FileIsWritable (file->file.fd))
	{
	    n = write (file->file.fd, &FileBuffer(ic)[ic->ptr], ic->used - ic->ptr);
	    err = errno;
	}
	else
	{
	    n = -1;
	    err = EWOULDBLOCK;
	}
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
	    if (!(file->file.flags & FileBlockWrites) && !block)
	    {
		FileSetBlocked (file, FileOutputBlocked);
		return FileBlocked;
	    }
	    FileWaitForWriteable (file);
	}
    }
    return 0;
}

/*
 * May not allocate memory as it is called while garbage collecting
 */

int
FileFlush (Value file, Bool block)
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
		n = FileFlushChain (file, ic, block);
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
		else
		    FileChainFree (ic);
		*prev = 0;
	    }
	}
    }
    if (file->file.flags & FileClosed && n != FileBlocked)
    {
	if (file->file.fd != -1)
	{
	    FileResetFd (file->file.fd);
	    close (file->file.fd);
	    file->file.fd = -1;
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
	ic = file->file.output = FileChainAlloc (0, FileBufferSize);
    if (ic->used == ic->size)
    {
	if (FileFlush (file, False) == FileError)
	{
	    EXIT ();
	    return FileError;
	}
	ic = file->file.output;
	if (ic->used == ic->size)
	    ic = file->file.output = FileChainAlloc (file->file.output, FileBufferSize);
    }
    ic = file->file.output;
    FileBuffer(ic)[ic->used++] = c;
    if ((c == '\n' && file->file.flags & FileLineBuf) ||
	file->file.flags & FileUnBuf)
    {
	if (FileFlush (file, False) == FileError)
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
FilePutsc (Value file, char *s, long length)
{
    while (length--)
	FileOutput (file, *s++);
}

void
FilePutDoubleDigitBase (Value file, double_digit a, int base)
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
FilePutUIntBase (Value file, unsigned int a, int base)
{
    FilePutDoubleDigitBase (file, (double_digit) a, base);
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
FileStringWidth (char *string, long length, char format)
{
    if (format == 's')
	return StringLength (string, length);
    else
    {
	int	    width = 2;
	unsigned    c;
	while ((string = StringNextChar (string, &c, &length)))
	{
	    if (c < ' ' || '~' < c)
		switch (c) {
		case '\n':
		case '\r':
		case '\t':
		case '\b':
		case '\f':
		case '\v':
		case '\0':
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
FilePutString (Value f, char *string, long length, char format)
{
    if (format == 's')
	FilePutsc (f, string, length);
    else
    {
	unsigned c;
	FileOutput (f, '"');
	while ((string = StringNextChar (string, &c, &length))) 
	{
	    if (c < ' ')
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
		case '\v':
		    FilePuts (f, "\\v");
		case '\0':
		    FilePuts (f, "\\0");
		    break;
		default:
		    FileOutput (f, '\\');
		    Print (f, NewInt (c), 'o', 8, 3, -1, '0');
		    break;
		}
	    else if (c == '"')
		FilePuts (f, "\\\"");
	    else if (c == '\\')
		FilePuts (f, "\\\\");
	    else
	    {
		char	dest[7];
		int l = StringPutChar (c, dest);
		dest[l] = '\0';
		FilePuts (f, dest);
	    }
	}
	FileOutput (f, '"');
    }
}

void
FilePutRep (Value f, Rep tag, Bool minimal)
{
    switch (tag) {
    case rep_undef:
	if (!minimal)
	    FilePuts (f, "poly");
	break;
    case rep_int:
    case rep_integer:
	FilePuts (f, "int");
	break;
    case rep_rational:
	FilePuts (f, "rational");
	break;
    case rep_float:
	FilePuts (f, "real");
	break;
    case rep_string:
	FilePuts (f, "string");
	break;
    case rep_file:
	FilePuts (f, "file");
	break;
    case rep_thread:
	FilePuts (f, "thread");
	break;
    case rep_semaphore:
	FilePuts (f, "semaphore");
	break;
    case rep_continuation:
	FilePuts (f, "continuation");
	break;
    case rep_bool:
	FilePuts (f, "bool");
	break;
    case rep_foreign:
	FilePuts (f, "foreign");
	break;
    case rep_void:
	FilePuts (f, "void");
	break;
	
    case rep_array:
	FilePuts (f, "array");
	break;
    case rep_ref:
	FilePuts (f, "ref");
	break;
    case rep_struct:
	FilePuts (f, "struct");
	break;
    case rep_func:
	FilePuts (f, "function");
	break;
    default:
	FilePrintf (f, "bad type %d", tag);
	break;
    }
    if (minimal && tag != rep_undef)
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
FilePutArgType (Value f, ArgType *at)
{
    FilePuts (f, "(");
    while (at)
    {
	if (at->type)
	    FilePutType (f, at->type, at->name != 0);
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
FilePutDimensions (Value f, ExprPtr dims, Bool resizable)
{
    while (dims)
    {
	if (dims->tree.left)
	    PrettyExpr (f, dims->tree.left, -1, 0, False);
	else if (resizable)
	    FilePuts (f, "...");
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
FilePutBaseType (Value f, Type *t, Bool minimal)
{
    switch (t->base.tag) {
    case type_func:
	FilePutBaseType (f, t->func.ret, minimal);
	break;
    case type_array:
	FilePutBaseType (f, t->array.type, minimal);
	break;
    case type_hash:
	FilePutBaseType (f, t->hash.type, minimal);
	break;
    default:
	FilePutType (f, t, minimal);
	break;
    }
}

void
FilePutSubscriptType (Value f, Type *t, Bool minimal)
{
    switch (t->base.tag) {
    case type_func:
	FilePutArgType (f, t->func.args);
	FilePutSubscriptType (f, t->func.ret, minimal);
	break;
    case type_array:
	FilePuts (f, "[");
	FilePutDimensions (f, t->array.dimensions, t->array.resizable);
	FilePuts (f, "]");
	FilePutSubscriptType (f, t->array.type, minimal);
	break;
    case type_hash:
	FilePuts (f, "[");
	FilePutType (f, t->hash.keyType, False);
	FilePuts (f, "]");
	FilePutSubscriptType (f, t->hash.type, minimal);
	break;
    default:
	break;
    }
}

void
FilePutType (Value f, Type *t, Bool minimal)
{
    int		    i;
    StructType	    *st;
    Bool	    spaceit = minimal;
    TypeElt	    *elt;
    
    if (!t)
    {
	FilePuts (f, "<undefined>");
	return;
    }
    switch (t->base.tag) {
    case type_prim:
	if (t->prim.prim != rep_undef || !minimal)
	    FilePutRep (f, t->prim.prim, False);
	else
	    spaceit = False;
	break;
    case type_name:
	FilePutTypename (f, t->name.expr);
	break;
    case type_ref:
	if (t->ref.pointer)
	    FilePuts (f, "*");
	else
	    FilePuts (f, "&");
	FilePutType (f, t->ref.ref, False);
	break;
    case type_func:
    case type_array:
    case type_hash:
	FilePutBaseType (f, t, False);
	FilePutSubscriptType (f, t, False);
	break;
    case type_struct:
    case type_union:
	if (t->structs.left && t->structs.right)
	{
	    FilePutType (f, t->structs.left, False);
	    FilePuts (f, " + ");
	    FilePutType (f, t->structs.right, False);
	}
	else if (t->structs.enumeration)
	{
	    FilePuts (f, "enum { ");
	    st = t->structs.structs;
	    for (i = 0; i < st->nelements; i++)
	    {
		if (i)
		    FilePuts (f, ", ");
		FilePuts (f, AtomName (StructTypeAtoms(st)[i]));
	    }
	    FilePuts (f, " }");
	}
	else
	{
	    if (t->base.tag == type_struct)
		FilePuts (f, "struct { ");
	    else
		FilePuts (f, "union { ");
	    st = t->structs.structs;
	    for (i = 0; i < st->nelements; i++)
	    {
		FilePutType (f, BoxTypesElements(st->types)[i], True);
		FilePuts (f, AtomName (StructTypeAtoms (st)[i]));
		FilePuts (f, "; ");
	    }
	    FilePuts (f, "}");
	}
	break;
    case type_types:
	for (elt = t->types.elt; elt; elt = elt->next)
	{
	    FilePutType (f, elt->type, False);
	    if (elt->next)
		FilePuts (f, ", ");
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
	    case 'D':
		FilePutDoubleDigitBase (file, va_arg (args, double_digit), 10);
		break;
	    case 'v':
	    case 'g':
		v = va_arg (args, Value);
		if (!v)
		    (void) FilePuts (file, "<uninit>");
		else
		    Print (file, v, *fmt, 0, 0, DEFAULT_OUTPUT_PRECISION, ' ');
		break;
	    case 'G':
		v = va_arg (args, Value);
		if (!v)
		    (void) FilePuts (file, "<uninit>");
		else {
		    if (ValueRep(v)->tag <= rep_void) {
			Print (file, v, 'g', 0, 0, DEFAULT_OUTPUT_PRECISION, ' ');
		    } else {
			(void) FilePuts (file, "<composite>");
		    }
		}
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
	    case 'S': {
		char *s = va_arg (args, char *);
		FilePutString (file, s, strlen(s), 'v');
		break;
	    }
	    case 'A':
		(void) FilePuts (file, AtomName (va_arg (args, Atom)));
		break;
	    case 't':
		FilePutType (file, va_arg (args, Type *), True);
		break;
	    case 'T':
		FilePutType (file, va_arg (args, Type *), False);
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
		(void) FileOutchar (file, va_arg (args, int));
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
FileCheckBlocked (Bool block)
{
    ENTER ();
    fd_set	    readable, writable;
    int		    n, fd;
    Value	    blocked, *prev;
    Bool	    ready;
    Bool	    writeBlocked;
#ifdef NO_PIPE_SIGIO
    Bool	    readPipeBlocked;
#endif
    
    FD_ZERO (&readable);
    FD_ZERO (&writable);
    n = 0;
    for (prev = &fileBlocked; (blocked = *prev); )
    {
	fd = blocked->file.fd;
	if (fd < 0)
	{
	    *prev = blocked->file.next;
	    continue;
	}
	prev = &blocked->file.next;
	if (fd == 0 && !stdinOwned)
	    continue;
	if (blocked->file.flags & FileInputBlocked)
	    FD_SET (fd, &readable);
	if (blocked->file.flags & FileOutputBlocked)
	    FD_SET (fd, &writable);
	if (fd >= n)
	    n = fd + 1;
    }
    if (n > 0)
    {
	struct timeval  tv, *tvp;
	if (block)
	    tvp = 0;
	else
	{
	    tv.tv_usec = 0;
	    tv.tv_sec = 0;
	    tvp = &tv;
	}
	n = select (n, &readable, &writable, 0, tvp);
    }
    else
    {
	anyFileWriteBlocked = False;
#ifdef NO_PIPE_SIGIO
	anyPipeReadBlocked = False;
#endif
	if (block) {
	    sigset_t	    set, oset;
	    sigfillset (&set);
	    sigprocmask (SIG_SETMASK, &set, &oset);
	    if (!signaling && !running)
		sigsuspend(&oset);
	    sigprocmask (SIG_SETMASK, &oset, &set);
	}
    }
    if (n > 0)
    {
	writeBlocked = False;
#ifdef NO_PIPE_SIGIO
	readPipeBlocked = False;
#endif
	if (block)
	    signaling = True;
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
		if (FileFlush (blocked, False) != FileBlocked)
		{
		    blocked->file.flags &= ~FileOutputBlocked;
		    ready = True;
		}
	    }
	    if (blocked->file.flags & FileOutputBlocked)
		writeBlocked = True;
#ifdef NO_PIPE_SIGIO
	    if (blocked->file.flags & FileInputBlocked &&
		blocked->file.flags & FileIsPipe)
		readPipeBlocked = True;
#endif
	    if (ready)
		ThreadsWakeup (blocked, WakeAll);
	    if ((blocked->file.flags & (FileOutputBlocked|FileInputBlocked)) == 0)
		*prev = blocked->file.next;
	    else
		prev = &blocked->file.next;
	}
	anyFileWriteBlocked = writeBlocked;
#ifdef NO_PIPE_SIGIO
	anyPipeReadBlocked = readPipeBlocked;
#endif
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
#ifdef NO_PIPE_SIGIO
    if (flag == FileInputBlocked && 
	(file->file.flags & FileIsPipe) && 
	!anyPipeReadBlocked)
    {
	anyPipeReadBlocked = True;
	IoNoticeReadBlocked ();
    }
#endif
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

/*
 * Output one character in UTF-8 format
 */

int
FileOutchar (Value file, int c)
{
    char d;
    int	bits;
    
         if (c <       0x80) { d = c;                         bits= -6; }
    else if (c <      0x800) { d= ((c >>  6) & 0x1F) | 0xC0;  bits=  0; }
    else if (c <    0x10000) { d= ((c >> 12) & 0x0F) | 0xE0;  bits=  6; }
    else if (c <   0x200000) { d= ((c >> 18) & 0x07) | 0xF0;  bits= 12; }
    else if (c <  0x4000000) { d= ((c >> 24) & 0x03) | 0xF8;  bits= 18; }
    else if (c < 0x80000000) { d= ((c >> 30) & 0x01) | 0xFC;  bits= 24; }
    else return FileError;

    if (FileOutput (file, d) < 0)
	return FileError;
    
    for ( ; bits >= 0; bits-= 6)
	if (FileOutput (file, ((c >> bits) & 0x3F) | 0x80) < 0)
	    return FileError;

    return 0;
}

int
FileInchar (Value file)
{
    char    buf[6];
    int	    n = 0;
    int	    result;
    int	    mask;
    int	    extra;

    result = FileInput (file);
    if (result < 0)
	return result;
    
    buf[n++] = result;
    if ((result & 0x80) != 0)
    {
	if ((result & 0xc0) != 0xc0)
	{
	    file->file.input_errno = EUTF8;
	    return FileError;
	}
	
	mask = 0x20;
	extra = 1;
	while ((result & mask) != 0)
	{
	    extra++;
	    mask >>= 1;
	}
	result &= (mask - 1);
	while (extra-- > 0)
	{
	    int c = FileInput (file);
	    if (c < 0)
	    {
		while (--n >= 0)
		    FileUnput (file, buf[n]);
		return c;
	    }
	    buf[n++] = c;
	    if ((c & 0xc0) != 0x80)
	    {
		file->file.input_errno = EUTF8;
		return FileError;
	    }
	    result = (result << 6) | (c & 0x3f);
	}
    }
    return result;
}

void
FileUnchar (Value file, int c)
{
    char d;
    int	bits;
    
         if (c <       0x80) { d = c;                         bits= -6; }
    else if (c <      0x800) { d= ((c >>  6) & 0x1F) | 0xC0;  bits=  0; }
    else if (c <    0x10000) { d= ((c >> 12) & 0x0F) | 0xE0;  bits=  6; }
    else if (c <   0x200000) { d= ((c >> 18) & 0x07) | 0xF0;  bits= 12; }
    else if (c <  0x4000000) { d= ((c >> 24) & 0x03) | 0xF8;  bits= 18; }
    else if (c < 0x80000000) { d= ((c >> 30) & 0x01) | 0xFC;  bits= 24; }
    else return;

    for ( ; bits >= 0; bits-= 6)
    {
	FileUnput (file, (c & 0x3F) | 0x80);
	c >>= 6;
    }
    FileUnput (file, d);
}
