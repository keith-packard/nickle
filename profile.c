/* $Header$ */

/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include	"nickle.h"
#include	<sys/time.h>
#include	<assert.h>

#define TICK_MS	10

volatile static unsigned long	currentTick;
volatile Bool			signalProfile;
static unsigned long		previousTick;
Bool				profiling;

static RETSIGTYPE
sigprofile (int sig)
{
    resetSignal (SIGVTALRM, sigprofile);
    currentTick += TICK_MS;
    SetSignalProfile ();
}

void
ProfileInterrupt (Value thread)
{
    unsigned long   now;
    unsigned long   ticks;
    InstPtr	    pc;
    ExprPtr	    stat;
    FramePtr	    frame;
    
    now = currentTick;
    ticks = now - previousTick;
    previousTick = now;
    if (!thread)
	return;
    pc = thread->thread.continuation.pc;
    if (pc)
    {
	stat = ObjStatement (thread->thread.continuation.obj,pc);
	if (stat)
	{
	    stat->base.ticks += ticks;
	}
    }
    for (frame = thread->thread.continuation.frame; frame; frame = frame->previous)
    {
	pc = frame->savePc;
        stat = ObjStatement (frame->saveObj, frame->savePc);
        if (stat)
	    stat->base.sub_ticks += ticks;
    }
}

Value
do_profile (Value on)
{
    struct itimerval    v;
    Bool    previous = profiling;
	
    if (True (on))
    {
	previousTick = currentTick;
	catchSignal (SIGVTALRM, sigprofile);
	v.it_interval.tv_sec = 0;
	v.it_interval.tv_usec = TICK_MS * 1000;
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
	/*
	 * expr nodes clear their accumulated time 
	 * on GC when profiling is false 
	 */
	MemCollect ();
    }
    return previous ? TrueVal : FalseVal;
}
