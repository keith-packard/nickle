/* $Header$ */

/*
 * Copyright (C) 1988-2001 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * avl_tree.h
 *
 * balanced binary tree
 */

typedef long PtrInt;

struct block {
	struct block		*left, *right;
	short			balance;
	short			sizeIndex;
	short			ref;
	short			bitmapsize;	/* chars in bitmap */
	int			datasize;	/* chars in data */
	unsigned char		*bitmap;
	unsigned char		*data;
};

typedef struct block	tree;
typedef unsigned char	*tree_data;
int	tree_insert (tree **, tree *);
int	tree_delete (tree **, tree *);
