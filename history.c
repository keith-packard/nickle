/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * history.c
 *
 * save previous printed values
 */

#include	"nickle.h"

# define HISTORY_INCREMENT	100

typedef struct _History { 
    DataType	*type;
    int		index;
    int		size;
    DataType	**arrayBase;
    Value	*array;
} Hist;

static void
HistoryMark (void *object)
{
    Hist *history = object;
    int	    i;

    MemReference (history->arrayBase);
    for (i = 0; i < history->index; i++)
	MemReference (history->array[i]);
}

DataType    HistoryType = { HistoryMark, 0 };

static Hist	*hist;

void
HistoryInit (void)
{
    ENTER ();
    hist = ALLOCATE (&HistoryType, sizeof (Hist));
    MemAddRoot (hist);
    EXIT ();
}

void
HistoryInsert (Value v)
{
    ENTER ();
    if (hist->size == hist->index) 
    {
	DataType    **arrayBase;
	Value	    *array;

	hist->size += HISTORY_INCREMENT;
	arrayBase = ALLOCATE (0, sizeof (DataType *) + hist->size * sizeof (Value));
	array = (Value *) (arrayBase + 1);
	if (hist->array)
	    memcpy (array, hist->array, hist->index * sizeof (Value));
	hist->arrayBase = arrayBase;
	hist->array = array;
    }
    hist->array[hist->index++] = v;
    EXIT ();
}

Value
History (Value thread, Value h)
{
    int	i;

    i = IntPart (h, "Invalid history offset");
    if (aborting)
	return Zero;
    if (i == 0)
    {
	if (hist->index == 0)
	    return Zero;
	return hist->array[hist->index-1];
    }
    if (i < 0)
    {
	i = -i;
	if (i >= hist->index)
	    return Zero;
	return hist->array[hist->index-1 - i];
    }
    else
    {
	if (i > hist->index)
	    return Zero;
	return hist->array[i - 1];
    }
}

void
HistoryDisplay (Value format, Value *from, Value *to)
{
    ENTER ();
    int	    f, t;
    Value   v[2];
    Value   fv;

    if (!from)
	f = hist->index - 10;
    else
    {
	f = IntPart (*from, "bad history value");
	if (!to)
	    f = hist->index + 1 - f;
    }
    if (!to)
	t = hist->index;
    else
	t = IntPart (*to, "bad history value");
    if (!aborting)
    {
	if (f <= 0)
	    f = 1;
	if (t > hist->index)
	    t = hist->index;
	v[0] = format;
	while (f <= t) {
	    fv = NewInt (f);
	    callformat (FileStdout, "$%-4d\t", 1, &fv);
	    v[1] = hist->array[f - 1];
	    do_printf (2, v);
	    callformat (FileStdout, "\n", 0, 0);
	    ++f;
	}
    }
    EXIT ();
}
