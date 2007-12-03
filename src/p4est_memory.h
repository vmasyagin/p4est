/*
  This file is part of p4est.
  p4est is a C library to manage a parallel collection of quadtrees and/or octrees.

  Copyright (C) 2007 Carsten Burstedde, Lucas Wilcox.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __P4EST_MEMORY_H__
#define __P4EST_MEMORY_H__

/* p4est memory management */

#include <p4est_obstack.h>

/** The p4est_array object provides a large array of equal-size elements.
 * The array can be resized.
 * Elements are accessed by their 0-based index, their address may change.
 * The size (== elem_count) of the array can be changed by _grow1 or _resize.
 * Elements can be sorted.
 */
typedef struct p4est_array
{
  /* interface variables */
  int                 elem_size;        /* size of a single element */
  int                 elem_count;       /* number of valid elements */

  /* implementation variables */
  int                 elem_alloc;       /* number of allocated elements */
  char               *array;    /* linear array to store elements */
}
p4est_array_t;

p4est_array_t      *p4est_array_new (int elem_size);
void                p4est_array_destroy (p4est_array_t * array);

void                p4est_array_resize (p4est_array_t * array, int new_count);
void                p4est_array_sort (p4est_array_t * array,
                                      int (*compar) (const void *,
                                                     const void *));

void               *p4est_array_index (p4est_array_t * array, int index);

/** The p4est_mempool object provides a large pool of equal-size elements.
 * The pool grows dynamically for element allocation.
 * Elements are referenced by their address which never changes.
 * Elements can be freed (that is, returned to the pool)
 *    and are transparently reused.
 */
typedef struct p4est_mempool
{
  /* interface variables */
  int                 elem_size;        /* size of a single element */
  int                 elem_count;       /* number of valid elements */

  /* implementation variables */
  struct obstack      obstack;  /* holds the allocated elements */
  p4est_array_t      *freed;    /* buffers the freed elements */
}
p4est_mempool_t;

/* create, destroy or reset to count=0 the memory pool */
p4est_mempool_t    *p4est_mempool_new (int elem_size);
void                p4est_mempool_destroy (p4est_mempool_t * mempool);
void                p4est_mempool_reset (p4est_mempool_t * mempool);

/* allocate or free a single element */
void               *p4est_mempool_alloc (p4est_mempool_t * mempool);
void                p4est_mempool_free (p4est_mempool_t * mempool,
                                        void *elem);

/** The p4est_link is one link of a linked list.
 */
typedef struct p4est_link
{
  void               *data;
  struct p4est_link  *next;
}
p4est_link_t;

/** The p4est_list object provides a linked list.
 */
typedef struct p4est_list
{
  /* interface variables */
  int                 elem_count;
  p4est_link_t       *first;
  p4est_link_t       *last;

  /* implementation variables */
  int                 allocator_owned;
  p4est_mempool_t    *allocator;        /* must allocate sizeof (p4est_link_t) */
}
p4est_list_t;

/** Allocate a linked list structure
 * \param [in] allocator Memory allocator, can be NULL.
 */
p4est_list_t       *p4est_list_new (p4est_mempool_t * allocator);

/** Destroy a linked list structure
 * \note if allocator was provided in p4est_list_new, it will not be destroyed.
 */
void                p4est_list_destroy (p4est_list_t * list);

void                p4est_list_prepend (p4est_list_t * list, void *data);
void                p4est_list_append (p4est_list_t * list, void *data);

/** Insert a link after a given position
 */
void                p4est_list_insert (p4est_list_t * list,
                                       p4est_link_t * after, void *data);

/** Remove a link from the front of the list
 * \return Returns the removed first list element.
 */
void               *p4est_list_pop (p4est_list_t * list);

#endif /* !__P4EST_MEMORY_H__ */
