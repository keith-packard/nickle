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

#include	<setjmp.h>
#include	<signal.h>
#include	<stdio.h>
#include	"nickle.h"

int	stdin_interactive;

static void
setArgv (int argc, char **argv)
{
    ENTER ();
    Value   args;
    int	    i;

    args = NewArray (True, NewTypesPrim (type_string), 1, &argc);
    for (i = 0; i < argc; i++)
	BoxValueSet (args->array.values, i, NewStrString (argv[i]));
    setVar ("argv", args);
    EXIT ();
}

static void
try_nicklerc (void)
{
    char    nicklerc[2048];
    char    *home;

    home = getenv ("NICKLERC");
    if (home) {
	pushinput (nicklerc, True);
	return;
    }
    home = getenv ("HOME");
    if (!home)
	return;
    if (strlen(home) > 1024) {
	fprintf(stderr, "warning: $HOME too large, giving up on .nicklerc\n");
	return;
    }
    strcpy (nicklerc, home);
    strcat (nicklerc, "/.nicklerc");
    pushinput (nicklerc, False);
}

#ifndef NICKLELIB
#define NICKLELIB "/usr/local/share/nickle/builtin.5c"
#endif

static void
try_nicklelib (void)
{
    char    *lib;

    lib = getenv ("NICKLELIB");
    if (!lib)
	lib = NICKLELIB;
    pushinput (lib, True);
}

RETSIGTYPE	intr(int), ferr(int);
RETSIGTYPE	stop (int), die (int), segv (int);

/*ARGSUSED*/
int
main (int argc, char **argv)
{
    (void) catchSignal (SIGHUP, die);
    (void) catchSignal (SIGINT, intr);
    (void) catchSignal (SIGQUIT, die);
    (void) catchSignal (SIGILL, die);
    (void) catchSignal (SIGABRT, die);
    (void) catchSignal (SIGSEGV, segv);
    (void) catchSignal (SIGPIPE, SIG_IGN);
    (void) catchSignal (SIGTERM, die);
    (void) catchSignal (SIGTSTP, stop);
    (void) catchSignal (SIGTTIN, stop);
    (void) catchSignal (SIGTTOU, stop);
    stdin_interactive = isatty(0);
    init ();
    lexstdin ();
    try_nicklelib();
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
	try_nicklerc();
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
    LexInit ();
    NamespaceInit ();
    SymbolInit ();
    BuiltinInit ();
    ThreadInit ();
    HistoryInit ();
    TimerInit ();
}

void
catchSignal (int sig, RETSIGTYPE (*func) (int sig))
{
#ifdef HAS_SIGACTION
    struct sigaction sa;

    memset (&sa, '\0', sizeof (struct sigaction));
    sa.sa_handler = func;
    sa.sa_flags = SA_RESTART;
    sigaction (sig, &sa, 0);
#else
    signal (sig, func);
#endif
}

void
resetSignal (int sig, RETSIGTYPE (*func) (int sig))
{
#ifndef HAS_SIGACTION
    signal (sig, func);
#endif
}

volatile Bool	signalInterrupt;

RETSIGTYPE
intr (int sig)
{
    resetSignal (SIGINT, intr);
    SetSignalInterrupt ();
}

RETSIGTYPE
stop (int sig)
{
    sigset_t	set, oset;

    sigfillset (&set);
    sigprocmask (SIG_SETMASK, &set, &oset);
    IoStop ();
    catchSignal (sig, SIG_DFL);
    sigfillset (&set);
    sigdelset (&set, sig);
    sigprocmask (SIG_SETMASK, &set, &set);
    kill (getpid(), sig);
    sigprocmask (SIG_SETMASK, &oset, &set);
    IoStart ();
    catchSignal (sig, stop);
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
    catchSignal (SIGSEGV, SIG_DFL);
    /* return and reexecute the fatal instruction */
}
