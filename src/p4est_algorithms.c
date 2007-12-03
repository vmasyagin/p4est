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

#include <p4est_algorithms.h>
#include <p4est_base.h>

static const int8_t log_lookup_table[256] =
  { -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
};

#define P4EST_LOG2_8(x) (log_lookup_table[(x)])
#define P4EST_LOG2_16(x) (((x) > 0xff) ? \
                          (P4EST_LOG2_8 ((x) >> 8) + 8) : P4EST_LOG2_8 (x))
#define P4EST_LOG2_32(x) (((x) > 0xffff) ? \
                          (P4EST_LOG2_16 ((x) >> 16)) + 16 : P4EST_LOG2_16 (x))

/* here come small auxiliary functions */

int
p4est_quadrant_compare (const void *v1, const void *v2)
{
  const p4est_quadrant_t *q1 = v1;
  const p4est_quadrant_t *q2 = v2;

  int32_t             exclorx, exclory;

  P4EST_ASSERT (p4est_quadrant_is_valid (q1));
  P4EST_ASSERT (p4est_quadrant_is_valid (q2));

  exclorx = q1->x ^ q2->x;
  exclory = q1->y ^ q2->y;

  if (exclory == 0 && exclorx == 0) {
    return q1->level - q2->level;
  }
  else if (P4EST_LOG2_32 (exclory) >= P4EST_LOG2_32 (exclorx)) {
    return q1->y - q2->y;
  }
  else {
    return q1->x - q2->x;
  }
}

int
p4est_quadrant_child_id (const p4est_quadrant_t * q)
{
  int                 id = 0;

  P4EST_ASSERT (p4est_quadrant_is_valid (q));

  id |= ((q->x & (1 << (P4EST_MAXLEVEL - q->level))) ? 0x01 : 0);
  id |= ((q->y & (1 << (P4EST_MAXLEVEL - q->level))) ? 0x02 : 0);

  return id;
}

int
p4est_quadrant_is_valid (const p4est_quadrant_t * q)
{
  return
    (q->level >= 0 && q->level <= P4EST_MAXLEVEL) &&
    (q->x >= 0 && q->x < (1 << P4EST_MAXLEVEL)) &&
    (q->y >= 0 && q->y < (1 << P4EST_MAXLEVEL)) &&
    ((q->x & ((1 << (P4EST_MAXLEVEL - q->level)) - 1)) == 0) &&
    ((q->y & ((1 << (P4EST_MAXLEVEL - q->level)) - 1)) == 0);
}

int
p4est_quadrant_is_equal (const p4est_quadrant_t * q1,
                         const p4est_quadrant_t * q2)
{
  P4EST_ASSERT (p4est_quadrant_is_valid (q1));
  P4EST_ASSERT (p4est_quadrant_is_valid (q2));

  return (q1->level == q2->level && q1->x == q2->x && q1->y == q2->y);
}

int
p4est_quadrant_is_sibling (const p4est_quadrant_t * q1,
                           const p4est_quadrant_t * q2)
{
  int32_t             exclorx, exclory;

  P4EST_ASSERT (p4est_quadrant_is_valid (q1));
  P4EST_ASSERT (p4est_quadrant_is_valid (q2));

  exclorx = q1->x ^ q2->x;
  exclory = q1->y ^ q2->y;
  if (exclorx == 0 && exclory == 0) {
    return 0;
  }

  return
    (q1->level == q2->level) &&
    ((exclorx & ~(1 << (P4EST_MAXLEVEL - q1->level))) == 0) &&
    ((exclory & ~(1 << (P4EST_MAXLEVEL - q1->level))) == 0);
}

int
p4est_quadrant_is_sibling_D (const p4est_quadrant_t * q1,
                             const p4est_quadrant_t * q2)
{
  p4est_quadrant_t    p1, p2;

  /* validity of q1 and q2 is asserted in p4est_quadrant_is_equal */
  if (p4est_quadrant_is_equal (q1, q2)) {
    return 0;
  }

  p4est_quadrant_parent (q1, &p1);
  p4est_quadrant_parent (q2, &p2);

  return p4est_quadrant_is_equal (&p1, &p2);
}

int
p4est_quadrant_is_parent (const p4est_quadrant_t * q,
                          const p4est_quadrant_t * r)
{
  P4EST_ASSERT (p4est_quadrant_is_valid (q));
  P4EST_ASSERT (p4est_quadrant_is_valid (r));

  return
    (q->level + 1 == r->level) &&
    (q->x == (r->x & ~(1 << (P4EST_MAXLEVEL - r->level)))) &&
    (q->y == (r->y & ~(1 << (P4EST_MAXLEVEL - r->level))));
}

int
p4est_quadrant_is_parent_D (const p4est_quadrant_t * q,
                            const p4est_quadrant_t * r)
{
  p4est_quadrant_t    p;

  P4EST_ASSERT (p4est_quadrant_is_valid (q));

  /* validity of r is asserted in p4est_quadrant_parent */
  p4est_quadrant_parent (r, &p);

  return p4est_quadrant_is_equal (q, &p);
}

int
p4est_quadrant_is_ancestor (const p4est_quadrant_t * q,
                            const p4est_quadrant_t * r)
{
  int32_t             exclorx;
  int32_t             exclory;

  P4EST_ASSERT (p4est_quadrant_is_valid (q));
  P4EST_ASSERT (p4est_quadrant_is_valid (r));

  if (q->level >= r->level) {
    return 0;
  }

  exclorx = (q->x ^ r->x) >> (P4EST_MAXLEVEL - q->level);
  exclory = (q->y ^ r->y) >> (P4EST_MAXLEVEL - q->level);

  return (exclorx == 0 && exclory == 0);
}

int
p4est_quadrant_is_ancestor_D (const p4est_quadrant_t * q,
                              const p4est_quadrant_t * r)
{
  p4est_quadrant_t    s;

  /* validity of q and r is asserted in p4est_quadrant_is_equal */
  if (p4est_quadrant_is_equal (q, r)) {
    return 0;
  }

  p4est_nearest_common_ancestor_D (q, r, &s);

  return p4est_quadrant_is_equal (q, &s);
}

int
p4est_quadrant_is_next (const p4est_quadrant_t * q,
                        const p4est_quadrant_t * r)
{
  int8_t              minlevel;
  int32_t             mask;
  int64_t             i1, i2;

  /* validity of q and r is asserted in p4est_quadrant_compare */
  if (p4est_quadrant_compare (q, r) >= 0) {
    return 0;
  }

  if (q->level > r->level) {
    /* check if q is the third child up to the common level */
    mask =
      (1 << (P4EST_MAXLEVEL - r->level)) - (1 << (P4EST_MAXLEVEL - q->level));
    if ((q->x & mask) != mask || (q->y & mask) != mask) {
      return 0;
    }
    minlevel = r->level;
  }
  else {
    minlevel = q->level;
  }
  i1 = p4est_quadrant_linear_id (q, minlevel);
  i2 = p4est_quadrant_linear_id (r, minlevel);

  return (i1 + 1 == i2);
}

int
p4est_quadrant_is_next_D (const p4est_quadrant_t * q,
                          const p4est_quadrant_t * r)
{
  int64_t             i1, i2;
  p4est_quadrant_t    a, b;

  /* validity of q and r is asserted in p4est_quadrant_compare */
  if (p4est_quadrant_compare (q, r) >= 0) {
    return 0;
  }

  a = *q;
  b = *r;
  while (a.level > b.level) {
    if (p4est_quadrant_child_id (&a) != 3) {
      return 0;
    }
    p4est_quadrant_parent (&a, &a);
  }
  i1 = p4est_quadrant_linear_id (&a, a.level);
  i2 = p4est_quadrant_linear_id (&b, a.level);

  return (i1 + 1 == i2);
}

void
p4est_quadrant_parent (const p4est_quadrant_t * q, p4est_quadrant_t * r)
{
  P4EST_ASSERT (p4est_quadrant_is_valid (q));
  P4EST_ASSERT (q->level > 0);

  r->x = q->x & ~(1 << (P4EST_MAXLEVEL - q->level));
  r->y = q->y & ~(1 << (P4EST_MAXLEVEL - q->level));
  r->level = (int8_t) (q->level - 1);

  P4EST_ASSERT (p4est_quadrant_is_valid (r));
}

void
p4est_quadrant_children (const p4est_quadrant_t * q,
                         p4est_quadrant_t * c0, p4est_quadrant_t * c1,
                         p4est_quadrant_t * c2, p4est_quadrant_t * c3)
{
  P4EST_ASSERT (p4est_quadrant_is_valid (q));
  P4EST_ASSERT (q->level < P4EST_MAXLEVEL);

  c0->x = q->x;
  c0->y = q->y;
  c0->level = (int8_t) (q->level + 1);

  c1->x = c0->x | (1 << (P4EST_MAXLEVEL - c0->level));
  c1->y = c0->y;
  c1->level = c0->level;

  c2->x = c0->x;
  c2->y = c0->y | (1 << (P4EST_MAXLEVEL - c0->level));
  c2->level = c0->level;

  c3->x = c1->x;
  c3->y = c2->y;
  c3->level = c0->level;

  P4EST_ASSERT (p4est_quadrant_is_valid (c0));
  P4EST_ASSERT (p4est_quadrant_is_valid (c1));
  P4EST_ASSERT (p4est_quadrant_is_valid (c2));
  P4EST_ASSERT (p4est_quadrant_is_valid (c3));
}

void
p4est_nearest_common_ancestor (const p4est_quadrant_t * q1,
                               const p4est_quadrant_t * q2,
                               p4est_quadrant_t * r)
{
  int32_t             exclorx, exclory;
  int32_t             maxclor, maxlevel;

  P4EST_ASSERT (p4est_quadrant_is_valid (q1));
  P4EST_ASSERT (p4est_quadrant_is_valid (q2));

  exclorx = q1->x ^ q2->x;
  exclory = q1->y ^ q2->y;
  maxclor = exclorx | exclory;
  maxlevel = P4EST_LOG2_32 (maxclor) + 1;

  r->x = q1->x & ~((1 << maxlevel) - 1);
  r->y = q1->y & ~((1 << maxlevel) - 1);
  r->level = (int8_t) P4EST_MIN (P4EST_MAXLEVEL - maxlevel,
                                 P4EST_MIN (q1->level, q2->level));

  P4EST_ASSERT (p4est_quadrant_is_valid (r));
}

void
p4est_nearest_common_ancestor_D (const p4est_quadrant_t * q1,
                                 const p4est_quadrant_t * q2,
                                 p4est_quadrant_t * r)
{
  p4est_quadrant_t    s1 = *q1;
  p4est_quadrant_t    s2 = *q2;

  P4EST_ASSERT (p4est_quadrant_is_valid (q1));
  P4EST_ASSERT (p4est_quadrant_is_valid (q2));

  /* first stage: promote the deepest one to the same level */
  while (s1.level > s2.level) {
    p4est_quadrant_parent (&s1, &s1);
  }
  while (s1.level < s2.level) {
    p4est_quadrant_parent (&s2, &s2);
  }

  /* second stage: simultaneously go through their parents */
  while (!p4est_quadrant_is_equal (&s1, &s2)) {
    p4est_quadrant_parent (&s1, &s1);
    p4est_quadrant_parent (&s2, &s2);
  }

  /* don't overwrite r's user_data */
  r->x = s1.x;
  r->y = s1.y;
  r->level = s1.level;

  P4EST_ASSERT (p4est_quadrant_is_valid (r));
}

int64_t
p4est_quadrant_linear_id (const p4est_quadrant_t * quadrant, int8_t level)
{
  int8_t              i;
  int64_t             x, y;
  int64_t             id;

  P4EST_ASSERT (p4est_quadrant_is_valid (quadrant));
  P4EST_ASSERT (quadrant->level >= level && level >= 0);

  x = quadrant->x >> (P4EST_MAXLEVEL - level);
  y = quadrant->y >> (P4EST_MAXLEVEL - level);

  id = 0;
  for (i = 0; i < level; ++i) {
    id |= ((x & (1 << i)) << i);
    id |= ((y & (1 << i)) << (i + 1));
  }

  return id;
}

void
p4est_quadrant_set_morton (p4est_quadrant_t * quadrant,
                           int8_t level, int64_t id)
{
  int8_t              i;

  P4EST_ASSERT (0 <= level && level <= P4EST_MAXLEVEL);
  P4EST_ASSERT (id < (1LL << (2 * level)));

  quadrant->level = level;
  quadrant->x = 0;
  quadrant->y = 0;

  for (i = 0; i < level; ++i) {
    quadrant->x |= (int32_t) ((id & (1LL << (2 * i))) >> i);
    quadrant->y |= (int32_t) ((id & (1LL << (2 * i + 1))) >> (i + 1));
  }

  quadrant->x <<= (P4EST_MAXLEVEL - level);
  quadrant->y <<= (P4EST_MAXLEVEL - level);

  P4EST_ASSERT (p4est_quadrant_is_valid (quadrant));
}

void
p4est_quadrant_init_data (p4est_t * p4est, int32_t which_tree,
                          p4est_quadrant_t * quad, p4est_init_t init_fn)
{
  P4EST_ASSERT (p4est_quadrant_is_valid (quad));

  if (p4est->data_size > 0) {
    quad->user_data = p4est_mempool_alloc (p4est->user_data_pool);
  }
  else {
    quad->user_data = NULL;
  }
  if (init_fn != NULL) {
    init_fn (p4est, which_tree, quad);
  }
}

void
p4est_quadrant_free_data (p4est_t * p4est, p4est_quadrant_t * quad)
{
  P4EST_ASSERT (p4est_quadrant_is_valid (quad));

  if (p4est->data_size > 0) {
    p4est_mempool_free (p4est->user_data_pool, quad->user_data);
  }
  quad->user_data = NULL;
}

int
p4est_tree_is_sorted (p4est_tree_t * tree)
{
  int                 i;
  p4est_quadrant_t   *q1, *q2;

  if (tree->quadrants->elem_count <= 1) {
    return 1;
  }

  q1 = p4est_array_index (tree->quadrants, 0);
  for (i = 1; i < tree->quadrants->elem_count; ++i) {
    q2 = p4est_array_index (tree->quadrants, i);
    if (p4est_quadrant_compare (q1, q2) >= 0) {
      return 0;
    }
    q1 = q2;
  }

  return 1;
}

int
p4est_tree_is_complete (p4est_tree_t * tree)
{
  int                 i;
  p4est_quadrant_t   *q1, *q2;

  if (tree->quadrants->elem_count <= 1) {
    return 1;
  }

  q1 = p4est_array_index (tree->quadrants, 0);
  for (i = 1; i < tree->quadrants->elem_count; ++i) {
    q2 = p4est_array_index (tree->quadrants, i);
    if (!p4est_quadrant_is_next (q1, q2)) {
      return 0;
    }
    q1 = q2;
  }

  return 1;
}

void
p4est_tree_print (p4est_tree_t * tree, int identifier, FILE * nout)
{
  int                 j, childid, comp;
  char                prefix[BUFSIZ];
  p4est_quadrant_t   *q1, *q2;

  if (nout == NULL) {
    return;
  }

  if (identifier >= 0) {
    snprintf (prefix, BUFSIZ, "[%d] ", identifier);
  }
  else {
    prefix[0] = '\0';
  }

  q1 = NULL;
  for (j = 0; j < tree->quadrants->elem_count; ++j) {
    q2 = p4est_array_index (tree->quadrants, j);
    childid = p4est_quadrant_child_id (q2);
    fprintf (nout, "%s0x%x 0x%x %d", prefix, q2->x, q2->y, q2->level);
    if (j > 0) {
      comp = p4est_quadrant_compare (q1, q2);
      if (comp > 0) {
        fputs (" R", nout);
      }
      else if (comp == 0) {
        fputs (" I", nout);
      }
      else {
        if (p4est_quadrant_is_sibling (q1, q2)) {
          fprintf (nout, " S%d", childid);
        }
        else if (p4est_quadrant_is_parent (q1, q2)) {
          fprintf (nout, " C%d", childid);
        }
        else if (p4est_quadrant_is_ancestor (q1, q2)) {
          fputs (" D", nout);
        }
        else if (p4est_quadrant_is_next (q1, q2)) {
          fprintf (nout, " N%d", childid);
        }
        else {
          fprintf (nout, " Q%d", childid);
        }
      }
    }
    else {
      fprintf (nout, " Q%d", childid);
    }
    fputs ("\n", nout);
    q1 = q2;
  }
}

/* here come the heavyweight algorithms */

void
p4est_complete_region (p4est_t * p4est,
                       const p4est_quadrant_t * q1,
                       int include_q1,
                       const p4est_quadrant_t * q2,
                       int include_q2,
                       p4est_tree_t * tree,
                       int32_t which_tree, p4est_init_t init_fn)
{
  p4est_tree_t       *R;
  p4est_list_t       *W;

  p4est_quadrant_t    a = *q1;
  p4est_quadrant_t    b = *q2;

  p4est_quadrant_t    Afinest;
  p4est_quadrant_t   *c0, *c1, *c2, *c3;

  p4est_array_t      *quadrants;
  p4est_mempool_t    *quadrant_pool = p4est->quadrant_pool;

  p4est_quadrant_t   *w;
  p4est_quadrant_t   *r;

  int                 comp;
  int                 quadrant_pool_size, data_pool_size;
  int8_t              level;
  int8_t              maxlevel = 0;
  int32_t            *quadrants_per_level;
  int32_t             num_quadrants = 0;

  W = p4est_list_new (NULL);
  R = tree;

  /* needed for sanity check */
  quadrant_pool_size = p4est->quadrant_pool->elem_count;
  if (p4est->user_data_pool != NULL) {
    data_pool_size = p4est->user_data_pool->elem_count;
  }

  quadrants = R->quadrants;
  quadrants_per_level = R->quadrants_per_level;

  /* Assert that we have an empty tree */
  P4EST_ASSERT (quadrants->elem_count == 0);

  comp = p4est_quadrant_compare (&a, &b);
  /* Assert that a<b */
  P4EST_ASSERT (comp < 0);

  /* R <- R + a */
  if (include_q1) {
    p4est_array_resize (quadrants, 1);
    r = p4est_array_index (quadrants, 0);
    *r = a;
    maxlevel = (int8_t) P4EST_MAX (a.level, maxlevel);
    ++quadrants_per_level[a.level];
    ++num_quadrants;
  }

  if (comp < 0) {
    /* W <- C(A_{finest}(a,b)) */
    p4est_nearest_common_ancestor (&a, &b, &Afinest);

    c0 = p4est_mempool_alloc (quadrant_pool);
    c1 = p4est_mempool_alloc (quadrant_pool);
    c2 = p4est_mempool_alloc (quadrant_pool);
    c3 = p4est_mempool_alloc (quadrant_pool);

    p4est_quadrant_children (&Afinest, c0, c1, c2, c3);

    p4est_list_append (W, c0);
    p4est_list_append (W, c1);
    p4est_list_append (W, c2);
    p4est_list_append (W, c3);

    /* for each w in W */
    while (W->elem_count > 0) {
      w = p4est_list_pop (W);
      level = w->level;

      /* if (a < w < b) and (w not in {A(b)}) */
      if (((p4est_quadrant_compare (&a, w) < 0) &&
           (p4est_quadrant_compare (w, &b) < 0)
          ) && !p4est_quadrant_is_ancestor (w, &b)
        ) {
        /* R <- R + w */
        p4est_array_resize (quadrants, num_quadrants + 1);
        r = p4est_array_index (quadrants, num_quadrants);
        *r = *w;
        p4est_quadrant_init_data (p4est, which_tree, r, init_fn);
        maxlevel = (int8_t) P4EST_MAX (level, maxlevel);
        ++quadrants_per_level[level];
        ++num_quadrants;
      }
      /* else if (w in {{A(a)}, {A(b)}}) */
      else if (p4est_quadrant_is_ancestor (w, &a)
               || p4est_quadrant_is_ancestor (w, &b)) {
        /* W <- W + C(w) */
        c0 = p4est_mempool_alloc (quadrant_pool);
        c1 = p4est_mempool_alloc (quadrant_pool);
        c2 = p4est_mempool_alloc (quadrant_pool);
        c3 = p4est_mempool_alloc (quadrant_pool);

        p4est_quadrant_children (w, c0, c1, c2, c3);

        p4est_list_prepend (W, c3);
        p4est_list_prepend (W, c2);
        p4est_list_prepend (W, c1);
        p4est_list_prepend (W, c0);
      }

      /* W <- W - w */
      p4est_mempool_free (quadrant_pool, w);
    }                           /* end for */

    /* R <- R + b */
    if (include_q2) {
      p4est_array_resize (quadrants, num_quadrants + 1);
      r = p4est_array_index (quadrants, num_quadrants);
      *r = b;
      maxlevel = (int8_t) P4EST_MAX (b.level, maxlevel);
      ++quadrants_per_level[b.level];
      ++num_quadrants;
    }
  }

  R->maxlevel = maxlevel;

  P4EST_ASSERT (W->first == NULL && W->last == NULL);
  p4est_list_destroy (W);

  P4EST_ASSERT (p4est_tree_is_complete (R));
  P4EST_ASSERT (quadrant_pool_size == p4est->quadrant_pool->elem_count);
  P4EST_ASSERT (num_quadrants == quadrants->elem_count);
  if (p4est->user_data_pool != NULL) {
    P4EST_ASSERT (data_pool_size + quadrants->elem_count ==
                  p4est->user_data_pool->elem_count + (include_q1 ? 1 : 0)
                  + (include_q2 ? 1 : 0));
  }
}

/* EOF p4est_algorithms.c */
