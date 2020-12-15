/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"
#include	"gram.h"

Bool	signalError;

void
PrintError (char *s, ...)
{
    va_list args;

    va_start (args, s);
    FileVPrintf (FileStderr, s, args);
    va_end (args);
}
