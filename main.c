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

#if HAVE_SYS_TIME_H
#include	<sys/time.h>
#endif

#if HAVE_SYS_RESOURCE_H
#include	<sys/resource.h>
#endif

#if HAVE_LIBREADLINE
#include <readline/readline.h>
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

void	intr(int), ferr(int);
void	stop (int), die (int), segv (int);

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
    (void) ignoreSignal (SIGTTIN);
    (void) ignoreSignal (SIGTTOU);
    stdin_interactive = isatty(0);
    init ();
    setArgv (argc - 1, argv + 1);
    if (!try_nicklestart()) {
	fprintf(stderr, "nickle: NICKLESTART environment var points at bad code\n");
	exit(1);
    }
    (void) yyparse ();
    /* Wait for any running threads to execute */
    ThreadsRun (0, 0);
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
catchSignal (int sig, void (*func) (int sig))
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
resetSignal (int sig, void (*func) (int sig))
{
#ifndef HAVE_SIGACTION
    signal (sig, func);
#endif
}

volatile Bool	signalInterrupt;

void
intr (int sig)
{
    resetSignal (SIGINT, intr);
    if (signalInterrupt) {
	int ret = write(2,"Double interrupt, exiting\n", 26);
	(void) ret;
#if HAVE_RL_CLEANUP_AFTER_SIGNAL
	if (stdin_in_readline)
	    rl_cleanup_after_signal();
#endif
	exit(1);
    }
    SetSignalInterrupt ();
}

void
stop (int sig)
{
    sigset_t	set, oset;

#if HAVE_RL_CLEANUP_AFTER_SIGNAL
    if (stdin_in_readline) {
	rl_echo_signal_char(sig);
	rl_cleanup_after_signal();
    }
#endif

    IoStop ();
    releaseSignal (sig);
    killpg (0, sig);
    sigemptyset(&set);
    sigaddset(&set, sig);
    sigprocmask (SIG_UNBLOCK, &set, &oset);

    /* stopped ... */

    sigprocmask (SIG_SETMASK, &oset, NULL);
    catchSignal (sig, stop);
    IoStart ();

#if HAVE_RL_CLEANUP_AFTER_SIGNAL
    if (stdin_in_readline)
	rl_reset_after_signal();
#endif
}

void
die (int sig)
{
#if HAVE_RL_CLEANUP_AFTER_SIGNAL
    if (stdin_in_readline) {
	rl_free_line_state();
	rl_cleanup_after_signal();
    }
#endif
    IoStop ();
    _exit (sig);
}

void
segv (int sig)
{
    IoStop ();
#if HAVE_RL_CLEANUP_AFTER_SIGNAL
    if (stdin_in_readline)
	rl_cleanup_after_signal();
#endif
    releaseSignal (SIGSEGV);
    /* return and reexecute the fatal instruction */
}
