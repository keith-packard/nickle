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
#include	"nickle.h"
#include	"ref.h"

volatile Bool	signalIo;
Bool		ownTty[3];
Bool		anyTtyUnowned;
Bool		ioTimeoutQueued;

#ifdef HAS_SIGACTION
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
    FileCheckBlocked ();
}

void
IoStop (void)
{
    int	fd;

    for (fd = 0; fd < 3; fd++)
    {
	ownTty[fd] = False;
	FileResetFd (fd);
    }
    anyTtyUnowned = True;
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
    int	fd;
    
    anyTtyUnowned = False;
    for (fd = 0; fd < 3; fd++)
    {
        ownTty[fd] = IoOwnTty (fd);
        if (!ownTty[fd])
	    anyTtyUnowned = True;
    }
    if (anyTtyUnowned)
	IoNoticeTtyUnowned ();
    else if (stdin_interactive)
	for (fd = 0; fd < 3; fd++)
	    FileSetFd (fd);
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
    if (anyTtyUnowned)
	IoStart ();
    FileCheckBlocked ();
    if (anyFileWriteBlocked || anyFileReadBlocked || anyTtyUnowned)
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
    IoSetupTimeout();
}

void
IoNoticeWriteBlocked (void)
{
    IoSetupTimeout ();
}

void
IoNoticeReadBlocked (void)
{
    IoSetupTimeout ();
}

void
IoInit (void)
{
    ENTER ();
    catchSignal (SIGIO, sigio);
    FileStdin = FileCreate (0);
    FileStdout = FileCreate (1);
    FileStderr = FileCreate (2);
    MemAddRoot (FileStdin);
    MemAddRoot (FileStdout);
    MemAddRoot (FileStderr);
    IoStart ();
    EXIT ();
}
