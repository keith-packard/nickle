/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"
#include	<sys/time.h>

volatile static unsigned long	currentTick;
volatile Bool			signalProfile;
static unsigned long		previousTick;
Bool				profiling;

static RETSIGTYPE
sigprofile (int sig)
{
    resetSignal (SIGVTALRM, sigprofile);
    currentTick++;
    SetSignalProfile ();
}

void
ProfileInterrupt (Value thread)
{
    unsigned long   ticks = currentTick - previousTick;
    InstPtr	    pc;
    ExprPtr	    stat;
    FramePtr	    frame;
    
    previousTick = 0;
    if (!thread)
	return;
    pc = thread->thread.continuation.pc;
    if (pc)
    {
	stat = pc->base.stat;
	if (stat)
	{
	    stat->base.ticks += ticks;
	    stat->base.sub_ticks += ticks;
	}
    }
    for (frame = thread->thread.continuation.frame; frame; frame = frame->previous)
    {
	pc = frame->savePc;
	if (pc)
	{
	    stat = pc->base.stat;
	    if (stat)
		stat->base.sub_ticks += ticks;
	}
    }
}

Value
do_profile (Value on)
{
    struct itimerval    v;
    Bool    previous = profiling;
	
    if (!True (on))
    {
	currentTick = previousTick = 0;
	catchSignal (SIGVTALRM, sigprofile);
	v.it_interval.tv_sec = 0;
	v.it_interval.tv_usec = 10000;
	v.it_value = v.it_interval;
	setitimer (ITIMER_VIRTUAL, &v, 0);
	profiling = True;
    }
    else
    {
	v.it_interval.tv_sec = 0;
	v.it_interval.tv_usec = 0;
	v.it_value = v.it_interval;
	setitimer (ITIMER_VIRTUAL, &v, 0);
	profiling = False;
    }
    return previous ? TrueVal : FalseVal;
}
