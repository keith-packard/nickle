/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	main.c
 *
 *	main routine for nick
 */

#include	<config.h>

#include	<setjmp.h>
#include	<signal.h>
#include	<stdio.h>
#include	"nickle.h"

int	stdin_interactive;
int	interactive;

void
setArgv (int argc, char **argv)
{
    ENTER ();
    Value   args;
    int	    i;

    argc = argc + 1;
    args = NewArray (False, NewTypesPrim (type_string), 1, &argc);
    for (i = 0; i < argc - 1; i++)
	BoxValue (args->array.values, i) = NewStrString (argv[i]);
    BoxValue (args->array.values, i) = Zero;
    setVar ("argv", args);
    EXIT ();
}

void
try_nickrc (void)
{
    char    nickrc[1024];
    char    *home;

    home = getenv ("HOME");
    if (!home)
	return;
    strcpy (nickrc, home);
    strcat (nickrc, "/.nickrc");
    pushinput (nickrc, False);
}

RETSIGTYPE	intr(int), ferr(int);
RETSIGTYPE	stop (int), die (int), segv (int);

/*ARGSUSED*/
int
main (int argc, char **argv)
{
    ignore_ferr ();
    (void) signal (SIGHUP, die);
    (void) signal (SIGINT, intr);
    (void) signal (SIGQUIT, die);
    (void) signal (SIGILL, die);
    (void) signal (SIGABRT, die);
    (void) signal (SIGSEGV, segv);
    (void) signal (SIGPIPE, SIG_IGN);
    (void) signal (SIGTERM, die);
    (void) signal (SIGTSTP, stop);
    (void) signal (SIGTTIN, stop);
    (void) signal (SIGTTOU, stop);
    interactive = stdin_interactive = isatty(0);
    init ();
    yyinput = FileStdin;
    if (argc > 1)
    {
	setArgv (argc - 1, argv + 1);
	if (!lexfile (argv[1]))
	{
	    IoFini ();
	    return 1;
	}
    }
    else
	try_nickrc();
    (void) yyparse ();
    IoFini ();
    return 0;
}

void
init (void)
{
    MemInitialize ();
    TypesInit ();
    ValueInit ();
    IoInit ();
    ScopeInit ();
    SymbolInit ();
    BuiltinInit ();
    ThreadInit ();
    HistoryInit ();
    TimerInit ();
}

volatile Bool	abortInterrupt;
volatile Bool	abortException;

RETSIGTYPE
intr (int sig)
{
    void	intr(int);

    (void) signal (SIGINT, intr);
    aborting = True;
    exception = True;
    abortInterrupt = True;
}

RETSIGTYPE
ferr(int sig)
{
    aborting = True;
    exception = True;
    abortException = True;
}

RETSIGTYPE
stop (int sig)
{
    sigset_t	set, oset;

    sigfillset (&set);
    sigprocmask (SIG_SETMASK, &set, &oset);
    IoStop ();
    (void) signal (sig, SIG_DFL);
    sigfillset (&set);
    sigdelset (&set, sig);
    sigprocmask (SIG_SETMASK, &set, &set);
    kill (getpid(), sig);
    sigprocmask (SIG_SETMASK, &oset, &set);
    IoStart ();
    (void) signal (sig, stop);
}

RETSIGTYPE
die (int sig)
{
    IoStop ();
    _exit (sig);
}

RETSIGTYPE
segv (int sig)
{
    IoStop ();
    (void) signal (SIGSEGV, SIG_DFL);
    /* return and reexecute the fatal instruction */
}
