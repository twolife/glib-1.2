/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#undef G_LOG_DOMAIN

#include <stdio.h>
#include <string.h>
#include "glib.h"

int array[10000];
gboolean failed = FALSE;

#define	TEST(m,cond)	G_STMT_START { failed = !(cond); \
if (failed) \
  { if (!m) \
      g_print ("\n(%s:%d) failed for: %s\n", __FILE__, __LINE__, ( # cond )); \
    else \
      g_print ("\n(%s:%d) failed for: %s: (%s)\n", __FILE__, __LINE__, ( # cond ), (gchar*)m); \
  } \
else \
  g_print ("."); fflush (stdout); \
} G_STMT_END

#define	C2P(c)		((gpointer) ((long) (c)))
#define	P2C(p)		((gchar) ((long) (p)))

#define GLIB_TEST_STRING "el dorado "
#define GLIB_TEST_STRING_5 "el do"

typedef struct {
	guint age;
	gchar name[40];
} GlibTestInfo;


static gboolean
my_hash_callback_remove (gpointer key,
			 gpointer value,
			 gpointer user_data)
{
  int *d = value;

  if ((*d) % 2)
    return TRUE;

  return FALSE;
}

static void
my_hash_callback_remove_test (gpointer key,
			      gpointer value,
			      gpointer user_data)
{
  int *d = value;

  if ((*d) % 2)
    g_print ("bad!\n");
}

static void
my_hash_callback (gpointer key,
		  gpointer value,
		  gpointer user_data)
{
  int *d = value;
  *d = 1;
}

static guint
my_hash (gconstpointer key)
{
  return (guint) *((const gint*) key);
}

static gint
my_hash_compare (gconstpointer a,
		 gconstpointer b)
{
  return *((const gint*) a) == *((const gint*) b);
}


int
main (int   argc,
      char *argv[])
{
  GHashTable *hash_table;
  gint i;

  hash_table = g_hash_table_new (my_hash, my_hash_compare);
  for (i = 0; i < 10000; i++)
    {
      array[i] = i;
      g_hash_table_insert (hash_table, &array[i], &array[i]);
    }
  g_hash_table_foreach (hash_table, my_hash_callback, NULL);

  for (i = 0; i < 10000; i++)
    if (array[i] == 0)
      g_assert_not_reached();

  for (i = 0; i < 10000; i++)
    g_hash_table_remove (hash_table, &array[i]);

  for (i = 0; i < 10000; i++)
    {
      array[i] = i;
      g_hash_table_insert (hash_table, &array[i], &array[i]);
    }

  if (g_hash_table_foreach_remove (hash_table, my_hash_callback_remove, NULL) != 5000 ||
      g_hash_table_size (hash_table) != 5000)
    g_assert_not_reached();

  g_hash_table_foreach (hash_table, my_hash_callback_remove_test, NULL);


  g_hash_table_destroy (hash_table);

  return 0;

}