/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * edit.c
 *
 * invoke the users editor (default /bin/ed) 
 */

#include	<config.h>

#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	"nickle.h"
    
#ifndef DEFAULT_EDITOR
#define DEFAULT_EDITOR	"ed"
#endif

static void
edit (char *file_name)
{
    char	buf[1024];
    char	*editor;

    if (!(editor = getenv ("EDITOR")))
	    editor = DEFAULT_EDITOR;
    if (!file_name)
	file_name = "";
    (void) sprintf (buf, "%s %s", editor, file_name);
    IoStop ();
    (void) system (buf);
    IoStart ();
}

void
EditFunction (SymbolPtr name)
{
    Value	tmp;
    static char	template[] = "/tmp/nXXXXXX.nick";
    char	tmpName[sizeof (template)];
    
    (void) strcpy (tmpName, template);
    (void) mktemp (tmpName);
    tmp = FileFopen (tmpName, "w");
    if (tmp)
    {
	PrettyPrint (tmp, name);
	(void) FileClose (tmp);
	edit (tmpName);
	pushinput (tmpName, True);
    }
    (void) unlink (tmpName);
}

void
EditFile (Value file_name)
{
    if (!file_name)
    {
	edit (0);
    }
    else
    {
	switch (file_name->value.tag) {
	default:
	    RaiseError (0, "edit: non-string filename");
	    break;
	case type_string:
	    edit (StringChars (&file_name->string));
	    break;
	}
    }
}
