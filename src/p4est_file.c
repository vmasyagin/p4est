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

#include <p4est_file.h>
#include <p4est_base.h>

enum Section
{
  NONE,
  INFO,                         /* [Forest Info] */
  COORD,                        /* [Coordinates of Element Vertices] */
  ETOV,                         /* [Element to Vertex] */
  ETOE,                         /* [Element to Element] */
  ETOF,                         /* [Element to Face] */
  ET,                           /* [Element Tags] */
  FT,                           /* [Face Tags] */
  CF,                           /* [Curved Faces] */
  CT                            /* [Curved Types] */
};

static void
p4est_trim_comments (char *line)
{
  /* Truncate comments by setting them to '\0' */

  for (; *line != '\0'; ++line) {
    if (*line == '#') {
      *line = '\0';
      break;
    }
  }
}

static void
p4est_trim_ending_whitespace (char *line)
{
  int                 n;

  /* Trim whitespace from the end of the line */
  for (n = strlen (line) - 1; n >= 0; n--)
    if (!isspace (line[n]))
      break;
  line[n + 1] = '\0';
}

static char        *
p4est_trim_beginning_whitespace (char *line)
{
  /* Trim whitespace from the beginning of the line */
  for (; isspace (*line); ++line) {
  }

  return line;
}

int
p4est_connectivity_read (const char *filename,
                         p4est_connectivity_t ** connectivity)
{
  FILE               *file;
  size_t              length;
  int                 retval;
  char                buf[BUFSIZ];
  char               *line;
  enum Section        section = NONE;
  int                 section_lines_read = 0;
  int                 set_num_trees = 0;
  int                 set_num_vertices = 0;
  char               *key = NULL;
  char               *value = NULL;
  int32_t             num_trees = 0;
  int32_t             num_vertices = 0;
  int32_t             k, k0, k1, k2, k3, f0, f1, f2, f3, v0, v1, v2, v3;
  int32_t            *tree_to_vertex, *tree_to_tree;
  int8_t             *tree_to_face;

  *connectivity = NULL;

  file = fopen (filename, "rb");
  if (!file) {
    fprintf (stderr, "Failed to open p4est mesh file %s\n", filename);
    return 1;
  }

  /* loop through the lines of the file */
  while (fgets (buf, BUFSIZ, file)) {
    line = buf;

    p4est_trim_comments (line);

    p4est_trim_ending_whitespace (line);

    line = p4est_trim_beginning_whitespace (line);

    if (*line == '\0') {
      /* skip empty lines */
    }
    else if (*line == '[') {
      /* call any checks before leaving a section */
      switch (section) {
      case ETOV:
        P4EST_CHECK_ABORT (section_lines_read == num_trees,
                           "Not enough entries in [Element to Vertex]");
        break;
      case ETOE:
        P4EST_CHECK_ABORT (section_lines_read == num_trees,
                           "Not enough entries in [Element to Element]");
        break;
      case ETOF:
        P4EST_CHECK_ABORT (section_lines_read == num_trees,
                           "Not enough entries in [Element to Face]");
        break;
      default:
        ;
      }

      /* set section */
      length = strlen (line);

      /* Sections must end with ']' */
      P4EST_CHECK_ABORT (line[length - 1] == ']',
                         "Sections must end with ']'");

      line[length - 1] = '\0';
      ++line;

      if (strcmp (line, "Forest Info") == 0) {
        section = INFO;
      }
      else if (strcmp (line, "Coordinates of Element Vertices") == 0) {
        section = COORD;
      }
      else if (strcmp (line, "Element to Vertex") == 0) {
        section = ETOV;
      }
      else if (strcmp (line, "Element to Element") == 0) {
        section = ETOE;
      }
      else if (strcmp (line, "Element to Face") == 0) {
        section = ETOF;
      }
      else if (strcmp (line, "Element Tags") == 0) {
        section = ET;
      }
      else if (strcmp (line, "Face Tags") == 0) {
        section = FT;
      }
      else if (strcmp (line, "Curved Faces") == 0) {
        section = CF;
      }
      else if (strcmp (line, "Curved Types") == 0) {
        section = CT;
      }
      else {
        P4EST_CHECK_ABORT (0, "Unknown section in mesh file");
      }

      P4EST_CHECK_ABORT (section == INFO || *connectivity != NULL,
                         "The [Forest Info] setction must come first and set Nv and Nv.");

      section_lines_read = 0;
    }
    else {
      switch (section) {
      case INFO:
        key = strtok (line, "=");
        value = strtok (NULL, "=");

        P4EST_CHECK_ABORT (key != NULL && value != NULL,
                           "entries in the [Forest Info] setion must be\n"
                           "key value pairs, i.e. key=value");

        p4est_trim_ending_whitespace (key);

        if (strcmp (key, "Nk") == 0) {
          num_trees = atoi (value);
          set_num_trees = 1;
        }

        else if (strcmp (key, "Nv") == 0) {
          num_vertices = atoi (value);
          set_num_vertices = 1;
        }

        if (set_num_vertices && set_num_trees) {
          *connectivity = p4est_connectivity_new (num_trees, num_vertices);
          tree_to_vertex = (*connectivity)->tree_to_vertex;
          tree_to_tree = (*connectivity)->tree_to_tree;
          tree_to_face = (*connectivity)->tree_to_face;
        }

        break;
      case COORD:
        break;
      case ETOV:
        sscanf (line, "%d %d %d %d %d", &k, &v0, &v1, &v2, &v3);
        --k;
        --v0;
        --v1;
        --v2;
        --v3;

        P4EST_CHECK_ABORT (k >= 0 && k < num_trees &&
                           v0 >= 0 && v0 < num_vertices &&
                           v1 >= 0 && v1 < num_vertices &&
                           v2 >= 0 && v2 < num_vertices &&
                           v3 >= 0 && v3 < num_vertices,
                           "Bad [Element to Vertex] entry");

        tree_to_vertex[k * 4 + 0] = v0;
        tree_to_vertex[k * 4 + 1] = v1;
        tree_to_vertex[k * 4 + 2] = v2;
        tree_to_vertex[k * 4 + 3] = v3;

        break;
      case ETOE:
        sscanf (line, "%d %d %d %d %d", &k, &k0, &k1, &k2, &k3);
        --k;
        --k0;
        --k1;
        --k2;
        --k3;

        P4EST_CHECK_ABORT (k >= 0 && k < num_trees &&
                           k0 >= 0 && k0 < num_trees &&
                           k1 >= 0 && k1 < num_trees &&
                           k2 >= 0 && k2 < num_trees &&
                           k3 >= 0 && k3 < num_trees,
                           "Bad [Element to Element] entry");

        tree_to_tree[k * 4 + 0] = k0;
        tree_to_tree[k * 4 + 1] = k1;
        tree_to_tree[k * 4 + 2] = k2;
        tree_to_tree[k * 4 + 3] = k3;

        break;
      case ETOF:
        sscanf (line, "%d %d %d %d %d", &k, &f0, &f1, &f2, &f3);
        --k;
        --f0;
        --f1;
        --f2;
        --f3;

        P4EST_CHECK_ABORT (k >= 0 && k < num_trees &&
                           f0 >= 0 && f0 < 4 &&
                           f1 >= 0 && f1 < 4 &&
                           f2 >= 0 && f2 < 4 &&
                           f3 >= 0 && f3 < 4, "Bad [Element to Face] entry");

        tree_to_face[k * 4 + 0] = (int8_t) f0;
        tree_to_face[k * 4 + 1] = (int8_t) f1;
        tree_to_face[k * 4 + 2] = (int8_t) f2;
        tree_to_face[k * 4 + 3] = (int8_t) f3;

        break;
      case ET:
        break;
      case FT:
        break;
      case CF:
        break;
      case CT:
        break;
      case NONE:
        P4EST_CHECK_ABORT (0, "Mesh file must start with a section");
      default:
        P4EST_CHECK_ABORT (0, "Unknown section in mesh file");
      }

      ++section_lines_read;
    }
  }

  retval = fclose (file);
  if (retval) {
    fprintf (stderr, "Failed to close p4est mesh file %s (%d:%d)\n", filename,
             retval, EOF);
    return 1;
  }

  return 0;
}

void
p4est_connectivity_print (p4est_connectivity_t * connectivity)
{
  int                 k, num_trees, num_vertices;
  int32_t            *tree_to_vertex, *tree_to_tree;
  int8_t             *tree_to_face;

  num_trees = connectivity->num_trees;
  num_vertices = connectivity->num_vertices;
  tree_to_vertex = connectivity->tree_to_vertex;
  tree_to_tree = connectivity->tree_to_tree;
  tree_to_face = connectivity->tree_to_face;

  fprintf (stdout, "[Forest Info]\n");
  fprintf (stdout, "ver = 0.0.1  # Version of the forest file\n");
  fprintf (stdout, "Nk  = %d      # Number of elements\n", num_trees);
  fprintf (stdout, "Nv  = %d      # Number of mesh vertices\n", num_vertices);
  fprintf (stdout, "Net = 0      # Number of element tags\n");
  fprintf (stdout, "Nft = 0      # Number of face tags\n");
  fprintf (stdout, "Ncf = 0      # Number of curved faces\n");
  fprintf (stdout, "Nct = 0      # Number of curved types\n");
  fprintf (stdout, "\n");
  fprintf (stdout, "[Coordinates of Element Vertices]\n");
  fprintf (stdout, "[Element to Vertex]\n");
  for (k = 0; k < num_trees; ++k)
    printf ("    %d    %d    %d    %d    %d\n",
            k + 1, tree_to_vertex[4 * k + 0] + 1,
            tree_to_vertex[4 * k + 1] + 1, tree_to_vertex[4 * k + 2] + 1,
            tree_to_vertex[4 * k + 3] + 1);
  fprintf (stdout, "[Element to Element]\n");
  for (k = 0; k < num_trees; ++k)
    printf ("    %d    %d    %d    %d    %d\n",
            k + 1, tree_to_tree[4 * k + 0] + 1, tree_to_tree[4 * k + 1] + 1,
            tree_to_tree[4 * k + 2] + 1, tree_to_tree[4 * k + 3] + 1);
  fprintf (stdout, "[Element to Face]\n");
  for (k = 0; k < num_trees; ++k)
    printf ("    %d    %d    %d    %d    %d\n",
            k + 1, tree_to_face[4 * k + 0] + 1, tree_to_face[4 * k + 1] + 1,
            tree_to_face[4 * k + 2] + 1, tree_to_face[4 * k + 3] + 1);
  fprintf (stdout, "[Element Tags]\n");
  fprintf (stdout, "[Face Tags]\n");
  fprintf (stdout, "[Curved Faces]\n");
  fprintf (stdout, "[Curved Types]\n");
}

/* EOF p4est_file.h> */
