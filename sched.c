/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#include <assert.h>
#include "nickle.h"
#include "ref.h"

Value       running;
Value       stopped;
Bool        signalException;

extern void dumpSleep(void), dumpThreads(void);

static void
_ThreadInsert(Value thread)
{
    Value *prev, t;

    switch (thread->thread.state) {
    case ThreadRunning:
        prev = &running;
        break;
    case ThreadSuspended:
        prev = &stopped;
        break;
    case ThreadFinished:
    default:
        return;
    }
    for (; (t = *prev); prev = &t->thread.next)
        if (t->thread.priority < thread->thread.priority)
            break;
    thread->thread.next = t;
    *prev = thread;
}

static void
_ThreadRemove(Value thread)
{
    Value *prev;

    switch (thread->thread.state) {
    case ThreadRunning:
        prev = &running;
        break;
    case ThreadSuspended:
        prev = &stopped;
        break;
    case ThreadFinished:
    default:
        return;
    }
    for (; *prev != thread; prev = &(*prev)->thread.next)
        ;
    *prev = thread->thread.next;
}

void
ThreadSetState(Value thread, ThreadState state)
{
    if (state != thread->thread.state) {
        _ThreadRemove(thread);
        thread->thread.state = state;
        _ThreadInsert(thread);
    }
}

void
ThreadSleep(Value thread, void *sleep)
{
    thread->thread.sleep = sleep;
    SetSignalSuspend();
}

void
ThreadStepped(Value thread)
{
    Value t;

    if ((t = thread->thread.next) && thread->thread.priority <= t->thread.priority) {
        _ThreadRemove(thread);
        _ThreadInsert(thread);
    }
}

void
ThreadsWakeup(void *sleep, WakeKind kind)
{
    Value thread, next;

    for (thread = stopped; thread; thread = next) {
        next = thread->thread.next;
        if ((thread->thread.state == ThreadSuspended) && thread->thread.sleep == sleep) {
            /* sleep stays set until the waiting instruction completes, for ThreadAbandonSleep */
            ThreadSetState(thread, ThreadRunning);
            if (kind == WakeOne)
                break;
        }
    }
}

Bool lastThreadError;

/*
 * 'thread' stops sleeping before the instruction that put it to sleep
 * has completed, because it was killed or interrupted by a signal.
 * Undo whatever that instruction did before sleeping, and clear sleep
 * so nothing undoes it again. Semaphore::wait decremented the count: a
 * thread still asleep gives up its place, while one already woken by
 * Semaphore::signal passes that signal to the next sleeping thread.
 * sleep queued a timer: cancel it, so it can't wake the thread from a
 * later sleep
 */
static void
ThreadAbandonSleep(Value thread)
{
    void *sleep = thread->thread.sleep;

    if (!sleep)
        return;
    thread->thread.sleep = 0;
    if (MemType(sleep) == &SemaphoreRep.data) {
        Value s = sleep;

        ++s->semaphore.count;
        if (thread->thread.state != ThreadSuspended && s->semaphore.count <= 0)
            ThreadsWakeup(s, WakeOne);
    } else if (MemType(sleep) == &SleepQType)
        TimerCancel(sleep);
}

void
ThreadFinish(Value thread, Bool error)
{
    if (thread->thread.state != ThreadFinished) {
        ThreadAbandonSleep(thread);
        ThreadSetState(thread, ThreadFinished);
        ThreadsWakeup(thread, WakeAll);
        lastThreadError = error;
    }
}

Value
do_Thread_join(Value target)
{
    ENTER();
    if (!ValueIsThread(target)) {
        RaiseStandardException(exception_invalid_argument, 3,
                               NewStrString("join needs thread argument"), NewInt(0), target);
        RETURN(Void);
    }
    if (target == running) {
        RaiseStandardException(exception_invalid_argument, 3,
                               NewStrString("join: a thread can't wait for itself"), NewInt(0),
                               target);
        RETURN(Void);
    }
    if (target->thread.state != ThreadFinished) {
        ThreadSleep(running, target);
        RETURN(Void);
    }
    RETURN(target->thread.continuation.value);
}

static void
ThreadListState(Value thread)
{
    switch (thread->thread.state) {
    case ThreadRunning:
        FilePuts(FileStdout, " running");
        break;
    case ThreadSuspended:
        FilePuts(FileStdout, " suspended");
        break;
    case ThreadFinished:
        FilePuts(FileStdout, " finished");
        break;
    }
}

Value
do_Thread_list(void)
{
    ENTER();
    Value t;

    for (t = running; t; t = t->thread.next) {
        FilePrintf(FileStdout, "\t%%%z", t->thread.id);
        ThreadListState(t);
        FileOutput(FileStdout, '\n');
    }
    for (t = stopped; t; t = t->thread.next) {
        FilePrintf(FileStdout, "\t%%%z", t->thread.id);
        ThreadListState(t);
        if (t->thread.sleep) {
            if (MemType(t->thread.sleep) == &SleepQType)
                FilePuts(FileStdout, " sleep");
            else {
                Value sleep = t->thread.sleep;

                FilePrintf(FileStdout, " %g", sleep);
            }
        }
        FileOutput(FileStdout, '\n');
    }
    RETURN(Void);
}

Value
do_Thread_id_to_thread(Value id)
{
    ENTER();
    ptrdiff_t i;
    Value     t;

    if (!ValueToPtrdiff(id, &i))
        RETURN(Void);
    for (t = running; t; t = t->thread.next)
        if (t->thread.id == i)
            RETURN(t);
    for (t = stopped; t; t = t->thread.next)
        if (t->thread.id == i)
            RETURN(t);
    RETURN(Void);
}

Value
do_Thread_current(void)
{
    ENTER();
    Value ret;
    if (running)
        ret = running;
    else
        ret = Void;
    RETURN(ret);
}

Value
do_Thread_set_priority(Value thread, Value priority)
{
    ENTER();
    int i;
    if (!ValueIsThread(thread)) {
        RaiseStandardException(exception_invalid_argument, 3,
                               NewStrString("set_priority: not a thread"), NewInt(0), thread);
        RETURN(Void);
    }
    i = IntPart(priority, "Invalid thread priority");
    if (aborting)
        RETURN(Void);
    if (i != thread->thread.priority) {
        _ThreadRemove(thread);
        thread->thread.priority = i;
        _ThreadInsert(thread);
    }
    RETURN(NewInt(thread->thread.priority));
}

Value
do_Thread_get_priority(Value thread)
{
    ENTER();
    if (!ValueIsThread(thread)) {
        RaiseStandardException(exception_invalid_argument, 3,
                               NewStrString("get_priority: not a thread"), NewInt(0), thread);
        RETURN(Void);
    }
    RETURN(NewInt(thread->thread.priority));
}

static SymbolPtr         standardExceptions[_num_standard_exceptions];
static StandardException standardException;
static Value             standardExceptionArgs;

/*
 * Raise 'se' in 'thread'. In the running thread, while a builtin
 * executes, it's raised when the instruction ends. Any other thread
 * stops waiting and jumps to the handler, running when next scheduled
 */
static void
ThreadRaise(Value thread, StandardException se, Value args, Bool executing)
{
    ENTER();
    SymbolPtr except = standardExceptions[se];

    if (thread == running && executing) {
        standardException = se;
        standardExceptionArgs = args;
        SetSignalException();
    } else if (except) {
        InstPtr next;

        ThreadAbandonSleep(thread);
        thread->thread.partial = 0;
        RaiseException(thread, except, args, &next);
        thread->thread.continuation.value = args;
        thread->thread.continuation.pc = next;
        if (thread->thread.state == ThreadSuspended)
            ThreadSetState(thread, ThreadRunning);
    }
    EXIT();
}

/* Kill 'thread', argument 'i' of Thread::kill */
static int
KillThread(Value thread, ptrdiff_t i)
{
    if (!ValueIsThread(thread)) {
        RaiseStandardException(exception_invalid_argument, 3, NewStrString("kill: not a thread"),
                               NewPtrdiffInteger(i), thread);
        return 0;
    }
    if (thread->thread.state == ThreadFinished)
        return 0;
    if (thread->thread.killed) {
        /*
         * Killed again while unwinding: finish without waiting for the
         * rest, but count the thread only once
         */
        thread->thread.continuation.value = Void;
        if (thread == running)
            SetSignalFinished();
        else
            ThreadFinish(thread, False);
        return 0;
    }
    /* Unwind it through its twixt leave expressions; see RaiseException */
    thread->thread.killed = True;
    ThreadRaise(thread, exception_thread_killed, Void, True);
    return 1;
}

Value
do_Thread_kill(ptrdiff_t n, Value *p)
{
    ENTER();
    Value     thread;
    ptrdiff_t ret = 0;
    ptrdiff_t i;

    /* With no threads, kill the current one */
    if (n == 0) {
        thread = running;
        if (thread)
            ret = KillThread(thread, 0);
    } else {
        for (i = 0; i < n; i++)
            ret += KillThread(p[i], i);
    }
    RETURN(NewPtrdiffInteger(ret));
}

void
TraceFunction(Value file, FramePtr frame, CodePtr code, ExprPtr name)
{
    ptrdiff_t fe;

    FilePuts(file, "    ");
    if (name)
        PrettyExpr(file, name, -1, 0, False);
    else
        FilePuts(file, "<anonymous>");
    FilePuts(file, " (");
    for (fe = 0; fe < code->base.argc; fe++) {
        if (fe)
            FilePuts(file, ", ");
        FilePrintf(file, "%G", BoxValue(frame->frame, fe));
    }
    FilePuts(file, ")\n");
}

static void
TraceStatement(Value file, ExprPtr stat)
{
    FilePrintf(file, "%A:%d: ", stat->base.file, stat->base.line);
    PrettyStat(file, stat, False);
}

void
TraceFrame(Value file, FramePtr frame, ObjPtr obj, InstPtr pc, int depth)
{
    ENTER();
    int     max;
    CodePtr code;

    if (obj && pc)
        TraceStatement(file, ObjStatement(obj, pc));
    for (max = depth; frame && max--; frame = frame->previous) {
        code = frame->function->func.code;
        TraceFunction(file, frame, code, code->base.name);
        TraceStatement(file, ObjStatement(frame->saveObj, frame->savePc));
    }
    EXIT();
}

#ifdef DEBUG_JUMP
static void
TraceIndent(int indent)
{
    while (indent--)
        FilePuts(FileStdout, "    ");
}
#endif

Value
do_Thread_trace(ptrdiff_t n, Value *p)
{
    ENTER();
    Value    v;
    FramePtr frame;
    InstPtr  pc;
    ObjPtr   obj;
    int      depth = 20;

    if (n == 0)
        v = lookupVar(0, "cont");
    else
        v = p[0];
    if (n > 1) {
        depth = IntPart(p[1], "Invalid trace depth");
        if (aborting)
            RETURN(Void);
    }
    switch (ValueTag(v)) {
    case rep_thread:
    case rep_continuation:
        frame = v->continuation.frame;
        pc = v->continuation.pc;
        obj = v->continuation.obj;
        break;
    default:
        if (n == 0)
            RaiseStandardException(exception_invalid_argument, 3,
                                   NewStrString("trace: no default continuation"), NewInt(0), Void);
        else
            RaiseStandardException(exception_invalid_argument, 3,
                                   NewStrString("Thread::trace: neither continuation nor thread"),
                                   NewInt(0), v);
        RETURN(Void);
    }
    TraceFrame(FileStdout, frame, obj, pc, depth);
    RETURN(Void);
}

static void
ThreadMark(void *object)
{
    ThreadPtr thread = object;

    ContinuationMark(&thread->continuation);
    MemReference(thread->jump);
    MemReference(thread->sleep);
    MemReference(thread->next);
}

static Bool
ThreadPrint(Value f, Value av, uint32_t format, int base, int width, int prec, uint32_t fill)
{
    (void)format;
    (void)base;
    (void)width;
    (void)prec;
    (void)fill;
    FilePrintf(f, "%%%z", av->thread.id);
    return True;
}

ValueRep ThreadRep = {
    { ThreadMark, 0, "ThreadRep" }, /* base */
    rep_thread, /* tag */
    {
     /* binary */
        0, 0,
     0, 0,
     0, 0,
     0, ValueEqual,
     0, 0,
     },
    {
     /* unary */
        0, 0,
     0, },
    0,
    0,
    ThreadPrint,
    0,
    0,
};

static ptrdiff_t ThreadId;

Value
NewThread(FramePtr frame, ObjPtr code)
{
    ENTER();
    Value ret;

    ret = ALLOCATE(&ThreadRep.data, sizeof(Thread));

    ret->thread.jump = 0;
    ret->thread.state = ThreadRunning;
    ret->thread.priority = 0;
    ret->thread.sleep = 0;
    ret->thread.id = ++ThreadId;
    ret->thread.partial = 0;
    ret->thread.killed = False;
    ret->thread.next = 0;

    ContinuationInit(&ret->thread.continuation);
    ret->thread.continuation.obj = code;
    ret->thread.continuation.pc = ObjCode(code, 0);
    ret->thread.continuation.frame = frame;

    complete = True;
    if (code->error) {
        LexErrorExit();
        ret->thread.state = ThreadFinished;
    }
    _ThreadInsert(ret);
    RETURN(ret);
}

typedef struct _blockHandler {
    DataType             *data;
    struct _blockHandler *next;
    NickleBlockHandler    handler;
    void                 *closure;
} BlockHandler;

static void
BlockHandlerMark(void *object)
{
    BlockHandler *bh = object;

    MemReference(bh->next);
}

static DataType      BlockHandlerType = { BlockHandlerMark, 0, "BlockHandlerType" };

static BlockHandler *blockHandlers;

void
ThreadsRegisterBlockHandler(NickleBlockHandler handler, void *closure)
{
    ENTER();
    BlockHandler *bh = ALLOCATE(&BlockHandlerType, sizeof(BlockHandler));
    bh->next = blockHandlers;
    blockHandlers = bh;
    bh->handler = handler;
    bh->closure = closure;
    EXIT();
}

void
ThreadsUnregisterBlockHandler(NickleBlockHandler handler, void *closure)
{
    ENTER();
    BlockHandler **prev, *bh;

    for (prev = &blockHandlers; (bh = *prev); prev = &bh->next) {
        if (bh->handler == handler && bh->closure == closure) {
            bh->handler = 0;
            *prev = bh->next;
        }
    }
    EXIT();
}

void
ThreadsBlock(void)
{
    BlockHandler *bh, *next;

    for (bh = blockHandlers; bh; bh = next) {
        next = bh->next;
        if (bh->handler)
            (*bh->handler)(bh->closure);
    }

    /* Pend in either select or sigsuspend, depending
     * on whether there are files blocked
     */
    if (!running)
        FileCheckBlocked(True);
}

static ReferencePtr RunningReference, StoppedReference;
static ReferencePtr BlockHandlerReference;

void
ThreadInit(void)
{
    ENTER();
    RunningReference = NewReference((void **)&running);
    MemAddRoot(RunningReference);
    StoppedReference = NewReference((void **)&stopped);
    MemAddRoot(StoppedReference);
    BlockHandlerReference = NewReference((void **)&blockHandlers);
    MemAddRoot(BlockHandlerReference);
    /*
     * Thread::kill raises this in the killed thread. It is in no
     * namespace, so no catch can name it, and RaiseException unwinds
     * the thread quietly instead of reporting it
     */
    RegisterStandardException(
        exception_thread_killed,
        NewSymbolException(AtomId("thread_killed"), NewTypeFunc(typePoly, 0), Void));
    EXIT();
}

static DataType FarJumpType = { 0, 0, "FarJumpType" };

FarJumpPtr
NewFarJump(ptrdiff_t inst, ptrdiff_t twixt, ptrdiff_t catch, ptrdiff_t frame)
{
    ENTER();
    FarJumpPtr farJump;

    farJump = ALLOCATE(&FarJumpType, sizeof(FarJump));
    farJump->inst = inst;
    farJump->twixt = twixt;
    farJump->catch = catch;
    farJump->frame = frame;
    RETURN(farJump);
}

Value
FarJumpContinuation(ContinuationPtr continuation, FarJumpPtr farJump)
{
    ENTER();
    Value     ret;
    CatchPtr  catch;
    TwixtPtr  twixt;
    FramePtr  frame;
    InstPtr   pc;
    ObjPtr    obj;
    ptrdiff_t twixts;
    ptrdiff_t catches;
    ptrdiff_t frames;

    ret = NewContinuation(continuation, 0);

    /*
     * Unwind twixts
     */
    twixts = farJump->twixt;
    twixt = ret->continuation.twixts;
    while (twixts--)
        twixt = twixt->continuation.twixts;
    ret->continuation.twixts = twixt;

    /*
     * Unwind catches
     */
#ifdef DEBUG_JUMP
    FilePrintf(FileStdout, "FarJump catches before: ");
    ThreadCatches(running);
#endif
    catches = farJump->catch;
    catch = ret->continuation.catches;
    while (catches--)
        catch = catch->continuation.catches;
    ret->continuation.catches = catch;
#ifdef DEBUG_JUMP
    FilePrintf(FileStdout, "FarJump catches after: ");
    ThreadCatches(running);
#endif

    /*
     * Unwind frames
     */
    frames = farJump->frame;
    frame = ret->continuation.frame;
    obj = continuation->obj;
    pc = continuation->pc;
    if (farJump->inst < 0)
        frames++;
    while (frames--) {
        pc = frame->savePc;
        obj = frame->saveObj;
        frame = frame->previous;
    }
    ret->continuation.frame = frame;
    /*
     * Set pc for non-return jumps
     */
    if (farJump->inst >= 0)
        pc = ObjCode(obj, farJump->inst);

    /*
     * Assertion here -- the stack is OK because
     * only intervening catch frames are on the stack
     * and they never have extra values on the stack
     */

    ret->continuation.pc = pc;
    ret->continuation.obj = obj;
    RETURN(ret);
}

void
ContinuationMark(void *object)
{
    ContinuationPtr continuation = object;

    assert(!continuation->pc
           || (ObjCode(continuation->obj, 0) <= continuation->pc
               && continuation->pc <= ObjCode(continuation->obj, ObjLast(continuation->obj))));
    MemReference(continuation->obj);
    MemReference(continuation->frame);
    MemReference(continuation->stack);
    MemReference(continuation->value);
    MemReference(continuation->catches);
    MemReference(continuation->twixts);
}

static Bool
ContinuationPrint(Value f, Value av, uint32_t format, int base, int width, int prec, uint32_t fill)
{
    (void)av;
    (void)format;
    (void)base;
    (void)width;
    (void)prec;
    (void)fill;
    FilePuts(f, "continuation");
    return True;
}

ValueRep ContinuationRep = {
    { ContinuationMark, 0, "ContinuationRep" }, /* base */
    rep_continuation, /* tag */
    {
     /* binary */
        0, 0,
     0, 0,
     0, 0,
     0, ValueEqual,
     0, 0,
     },
    {
     /* unary */
        0, 0,
     0, },
    0,
    0,
    ContinuationPrint,
    0,
    0,
};

Value
NewContinuation(ContinuationPtr continuation, InstPtr pc)
{
    ENTER();
    Value ret;

    ret = ALLOCATE(&ContinuationRep.data, sizeof(Continuation));
    ContinuationSet(&ret->continuation, continuation);
    ret->continuation.pc = pc;
    RETURN(ret);
}

static ContinuationPtr
EmptyContinuation(void)
{
    ENTER();
    Value ret;

    ret = ALLOCATE(&ContinuationRep.data, sizeof(Continuation));
    ContinuationInit(&ret->continuation);
    RETURN(&ret->continuation);
}

#ifdef DEBUG_JUMP
int dump_jump = 0;

void
ContinuationTrace(char *where, Continuation *continuation, int indent)
{
    int          s;
    StackObject *stack = continuation->stack;
    CatchPtr     catches = continuation->catches;
    TwixtPtr     twixts = continuation->twixts;
    ObjPtr       obj = continuation->obj;
    InstPtr      pc = continuation->pc;

    if (!dump_jump)
        return;
    TraceIndent(indent);
    FilePuts(FileStdout, "*** ");
    FilePuts(FileStdout, where);
    FilePuts(FileStdout, " ***\n");
    TraceIndent(indent);
    FilePuts(FileStdout, "stack:     ");
    for (s = 0; STACK_TOP(stack) + (s) < STACK_MAX(stack); s++) {
        if (s)
            FilePuts(FileStdout, ", ");
        FilePrintf(FileStdout, "%g", STACK_ELT(stack, s));
    }
    FilePuts(FileStdout, "\n");
    TraceIndent(indent);
    FilePuts(FileStdout, "frame:\nCALLS\n");
    TraceFrame(FileStdout, continuation->frame, obj, pc, 20);
    FilePuts(FileStdout, "END CALLS\n");
    TraceIndent(indent);
    FilePuts(FileStdout, "catches:   ");
    for (s = 0; catches; catches = catches->continuation.catches, s++) {
        if (s)
            FilePuts(FileStdout, ", ");
        FilePrintf(FileStdout, "%A", catches->exception->symbol.name);
    }
    FilePuts(FileStdout, "\n");
    TraceIndent(indent);
    FilePuts(FileStdout, "statement: ");
    if (obj && pc)
        PrettyStat(FileStdout, ObjStatement(obj, pc), False);
    else
        FilePuts(FileStdout, "corrupted continuation!\n");
    for (s = 0; twixts; twixts = twixts->continuation.twixts, s++) {
        ContinuationTrace("twixt", &twixts->continuation, indent + 1);
    }
}
#endif

InstPtr
ContinuationSet(ContinuationPtr dst, ContinuationPtr src)
{
    ENTER();
    dst->value = src->value;
    dst->pc = 0;
    dst->obj = src->obj;
    dst->frame = src->frame;
    dst->stack = 0;
    dst->catches = src->catches;
    dst->twixts = src->twixts;
    /* last, to make sure remaining entries are initialized before any GC */
    dst->stack = StackCopy(src->stack);
    RETURN(src->pc);
}

void
ContinuationInit(ContinuationPtr dst)
{
    dst->pc = 0;
    dst->obj = 0;
    dst->frame = 0;
    dst->value = Void;
    dst->catches = 0;
    dst->twixts = 0;
    dst->stack = 0;
    dst->stack = StackCreate();
}

static void
MarkJump(void *object)
{
    JumpPtr jump = object;

    MemReference(jump->enter);
    MemReference(jump->entering);
    MemReference(jump->leave);
    MemReference(jump->parent);
    MemReference(jump->continuation);
    MemReference(jump->ret);
}

static DataType JumpType = { MarkJump, 0, "JumpType" };

JumpPtr
NewJump(TwixtPtr leave, TwixtPtr enter, TwixtPtr parent, ContinuationPtr continuation, Value ret)
{
    ENTER();
    JumpPtr jump;

    jump = ALLOCATE(&JumpType, sizeof(Jump));
    jump->leave = leave;
    jump->enter = enter;
    jump->entering = TwixtNext(parent, enter);
    jump->parent = parent;
    jump->continuation = continuation;
    jump->ret = ret;
    RETURN(jump);
}

/*
 * An unhandled exception will attempt to jump to NULL,
 * catch that and invoke the debugger.  When the exception
 * was raised, the code carefully pushed a continuation from
 * the point of the exception to pass to the debugger
 */

static void
JumpUnhandledException(Value thread)
{
    Value continuation = STACK_POP(thread->thread.continuation.stack);

    /* make exec loop reschedule */
    if (thread == running)
        SetSignalError();
    /* A killed thread pushed no continuation for the debugger; it just ends */
    if (continuation == Void) {
        thread->thread.continuation.value = Void;
        ThreadFinish(thread, False);
        return;
    }
    DebugStart(continuation);
    ThreadFinish(thread, True);
}

/*
 * Figure out where to go next in a longjmp through twixts
 */
Value
JumpContinue(Value thread, InstPtr *next)
{
    ENTER();
    JumpPtr  jump = thread->thread.jump;
    TwixtPtr twixt;

    if (jump->leave) {
        /*
         * Going up
         */
        twixt = jump->leave;
        ContinuationSet(&thread->thread.continuation, &twixt->continuation);
        *next = twixt->leave;
        jump->leave = twixt->continuation.twixts;
        /*
         * Matching element of the twixt chain, next time start
         * back down the other side
         */
        if (jump->leave == jump->parent)
            jump->leave = 0;
    } else if (jump->entering) {
        /*
         * Going down
         */
        twixt = jump->entering;
        *next = ContinuationSet(&thread->thread.continuation, &twixt->continuation);
        jump->entering = TwixtNext(jump->entering, jump->enter);
    } else {
        /*
         * All done, set to final continuation and drop the jump object
         */
        *next = ContinuationSet(&thread->thread.continuation, jump->continuation);
        thread->thread.jump = 0;
    }
    if (!*next)
        JumpUnhandledException(thread);
    RETURN(jump->ret);
}

/*
 * Build a Jump that threads through all of the necessary twixt blocks
 * ending up at 'continuation' returning 'ret'
 */
InstPtr
JumpStart(Value thread, ContinuationPtr continuation, Value ret)
{
    ENTER();
    int      diff;
    TwixtPtr leave = thread->thread.continuation.twixts;
    TwixtPtr enter = continuation->twixts;
    TwixtPtr leave_parent, enter_parent, parent;
    InstPtr  next;

    /*
     * Make both lists the same length.  Note that either can be empty
     */
    leave_parent = leave;
    enter_parent = enter;
    diff = TwixtDepth(leave_parent) - TwixtDepth(enter_parent);
    if (diff >= 0)
        while (diff-- > 0)
            leave_parent = leave_parent->continuation.twixts;
    else
        while (diff++ < 0)
            enter_parent = enter_parent->continuation.twixts;
    /*
     * Now find the common parent
     */
    while (leave_parent != enter_parent) {
        leave_parent = leave_parent->continuation.twixts;
        enter_parent = enter_parent->continuation.twixts;
    }

    parent = enter_parent;
    /*
     * Build a data structure to get from leave to enter via parent
     */
    thread->thread.jump = NewJump(leave, enter, parent, continuation, ret);
    /*
     * Don't need the jump return value yet; we're off to the twixt
     * blocks; after that, the return value will get retrieved by the
     * final OpLeaveDone or OpEnterDone instruction
     */
    (void)JumpContinue(thread, &next);
    RETURN(next);
}

Value
ContinuationJump(Value thread, ContinuationPtr continuation, Value ret, InstPtr *next)
{
#ifdef DEBUG_JUMP
    ContinuationTrace("ContinuationJump from", &thread->thread.continuation, 1);
    ContinuationTrace("ContinuationJump to", continuation, 1);
#endif
    ENTER();
    /*
     * If there are twixt enter or leave blocks to execute, build a Jump
     * that walks them and then resets the continuation.
     *
     * Otherwise, just jump
     */
    if (thread->thread.continuation.twixts != continuation->twixts)
        *next = JumpStart(thread, continuation, ret);
    else
        *next = ContinuationSet(&thread->thread.continuation, continuation);
    if (!*next)
        JumpUnhandledException(thread);
    RETURN(ret);
}

/*
 * It is necessary that SetJump and LongJump have the same number
 * of arguments -- the arguments pushed by SetJump will have to be
 * popped when LongJump executes.  If this is not so, the stack copy
 * created here should be adjusted to account for this difference
 */
Value
do_setjmp(Value continuation_ref, Value ret)
{
    ENTER();
    Value continuation;

    if (!ValueIsRef(continuation_ref)) {
        RaiseStandardException(exception_invalid_argument, 3,
                               NewStrString("setjump: not a reference"), NewInt(0),
                               continuation_ref);
        RETURN(Void);
    }
    continuation
        = NewContinuation(&running->thread.continuation, running->thread.continuation.pc + 1);
    /*
     * Adjust stack for set jump return
     */
    STACK_DROP(continuation->continuation.stack, 2);
    RefValueSet(continuation_ref, continuation);
#ifdef DEBUG_JUMP
    ContinuationTrace("do_setjmp", &continuation->continuation, 1);
#endif
    RETURN(ret);
}

Value
do_longjmp(InstPtr *next, Value continuation, Value ret)
{
    ENTER();

    if (!running)
        RETURN(Void);
    if (!ValueIsContinuation(continuation)) {
        RaiseStandardException(exception_invalid_argument, 3,
                               NewStrString("longjmp: non-continuation argument"), NewInt(0),
                               continuation);
        RETURN(Void);
    }
    RETURN(ContinuationJump(running, &continuation->continuation, ret, next));
}

static void
CatchMark(void *object)
{
    CatchPtr catch = object;

    ContinuationMark(&catch->continuation);
    MemReference(catch->exception);
}

static DataType CatchType = { CatchMark, 0, "CatchType" };

CatchPtr
NewCatch(Value thread, SymbolPtr exception)
{
    ENTER();
    CatchPtr catch;

    catch = ALLOCATE(&CatchType, sizeof(Catch));
    catch->exception = exception;
    ContinuationSet(&catch->continuation, &thread->thread.continuation);
    catch->continuation.pc = thread->thread.continuation.pc + 1;
    RETURN(catch);
}

static void
TwixtMark(void *object)
{
    TwixtPtr twixt = object;

    ContinuationMark(&twixt->continuation);
}

static DataType TwixtType = { TwixtMark, 0, "TwixtType" };

TwixtPtr
NewTwixt(ContinuationPtr continuation, InstPtr enter, InstPtr leave)
{
    ENTER();
    TwixtPtr twixt;

    twixt = ALLOCATE(&TwixtType, sizeof(Twixt));
    twixt->leave = leave;
    if (continuation->twixts)
        twixt->depth = continuation->twixts->depth + 1;
    else
        twixt->depth = 1;
    ContinuationSet(&twixt->continuation, continuation);
    twixt->continuation.pc = enter;
    RETURN(twixt);
}

/*
 * Twixts are chained deepest first.  Walking
 * down the list is a bit of work
 */

TwixtPtr
TwixtNext(TwixtPtr twixt, TwixtPtr last)
{
    if (last == twixt)
        return 0;
    while (last->continuation.twixts != twixt)
        last = last->continuation.twixts;
    return last;
}

void
RaiseException(Value thread, SymbolPtr except, Value args, InstPtr *next)
{
    ENTER();
    CatchPtr        catch;
    ContinuationPtr continuation = 0;

    for (catch = thread->thread.continuation.catches; catch; catch = catch->continuation.catches) {
        if (catch->exception == except) {
            continuation = &catch->continuation;
            /*
             * Hold a reference to this nested value because
             * ContinuationJump is about to smash the thread
             */
            REFERENCE(catch);
            break;
        }
    }
    /* unhandled exception -- build an empty continuation and jump to it */
    if (!continuation) {
        Value debug;

        if (except == standardExceptions[exception_thread_killed]) {
            /*
             * Thread::kill: the jump still runs the twixt leave
             * expressions, but Void tells JumpUnhandledException to end
             * the thread quietly
             */
            debug = Void;
        } else {
            ptrdiff_t i;
            InstPtr   pc = thread->thread.continuation.pc;

            PrintError("Unhandled exception %A (", except->symbol.name);
            if (args) {
                ptrdiff_t dim = ArrayLimits(&args->array)[0];
                for (i = 0; i < dim; i++) {
                    PrintError("%g", ArrayValue(&args->array, i));
                    if (i < dim - 1)
                        PrintError(", ");
                }
            }
            PrintError(")\n");
            TraceFrame(FileStderr, thread->thread.continuation.frame,
                       thread->thread.continuation.obj, pc, 20);
            debug = NewContinuation(&thread->thread.continuation, pc);
        }
        continuation = EmptyContinuation();
        STACK_PUSH(continuation->stack, debug);
    }
    ContinuationJump(thread, continuation, args, next);
    EXIT();
}

static ReferencePtr standardExceptionArgsRef;

void
RegisterStandardException(StandardException se, SymbolPtr sym)
{
    ENTER();
    standardExceptions[se] = sym;
    MemAddRoot(sym);
    if (!standardExceptionArgsRef) {
        standardExceptionArgsRef = NewReference((void **)&standardExceptionArgs);
        MemAddRoot(standardExceptionArgsRef);
    }
    EXIT();
}

SymbolPtr
CheckStandardException(void)
{
    SymbolPtr except = standardExceptions[standardException];

    signalException = False;
    /* Leave aborting set only if another signal still needs it */
    aborting = signalInterrupt || signalSuspend || signalFinished || signalError;
    standardException = exception_none;
    standardExceptionArgs = 0;
    return except;
}

void
RaiseStandardException(StandardException se, int argc, ...)
{
    ENTER();
    Value     args;
    ptrdiff_t i;
    va_list   va;

    va_start(va, argc);
    args = NewArray(False, False, typePoly, 1, &(ptrdiff_t) { argc });
    for (i = 0; i < argc; i++)
        ArrayValueSet(&args->array, i, va_arg(va, Value));
    standardException = se;
    standardExceptionArgs = args;
    SetSignalException();
    EXIT();
}

Value
JumpStandardException(Value thread, InstPtr *next)
{
    ENTER();
    SymbolPtr except = standardExceptions[standardException];
    Value     args = standardExceptionArgs;

    aborting = False;
    if (except)
        RaiseException(thread, except, args, next);
    standardException = exception_none;
    standardExceptionArgs = 0;
    RETURN(args);
}

static void
SignalThread(Value thread, Value signal, Bool executing)
{
    ENTER();
    Value args = NewArray(False, False, typePoly, 1, &(ptrdiff_t) { 1 });

    ArrayValueSet(&args->array, 0, signal);
    ThreadRaise(thread, exception_signal, args, executing);
    EXIT();
}

void
ThreadsSignal(Value signal)
{
    ENTER();
    Value thread, next;

    /* do running first -- signalling makes threads run */
    for (thread = running; thread; thread = next) {
        next = thread->thread.next;
        SignalThread(thread, signal, False);
    }
    for (thread = stopped; thread; thread = next) {
        next = thread->thread.next;
        SignalThread(thread, signal, False);
    }
    EXIT();
}

Value
do_Thread_signal(Value thread, Value signal)
{
    ENTER();
    SignalThread(thread, signal, True);
    RETURN(Void);
}
