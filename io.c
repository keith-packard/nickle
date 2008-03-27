/* $Header$ */

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

static RETSIGTYPE
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

Value   FileStdin, FileStdout, FileStderr;

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

void
IoInit (void)
{
    ENTER ();
    catchSignal (SIGIO, sigio);
    FileStdin = FileCreate (0, FileReadable);
    FileStdout = FileCreate (1, FileWritable);
    FileStderr = FileCreate (2, FileWritable);
    MemAddRoot (FileStdin);
    MemAddRoot (FileStdout);
    MemAddRoot (FileStderr);
    IoStart ();
    EXIT ();
}
