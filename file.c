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
#include	<errno.h>
#include	"nickle.h"
#include	"ref.h"

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

int
FileInit (void)
{
    ENTER ();
    fileBlockedReference = NewReference ((void **) &fileBlocked);
    MemAddRoot (fileBlockedReference);
    EXIT ();
    return 1;
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
    ret->value.tag = type_file;
    ret->file.fd = fd;
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
FileCreate (int fd)
{
    ENTER ();
    Value   file;

    file = NewFile (fd);
    if (isatty (fd))
	file->file.flags |= FileLineBuf;
    if (fd >= 3)
	FileSetFd (fd);
    RETURN (file);
}

Value
FileFopen (char *name, char *mode)
{
    ENTER ();
    int	    flags = 0;
    int	    fd;
    
    switch (mode[0]) {
    case 'r':
	if (mode[1] == '+')
	    flags = 2;
	else
	    flags = 0;
	break;
    case 'w':
	if (mode[1] == '+')
	    flags = 2;
	else
	    flags = 1;
	flags |= O_TRUNC|O_CREAT;
	break;
    case 'a':
	if (mode[1] == '+')
	    flags = 2;
	else
	    flags = 1;
	flags |= O_TRUNC|O_CREAT|O_APPEND;
	break;
    }
    fd = open (name, flags, 0666);
    if (fd < 0)
	RETURN (0);
    RETURN (FileCreate (fd));
}

int
FileClose (Value file)
{
    file->file.flags |= FileClosed;
    return FileFlush (file);
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
	c = FileError;
    else
    {
	if (!file->file.input)
	    file->file.input = NewFileChain (0, FileBufferSize);
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
			    c = FileError;
			}
			break;
		    }
		    ic->ptr = 0;
		    ic->used = n;
		}
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
		return FileError;
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
    FileChainPtr	ic, *prev;
    int		n;

    if (!file->file.output)
	n = 0;
    else
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
    }
    EXIT ();
    return n;
}

void
FileOutput (Value file, char c)
{
    ENTER ();
    FileChainPtr	ic;

    if (file->file.flags & FileClosed)
	return;
    ic = file->file.output;
    if (!ic)
	ic = file->file.output = NewFileChain (0, FileBufferSize);
    if (ic->used == ic->size)
	FileFlush (file);
    ic = file->file.output;
    FileBuffer(ic)[ic->used++] = c;
    if ((c == '\n' && file->file.flags & FileLineBuf) ||
	file->file.flags & FileUnBuf)
	FileFlush (file);
    EXIT ();
}

void
FilePuts (Value file, char *s)
{
    while (*s)
	FileOutput (file, *s++);
}

static void
FilePutIntBase (Value file, int a, int base)
{
    int	    digit;
    char    space[64], *s;
    int	    neg;

    neg = 0;
    if (a < 0)
    {
	a = -a;
	neg = 1;
    }
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
	if (neg)
	    *--s = '-';
    }
    FilePuts (file, s);
}

void
FilePutInt (Value file, int a)
{
    FilePutIntBase (file, a, 10);
}

void
FilePrintf (Value file, char *fmt, ...)
{
    Value   v;
    va_list args;
    
    va_start (args, fmt);
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
	    case 'o':
		FilePutIntBase (file, va_arg (args, int), 8);
		break;
	    case 'x':
		FilePutIntBase (file, va_arg (args, int), 16);
		break;
	    case 'v':
		v = va_arg (args, Value);
		if (!v)
		    (void) FilePuts (file, "(null)");
		else
		    Print (file, v, 'v', 0, 0, DEFAULT_OUTPUT_PRECISION, ' ');
		break;
	    case 's':
		(void) FilePuts (file, va_arg (args, char *));
		break;
	    case 'A':
		(void) FilePuts (file, AtomName (va_arg (args, Atom)));
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
		ThreadsWakeup (blocked);
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
