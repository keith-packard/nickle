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
#include	"gram.h"

int	stdin_interactive;

static void
setArgv (int argc, char **argv)
{
    ENTER ();
    Value   args;
    int	    i;

    args = NewArray (True, typesPrim[type_string], 1, &argc);
    for (i = 0; i < argc; i++)
	BoxValueSet (args->array.values, i, NewStrString (argv[i]));
    setVar (GlobalNamespace, "argv", args, 
	    NewTypesArray (typesPrim[type_string],
			   NewExprTree (COMMA, 0, 0)));
    EXIT ();
}

static void
try_nicklerc (void)
{
    ENTER ();
    char	*home;
    char	*nicklerc;
    static char filename[] = "/.nicklerc";

    if ((nicklerc = getenv ("NICKLERC")))
    {
	LexFile (nicklerc, True, False);
    }
    else if ((home = getenv ("HOME")))
    {
	nicklerc = AllocateTemp (strlen (home) + strlen (filename) + 1);
	strcpy (nicklerc, home);
	strcat (nicklerc, filename);
	LexFile (nicklerc, False, False);
    }
    EXIT ();
}

static void
try_nicklelib (void)
{
    LexLibrary ("builtin.5c", True, False);
}    

RETSIGTYPE	intr(int), ferr(int);
RETSIGTYPE	stop (int), die (int), segv (int);

/*ARGSUSED*/
int
main (int argc, char **argv)
{
    Bool    tryrc = True;
    
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
    /*
     * Get the main input stream set up
     */
    if (argc > 1)
    {
	if (!strcmp (argv[1], "-e"))
	{
	    Value   s;
	    int	    i;

	    ENTER ();
	    s = NewString (0);
	    for (i = 2; i < argc; i++)
	    {
		s = Plus (s, NewStrString (argv[i]));
		if (i != argc-1)
		    s = Plus (s, NewStrString (" "));
		else
		    s = Plus (s, NewStrString ("\n"));
	    }
	    LexString (StringChars (&s->string));
	    EXIT ();
	}
	else
	{
	    tryrc = False;
	    setArgv (argc - 1, argv + 1);
	    if (!LexFile (argv[1], True, False))
	    {
		IoFini ();
		return 1;
	    }
	}
    }
    else
    {
	LexStdin ();
    }
    /*
     * Push initialization files
     */
    if (tryrc)
	try_nicklerc();
    try_nicklelib();
    /*
     * Run the parser
     */
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
