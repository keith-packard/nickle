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
#include	"nickle.h"
#include	"ref.h"

volatile Bool	signalIo;
Bool		stdinOwned;
Bool		stdinPolling;
Bool		ioTimeoutQueued;
Bool		anyFileWriteBlocked;

#ifdef HAVE_SIGACTION
#define RESTART_SIGNAL(sig,func)
#else
#define RESTART_SIGNAL(sig,func) (void) signal (sig,func)
#endif

static void
sigio (int sig)
{
    resetSignal (SIGIO, sigio);
    SetSignalIo ();
}

void
IoInterrupt (void)
{
    FileCheckBlocked (False);
}

void
IoStop (void)
{
    if (stdin_interactive)
    {
	FileResetFd (0);
	stdinOwned = False;
    }
}

#ifdef GETPGRP_VOID
#define GetPgrp()   getpgrp()
#else
#define GetPgrp()   getpgrp(0)
#endif

static Bool
IoOwnTty (int fd)
{
    int	tpgrp;
    
    tpgrp = tcgetpgrp (fd);
    if (tpgrp == -1 || tpgrp == GetPgrp())
	return True;
    return False;
}

void
IoStart (void)
{
    if (stdin_interactive) 
    {
	stdinOwned = IoOwnTty (0);
	if (stdinOwned)
	{
	    stdinPolling = False;
	    FileSetFd (0);
	}
    }
    else
	stdinOwned = True;
}

void
IoFini (void)
{
    FileStdin->file.flags |= FileBlockWrites;
    FileClose (FileStdin);
    FileStdout->file.flags |= FileBlockWrites;
    FileClose (FileStdout);
    FileStderr->file.flags |= FileBlockWrites;
    FileClose (FileStderr);
}

BoxPtr   FileStdinBox, FileStdoutBox, FileStderrBox;

Bool
IoTimeout (void *closure)
{
    if (!stdinOwned)
	IoStart ();
    FileCheckBlocked (False);
    if (anyFileWriteBlocked || (!stdinOwned && stdinPolling)
#ifdef NO_PIPE_SIGIO
	|| anyPipeReadBlocked 
#endif
	)
	return True;
    ioTimeoutQueued = False;
    return False;
}

static void
IoSetupTimeout (void)
{
    if (!ioTimeoutQueued)
    {
	ioTimeoutQueued = True;
	TimerInsert (0, IoTimeout, 100, 100);
    }
}
    
void
IoNoticeTtyUnowned (void)
{
    if (!stdinOwned && !stdinPolling)
    {
	stdinPolling = True;
	IoSetupTimeout();
    }
}

void
IoNoticeWriteBlocked (void)
{
    IoSetupTimeout ();
}

#ifdef NO_PIPE_SIGIO
void
IoNoticeReadBlocked (void)
{
    IoSetupTimeout ();
}
#endif

static BoxPtr
IoMakeFile(int fd, int flags)
{
    BoxPtr	box = NewBox(False, False, 1, typePrim[rep_file]);
    MemAddRoot(box);
    BoxValueSet(box, 0, FileCreate(fd, flags));
    return box;
}

void
IoInit (void)
{
    ENTER ();
    catchSignal (SIGIO, sigio);
    FileStdinBox = IoMakeFile(0, FileReadable);
    FileStdoutBox = IoMakeFile(1, FileWritable);
    FileStderrBox = IoMakeFile(2, FileWritable);
    IoStart ();
    EXIT ();
}
