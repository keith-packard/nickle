/* $Header$ */

/*
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

/*
 * Semi-Balanced trees (avl).  This only contains two
 * routines - insert and delete.  Searching is
 * reserved for the client to write.
 */

#include	<config.h>

#include	<stdlib.h>
#include	"avl.h"

static int	rebalance_right (tree **), rebalance_left (tree **);

/*
 * insert a new node
 *
 * this routine returns non-zero if the tree has grown
 * taller
 */

int
tree_insert (tree **treep, tree *new)
{
	if (!(*treep)) {
		(*treep) = new;
		(*treep)->left = 0;
		(*treep)->right = 0;
		(*treep)->balance = 0;
		return 1;
	} else {
		if ((PtrInt) (*treep)->data < (PtrInt) new->data) {
			if (tree_insert (&((*treep)->right), new))
				switch (++(*treep)->balance) {
				case 0:
					return 0;
				case 1:
					return 1;
				case 2:
					(void) rebalance_right (treep);
				}
		} else if ((PtrInt) (*treep)->data > (PtrInt) new->data) {
			if (tree_insert (&((*treep)->left), new))
				switch (--(*treep)->balance) {
				case 0:
					return 0;
				case -1:
					return 1;
				case -2:
					(void) rebalance_left (treep);
				}
		}
	}
    	return 0;
}
					
/*
 * delete a node from a tree
 *
 * this routine return non-zero if the tree has been shortened
 */

int
tree_delete (tree **treep, tree *old)
{
	tree	*to_be_deleted;
	tree	*replacement;
	tree	*replacement_parent;
	int	replacement_direction;
	int	delete_direction;
	tree	*swap_temp;
	int	balance_temp;

	if (!*treep)
		/* node not found */
		return 0;
	if ((PtrInt) (*treep)->data < (PtrInt) old->data) {
		if (tree_delete (&(*treep)->right, old))
			/*
			 * check the balance factors
			 * Note that the conditions are
			 * inverted from the insertion case
			 */
			switch (--(*treep)->balance) {
			case 0:
				return 1;
			case -1:
				return 0;
			case -2:
				return rebalance_left (treep);
			}
		return 0;
	} else if ((PtrInt) (*treep)->data > (PtrInt) old->data) {
		if (tree_delete (&(*treep)->left, old))
			switch (++(*treep)->balance) {
			case 0:
				return 1;
			case 1:
				return 0;
			case 2:
				return rebalance_right (treep);
			}
		return 0;
	} else {
		to_be_deleted = *treep;
		/*
		 * find an empty down pointer (if any)
		 * and rehook the tree
		 */
		if (!to_be_deleted->right) {
			(*treep) = to_be_deleted->left;
			return 1;
		} else if (!to_be_deleted->left) {
			(*treep) = to_be_deleted->right;
			return 1;
		} else {
		/* 
		 * if both down pointers are full, then
		 * move a node from the bottom of the tree up here.
		 *
		 * This builds an incorrect tree -- the replacement
		 * node and the to_be_deleted node will not
		 * be in correct order.  This doesn't matter as
		 * the to_be_deleted node will obviously not leave
		 * this routine alive.
		 */
			/*
			 * if the tree is left heavy, then go left
			 * else go right
			 */
			replacement_parent = to_be_deleted;
			if (to_be_deleted->balance == -1) {
				delete_direction = -1;
				replacement_direction = -1;
				replacement = to_be_deleted->left;
				while (replacement->right) {
					replacement_parent = replacement;
					replacement_direction = 1;
					replacement = replacement->right;
				}
			} else {
				delete_direction = 1;
				replacement_direction = 1;
				replacement = to_be_deleted->right;
				while (replacement->left) {
					replacement_parent = replacement;
					replacement_direction = -1;
					replacement = replacement->left;
				}
			}
			/*
			 * swap the replacement node into
			 * the tree where the node is to be removed
			 * 
			 * this would be faster if only the data
			 * element was swapped -- but that
			 * won't work for kalypso.  The alternate
			 * code would be:
			   data_temp = to_be_deleted->data;
			   to _be_deleted->data = replacement->data;
			   replacement->data = data_temp;
			 */
			swap_temp = to_be_deleted->left;
			to_be_deleted->left = replacement->left;
			replacement->left = swap_temp;
			swap_temp = to_be_deleted->right;
			to_be_deleted->right = replacement->right;
			replacement->right = swap_temp;
			balance_temp = to_be_deleted->balance;
			to_be_deleted->balance = replacement->balance;
			replacement->balance = balance_temp;
			/*
			 * if the replacement node is directly below
			 * the to-be-removed node, hook the to_be_deleted
			 * node below it (instead of below itself!)
			 */
			if (replacement_parent == to_be_deleted)
				replacement_parent = replacement;
			if (replacement_direction == -1)
				replacement_parent->left = to_be_deleted;
			else
				replacement_parent->right = to_be_deleted;
			(*treep) = replacement;
			/*
			 * delete the node from the sub-tree
			 */
			if (delete_direction == -1) {
				if (tree_delete (&(*treep)->left, old)) {
					switch (++(*treep)->balance) {
					case 2:
						abort ();
					case 1:
						return 0;
					case 0:
						return 1;
					}
				}
				return 0;
			} else {
				if (tree_delete (&(*treep)->right, old)) {
					switch (--(*treep)->balance) {
					case -2:
						abort ();
					case -1:
						return 0;
					case 0:
						return 1;
					}
				}
				return 0;
			}
		}
	}
	/*NOTREACHED*/
}

/*
 * two routines to rebalance the tree.
 *
 * rebalance_right -- the right sub-tree is too long
 * rebalance_left --  the left sub-tree is too long
 *
 * These routines are the heart of avl trees, I've tried
 * to make their operation reasonably clear with comments,
 * but some study will be necessary to understand the
 * algorithm.
 *
 * these routines return non-zero if the resultant
 * tree is shorter than the un-balanced version.  This
 * is only of interest to the delete routine as the
 * balance after insertion can never actually shorten
 * the tree.
 */
 
static int
rebalance_right (tree **treep)
{
	tree	*temp;
	/*
	 * rebalance the tree
	 */
	if ((*treep)->right->balance == -1) {
		/* 
		 * double whammy -- the inner sub-sub tree
		 * is longer than the outer sub-sub tree
		 *
		 * this is the "double rotation" from
		 * knuth.  Scheme:  replace the tree top node
		 * with the inner sub-tree top node and
		 * adjust the maze of pointers and balance
		 * factors accordingly.
		 */
		temp = (*treep)->right->left;
		(*treep)->right->left = temp->right;
		temp->right = (*treep)->right;
		switch (temp->balance) {
		case -1:
			temp->right->balance = 1;
			(*treep)->balance = 0;
			break;
		case 0:
			temp->right->balance = 0;
			(*treep)->balance = 0;
			break;
		case 1:
			temp->right->balance = 0;
			(*treep)->balance = -1;
			break;
		}
		temp->balance = 0;
		(*treep)->right = temp->left;
		temp->left = (*treep);
		(*treep) = temp;
		return 1;
	} else {
		/*
		 * a simple single rotation
		 *
		 * Scheme:  replace the tree top node
		 * with the sub-tree top node 
		 */
		temp = (*treep)->right->left;
		(*treep)->right->left = (*treep);
		(*treep) = (*treep)->right;
		(*treep)->left->right = temp;
		/*
		 * only two possible configurations --
		 * if the right sub-tree was balanced, then
		 * *both* sides of it were longer than the
		 * left side, so the resultant tree will
		 * have a long leg (the left inner leg being
		 * the same length as the right leg)
		 */
		if ((*treep)->balance == 0) {
			(*treep)->balance = -1;
			(*treep)->left->balance = 1;
			return 0;
		} else {
			(*treep)->balance = 0;
			(*treep)->left->balance = 0;
			return 1;
		}
	}
}

static int
rebalance_left (tree **treep)
{
	tree	*temp;
	/*
	 * rebalance the tree
	 */
	if ((*treep)->left->balance == 1) {
		/* 
		 * double whammy -- the inner sub-sub tree
		 * is longer than the outer sub-sub tree
		 *
		 * this is the "double rotation" from
		 * knuth.  Scheme:  replace the tree top node
		 * with the inner sub-tree top node and
		 * adjust the maze of pointers and balance
		 * factors accordingly.
		 */
		temp = (*treep)->left->right;
		(*treep)->left->right = temp->left;
		temp->left = (*treep)->left;
		switch (temp->balance) {
		case 1:
			temp->left->balance = -1;
			(*treep)->balance = 0;
			break;
		case 0:
			temp->left->balance = 0;
			(*treep)->balance = 0;
			break;
		case -1:
			temp->left->balance = 0;
			(*treep)->balance = 1;
			break;
		}
		temp->balance = 0;
		(*treep)->left = temp->right;
		temp->right = (*treep);
		(*treep) = temp;
		return 1;
	} else {
		/*
		 * a simple single rotation
		 *
		 * Scheme:  replace the tree top node
		 * with the sub-tree top node 
		 */
		temp = (*treep)->left->right;
		(*treep)->left->right = (*treep);
		(*treep) = (*treep)->left;
		(*treep)->right->left = temp;
		/*
		 * only two possible configurations --
		 * if the left sub-tree was balanced, then
		 * *both* sides of it were longer than the
		 * right side, so the resultant tree will
		 * have a long leg (the right inner leg being
		 * the same length as the left leg)
		 */
		if ((*treep)->balance == 0) {
			(*treep)->balance = 1;
			(*treep)->right->balance = -1;
			return 0;
		} else {
			(*treep)->balance = 0;
			(*treep)->right->balance = 0;
			return 1;
		}
	}
}

#ifdef DEBUG

static 
depth (treep)
tree	*treep;
{
	int	ldepth, rdepth;

	if (!treep)
		return 0;
	ldepth = depth (treep->left);
	rdepth = depth (treep->right);
	if (ldepth > rdepth)
		return ldepth + 1;
	return rdepth + 1;
}

static tree *
left_most (treep)
tree	*treep;
{
	while (treep && treep->left)
		treep = treep->left;
	return treep;
}

static tree *
right_most (treep)
tree	*treep;
{
	while (treep && treep->right)
		treep = treep->right;
	return treep;
}

tree_verify (treep)
tree	*treep;
{
	tree_data	left_data, right_data;

	if (!treep)
		return 1;
	if (treep->left)
		left_data = right_most (treep->left)->data;
	else
		left_data = treep->data - 1;
	if (treep->right)
		right_data = left_most (treep->right)->data;
	else
		right_data = treep->data + 1;
	if (treep->data < left_data || treep->data > right_data) {
		abort ();
		return 0;
	}
	if (treep->balance != depth (treep->right) - depth (treep->left)) {
		abort ();
		return 0;
	}
	return tree_verify (treep->left) && tree_verify (treep->right);
}

#endif
