/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * edit.c
 *
 * invoke the users editor (default /bin/ed) 
 */

#define _GNU_SOURCE
#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	"nickle.h"
    
#ifndef DEFAULT_EDITOR
#define DEFAULT_EDITOR	"ed"
#endif

static int
edit (char *file_name)
{
    char	*buf;
    char	*editor;
    int		ret;

    if (!(editor = getenv ("EDITOR")))
	    editor = DEFAULT_EDITOR;
    if (!file_name)
	file_name = "";
    ret = asprintf (&buf, "%s %s", editor, file_name);
    if (ret >= 0) {
	IoStop ();
	ret = system (buf);
	IoStart ();
	free(buf);
    }
    return ret;
}

void
EditFunction (SymbolPtr symbol, Publish publish)
{
    Value	tmp;
    static const char	template[] = "/tmp/nXXXXXX";
    static const char	exten[] = ".5c";
    char	tmpName[sizeof (template)];
    char	nickleName[sizeof (template) + sizeof (exten) + 2];
    int		fd;
    
    (void) strcpy (tmpName, template);
    fd = mkstemp (tmpName);
    if (fd < 0)
	return;
    strcpy (nickleName, tmpName);
    strcat (nickleName, exten);
    if (rename (tmpName, nickleName) < 0)
    {
	close (fd);
	unlink (tmpName);
	return;
    }
    tmp = FileCreate (fd, FileWritable);
    if (tmp)
    {
	PrettyPrint (tmp, publish, symbol);
	(void) FileClose (tmp);
	if (edit (nickleName) == 0)
		LexFile (nickleName, True, False);
    }
    (void) unlink (nickleName);
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
	char	*name = StrzPart (file_name, "invalid filename");

	if (name)
	    edit (name);
    }
}
