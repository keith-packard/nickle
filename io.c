/*
 * $Id$
 *
 * Copyright 1996 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include    <unistd.h>
#include    <fcntl.h>
#include    <signal.h>
#include    <sys/time.h>
#include    <sys/types.h>
#include    "nick.h"
#include    "ref.h"

volatile Bool	abortIo;
Bool		ownTty[3];
Bool		anyTtyUnowned;
Bool		ioTimeoutQueued;

void
sigio (int sig)
{
    (void) signal (SIGIO, sigio);
    aborting = True;
    abortIo = True;
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

#ifdef sun
#define GetPgrp()   getpgrp(0)
#else
#define GetPgrp()   getpgrp()
#endif

Bool
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
	if (!ownTty[fd])
	{
	    ownTty[fd] = IoOwnTty (fd);
	    if (ownTty[fd])
		FileSetFd (fd);
	    else
		anyTtyUnowned = True;
	}
	else
	    anyTtyUnowned = True;
    }
    if (anyTtyUnowned)
	IoNoticeTtyUnowned ();
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
    if (anyFileWriteBlocked || anyTtyUnowned)
	return True;
    ioTimeoutQueued = False;
    return False;
}

void
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
IoInit (void)
{
    ENTER ();
    (void) signal (SIGIO, sigio);
    FileStdin = FileCreate (0);
    FileStdout = FileCreate (1);
    FileStderr = FileCreate (2);
    MemAddRoot (FileStdin);
    MemAddRoot (FileStdout);
    MemAddRoot (FileStderr);
    IoStart ();
    EXIT ();
}
