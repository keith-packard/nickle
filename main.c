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
#define __USE_UNIX98 /* Get sigignore() and sigrelse()
			prototypes for Linux */
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
    LexFile (NICKLELIB "/builtin.5c", True, False);
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
    Bool    tryrc = True;
    Bool    stdin_prog = True;
    
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
    /*
     * Get the main input stream set up
     */
    while (argc > 1) {
	if (argc > 2 && !strcmp (argv[1], "-e"))
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
	    LexString (StringChars (&s->string), True);
	    EXIT ();
	    stdin_prog = False;
	    argc -= (i - 1);	/* argc == 1 */
	    argv += (i - 1);	/* argv[1] == 0 */
	    break;
	}
	if (argc > 2 && !strcmp (argv[1], "-f"))
	{
	    if (!LexFile (argv[2], True, True))
	    {
		IoFini ();
		return 1;
	    }
	    argv += 2;
	    argc -= 2;
	    continue;
	}
	if (argc > 2 && !strcmp (argv[1], "-l"))
	{
	    if (!LexLibrary (argv[2], True, True))
	    {
		IoFini ();
		return 1;
	    }
	    argv += 2;
	    argc -= 2;
	    continue;
	}
	if (argc <= 0)
	    break;
	tryrc = False;
	stdin_prog = False;
	if (!LexFile (argv[1], True, True))
	{
	    IoFini ();
	    return 1;
	}
	break;
    }
    setArgv (argc - 1, argv + 1);
    if (stdin_prog)
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
