/* $Header$ */

/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	main.c
 *
 *	main routine for nick
 */

#include	"nickle.h"
#include	"gram.h"

#include	<setjmp.h>
#define __USE_UNIX98 /* Get sigignore() and sigrelse()
			prototype for Linux */
#include	<signal.h>
#include	<stdio.h>

#if HAVE_SYS_RESOURCE_H
#include	<sys/resource.h>
#endif

int	stdin_interactive;

static void
setArgv (int argc, char **argv)
{
    ENTER ();
    Value   args;
    int	    i;

    args = NewArray (True, True, typePrim[rep_string], 1, &argc);
    for (i = 0; i < argc; i++)
	ArrayValueSet (&args->array, i, NewStrString (argv[i]));
    setVar (GlobalNamespace, "argv", args, 
	    NewTypeArray (typePrim[rep_string],
			   NewExprTree (COMMA, 0, 0), False));
    EXIT ();
}

static Bool
try_nicklestart (void)
{
    char    *nicklestart;

    if ((nicklestart = getenv ("NICKLESTART")) == 0)
	nicklestart = NICKLELIBDIR "/builtin.5c";
    return LexFile (nicklestart, True, False);
}    

RETSIGTYPE	intr(int), ferr(int);
RETSIGTYPE	stop (int), die (int), segv (int);

static void
ignoreSignal(int sig) {
    catchSignal (sig, SIG_IGN);
}

static void
releaseSignal(int sig) {
    catchSignal (sig, SIG_DFL);
}

/*ARGSUSED*/
int
main (int argc, char **argv)
{
#if HAVE_GETRLIMIT && HAVE_SETRLIMIT
    /*
     * Allow stack to grow as large as possible to avoid
     * crashes during recursive datastructure marking in the
     * garbage collector
     */
    struct rlimit   lim;

    if (getrlimit (RLIMIT_STACK, &lim) == 0)
    {
	lim.rlim_cur = lim.rlim_max;
	(void) setrlimit (RLIMIT_STACK, &lim);
    }
#endif
    (void) catchSignal (SIGHUP, die);
    (void) catchSignal (SIGINT, intr);
    (void) catchSignal (SIGQUIT, die);
    (void) catchSignal (SIGILL, die);
    (void) catchSignal (SIGABRT, die);
    (void) catchSignal (SIGSEGV, segv);
    (void) ignoreSignal (SIGPIPE);
    (void) catchSignal (SIGTERM, die);
    (void) catchSignal (SIGTSTP, stop);
    (void) catchSignal (SIGTTIN, stop);
    (void) catchSignal (SIGTTOU, stop);
    stdin_interactive = isatty(0);
    init ();
    setArgv (argc - 1, argv + 1);
    if (!try_nicklestart()) {
	fprintf(stderr, "nickle: NICKLESTART environment var points at bad code\n");
	exit(1);
    }
    (void) yyparse ();
    IoFini ();
    FileFini ();
    return lastThreadError;
}

void
init (void)
{
    MemInitialize ();
    TypeInit ();
    ValueInit ();
    IoInit ();
    LexInit ();
    NamespaceInit ();
    SymbolInit ();
    BuiltinInit ();
    ThreadInit ();
    TimerInit ();
}

void
catchSignal (int sig, RETSIGTYPE (*func) (int sig))
{
#ifdef HAVE_SIGACTION
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
#ifndef HAVE_SIGACTION
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
    releaseSignal (sig);
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
    releaseSignal (SIGSEGV);
    /* return and reexecute the fatal instruction */
}
