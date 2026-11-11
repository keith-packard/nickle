/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 *	util.c
 *
 *	general purpose utilities
 */

#include "nickle.h"

#ifdef notdef
double
       dist(x0, y0, x1, y1)
double x0, y0, x1, y1;
{
    register double tx, ty;

    tx = x0 - x1;
    ty = y0 - y1;
    return sqrt(tx * tx + ty * ty);
}
#endif

DataType TempType = { 0, 0, "TempType" };

void *
AllocateTemp(size_t size)
{
    DataType **b;

    b = ALLOCATE(&TempType, sizeof(DataType *) + size);
    return b + 1;
}

#include <stdarg.h>
#include <stdio.h>

#ifdef HAVE_VPRINTF

#include <sys/poll.h>
#include <errno.h>

/*
 * Currently vfprintf() is required.  It would
 * be easy to do a _doprnt() version if necessary,
 * and it would certainly be possible to develop
 * non-varargs versions of these.  Contributed code welcome.
 */

static ptrdiff_t
wait_write(int fd, const char *buf, ptrdiff_t len)
{
    ssize_t   n;
    ptrdiff_t w = 0;

    while (len > 0) {
        n = write(fd, buf, (size_t)len);
        if (n < 0) {
            if (errno == EINTR) {
                struct pollfd f;

                f.fd = fd;
                f.events = POLLOUT;

                (void)poll(&f, 1, -1);
            } else {
                if (w)
                    return w;
                return -1;
            }
        } else {
            w += n;
            buf += n;
            len -= n;
        }
    }
    return w;
}

void
debug(char *format, ...)
{
    va_list ap;
    char    buf[4096];
    int     len;

    va_start(ap, format);
    len = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    /* vsnprintf returns the untruncated length */
    if (len > (int)sizeof(buf) - 1)
        len = (int)sizeof(buf) - 1;
    wait_write(2, buf, len);
}

/*
 * Report that memory is exhausted, then exit. Flush pending output
 * first so that it precedes the message, and shut down I/O after, as
 * that closes stderr. Either may itself run out of memory and call
 * here again, so do each only once.
 */
void
MemExhausted(void)
{
    static Bool flushed, finished;
    static char message[] = "Out of memory\n";

    if (!flushed) {
        flushed = True;
        FileFlush(FileStdout, True);
        FileFlush(FileStderr, True);
    }
    wait_write(2, message, (ptrdiff_t)sizeof(message) - 1);
    if (!finished) {
        finished = True;
        IoFini();
        FileFini();
    }
    exit(1);
}

void
panic(char *format, ...)
{
    va_list ap;
    char    buf[4096];
    int     len;

    va_start(ap, format);
    len = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    /* vsnprintf returns the untruncated length */
    if (len > (int)sizeof(buf) - 1)
        len = (int)sizeof(buf) - 1;
    wait_write(2, buf, len);
    abort();
}

#endif
