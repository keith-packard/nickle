#include    "mem.h"
#include    "stack.h"

typedef struct _Stuff {
    DataType	*type;
    int		v;
} Stuff;

static DataType stuffType = { 0, 0, "stuffType" };

main ()
{
    StackObject	*stack;
    Stuff	*stuff;
    int		i;

#ifndef NDEBUG
    GCdebug = 2;
#endif
    MemInitialize ();
    {
	ENTER ();
	stack = StackCreate ();
	MemAddRoot (stack);
	for (i = 0; i < 100; i++)
	{
	    stuff = ALLOCATE (&stuffType, sizeof (Stuff));
	    stuff->v = i;
	    StackPush (stack, stuff);
	}
	EXIT();
    }
    MemCollect ();
    for (i = 0; i < 100; i++)
	StackPop (stack);
    MemCollect ();
}
