/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gdataset.c: Generic dataset mechanism, similar to GtkObject data.
 * Copyright (C) 1998 Tim Janik
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */
#include        <string.h>
#include	"glib.h"



/* --- defines --- */
#define	G_QUARK_BLOCK_SIZE			(512)
#define	G_DATA_MEM_CHUNK_PREALLOC		(128)
#define	G_DATA_CACHE_MAX			(512)
#define	G_DATASET_MEM_CHUNK_PREALLOC		(32)


/* --- structures --- */
typedef struct _GDataset GDataset;
struct _GData
{
  GData *next;
  GQuark id;
  gpointer data;
  GDestroyNotify destroy_func;
};

struct _GDataset
{
  gconstpointer location;
  GData        *datalist;
};


/* --- prototypes --- */
static inline GDataset*	g_dataset_lookup		(gconstpointer	  dataset_location);
static inline void	g_datalist_clear_i		(GData		**datalist);
static void		g_dataset_destroy_internal	(GDataset	 *dataset);
static inline void	g_data_set_internal		(GData     	**datalist,
							 GQuark   	  key_id,
							 gpointer         data,
							 GDestroyNotify   destroy_func,
							 GDataset	 *dataset);
static void		g_data_initialize		(void);
static inline GQuark	g_quark_new			(const gchar  	*string);


/* --- variables --- */
static GHashTable   *g_quark_ht = NULL;
static gchar       **g_quarks = NULL;
static GQuark        g_quark_seq_id = 0;
static GHashTable   *g_dataset_location_ht = NULL;
static GDataset     *g_dataset_cached = NULL;
static GMemChunk    *g_dataset_mem_chunk = NULL;
static GMemChunk    *g_data_mem_chunk = NULL;
static GData	    *g_data_cache = NULL;
static guint	     g_data_cache_length = 0;


/* --- functions --- */
static inline void
g_datalist_clear_i (GData **datalist)
{
  register GData *list;
  
  /* unlink *all* items before walking their destructors
   */
  list = *datalist;
  *datalist = NULL;
  
  while (list)
    {
      register GData *prev;
      
      prev = list;
      list = prev->next;
      
      if (prev->destroy_func)
	prev->destroy_func (prev->data);
      
      if (g_data_cache_length < G_DATA_CACHE_MAX)
	{
	  prev->next = g_data_cache;
	  g_data_cache = prev;
	  g_data_cache_length++;
	}
      else
	g_mem_chunk_free (g_data_mem_chunk, prev);
    }
}

void
g_datalist_clear (GData **datalist)
{
  g_return_if_fail (datalist != NULL);
  
  while (*datalist)
    g_datalist_clear_i (datalist);
}

static inline GDataset*
g_dataset_lookup (gconstpointer	dataset_location)
{
  register GDataset *dataset;
  
  if (g_dataset_cached && g_dataset_cached->location == dataset_location)
    return g_dataset_cached;
  
  dataset = g_hash_table_lookup (g_dataset_location_ht, dataset_location);
  if (dataset)
    g_dataset_cached = dataset;
  
  return dataset;
}

static void
g_dataset_destroy_internal (GDataset *dataset)
{
  register gconstpointer dataset_location;
  
  dataset_location = dataset->location;
  while (dataset)
    {
      if (!dataset->datalist)
	{
	  if (dataset == g_dataset_cached)
	    g_dataset_cached = NULL;
	  g_hash_table_remove (g_dataset_location_ht, dataset_location);
	  g_mem_chunk_free (g_dataset_mem_chunk, dataset);
	  break;
	}
      
      g_datalist_clear_i (&dataset->datalist);
      dataset = g_dataset_lookup (dataset_location);
    }
}

void
g_dataset_destroy (gconstpointer  dataset_location)
{
  g_return_if_fail (dataset_location != NULL);
  
  if (g_dataset_location_ht)
    {
      register GDataset *dataset;

      dataset = g_dataset_lookup (dataset_location);
      if (dataset)
	g_dataset_destroy_internal (dataset);
    }
}

static inline void
g_data_set_internal (GData	  **datalist,
		     GQuark         key_id,
		     gpointer       data,
		     GDestroyNotify destroy_func,
		     GDataset	   *dataset)
{
  register GData *list;
  
  if (!g_dataset_location_ht)
    g_data_initialize ();
  
  list = *datalist;
  if (!data)
    {
      register GData *prev;
      
      prev = NULL;
      while (list)
	{
	  if (list->id == key_id)
	    {
	      if (prev)
		prev->next = list->next;
	      else
		{
		  *datalist = list->next;
		  
		  /* the dataset destruction *must* be done
		   * prior to invokation of the data destroy function
		   */
		  if (!*datalist && dataset)
		    g_dataset_destroy_internal (dataset);
		}
	      
	      /* the GData struct *must* already be unlinked
	       * when invoking the destroy function
	       * we use (data==NULL && destroy_func!=NULL) as
	       * a special hint combination to "steal"
	       * data without destroy notification
	       */
	      if (list->destroy_func && !destroy_func)
		list->destroy_func (list->data);
	      
	      if (g_data_cache_length < G_DATA_CACHE_MAX)
		{
		  list->next = g_data_cache;
		  g_data_cache = list;
		  g_data_cache_length++;
		}
	      else
		g_mem_chunk_free (g_data_mem_chunk, list);
	      
	      return;
	    }
	  
	  prev = list;
	  list = list->next;
	}
    }
  else
    {
      while (list)
	{
	  if (list->id == key_id)
	    {
	      if (!list->destroy_func)
		{
		  list->data = data;
		  list->destroy_func = destroy_func;
		}
	      else
		{
		  register GDestroyNotify dfunc;
		  register gpointer ddata;
		  
		  dfunc = list->destroy_func;
		  ddata = list->data;
		  list->data = data;
		  list->destroy_func = destroy_func;
		  
		  /* we need to have updated all structures prior to
		   * invokation of the destroy function
		   */
		  dfunc (ddata);
		}
	      
	      return;
	    }
	  
	  list = list->next;
	}
      
      if (g_data_cache)
	{
	  list = g_data_cache;
	  g_data_cache = list->next;
	  g_data_cache_length--;
	}
      else
	list = g_chunk_new (GData, g_data_mem_chunk);
      list->next = *datalist;
      list->id = key_id;
      list->data = data;
      list->destroy_func = destroy_func;
      *datalist = list;
    }
}

void
g_dataset_id_set_data_full (gconstpointer  dataset_location,
			    GQuark         key_id,
			    gpointer       data,
			    GDestroyNotify destroy_func)
{
  register GDataset *dataset;
  
  g_return_if_fail (dataset_location != NULL);
  if (!data)
    g_return_if_fail (destroy_func == NULL);
  if (!key_id)
    {
      if (data)
	g_return_if_fail (key_id > 0);
      else
	return;
    }
  
  if (!g_dataset_location_ht)
    g_data_initialize ();
  
  dataset = g_dataset_lookup (dataset_location);
  if (!dataset)
    {
      dataset = g_chunk_new (GDataset, g_dataset_mem_chunk);
      dataset->location = dataset_location;
      g_datalist_init (&dataset->datalist);
      g_hash_table_insert (g_dataset_location_ht, 
			   (gpointer) dataset->location,
			   dataset);
    }
  
  g_data_set_internal (&dataset->datalist, key_id, data, destroy_func, dataset);
}

void
g_datalist_id_set_data_full (GData	  **datalist,
			     GQuark         key_id,
			     gpointer       data,
			     GDestroyNotify destroy_func)
{
  g_return_if_fail (datalist != NULL);
  if (!data)
    g_return_if_fail (destroy_func == NULL);
  if (!key_id)
    {
      if (data)
	g_return_if_fail (key_id > 0);
      else
	return;
    }
  
  g_data_set_internal (datalist, key_id, data, destroy_func, NULL);
}

void
g_dataset_id_remove_no_notify (gconstpointer  dataset_location,
			       GQuark         key_id)
{
  g_return_if_fail (dataset_location != NULL);
  
  if (key_id && g_dataset_location_ht)
    {
      GDataset *dataset;
  
      dataset = g_dataset_lookup (dataset_location);
      if (dataset)
	g_data_set_internal (&dataset->datalist, key_id, NULL, (GDestroyNotify) 42, dataset);
    }
}

void
g_datalist_id_remove_no_notify (GData	**datalist,
				GQuark    key_id)
{
  g_return_if_fail (datalist != NULL);

  if (key_id)
    g_data_set_internal (datalist, key_id, NULL, (GDestroyNotify) 42, NULL);
}

gpointer
g_dataset_id_get_data (gconstpointer  dataset_location,
		       GQuark         key_id)
{
  g_return_val_if_fail (dataset_location != NULL, NULL);
  
  if (key_id && g_dataset_location_ht)
    {
      register GDataset *dataset;
      
      dataset = g_dataset_lookup (dataset_location);
      if (dataset)
	{
	  register GData *list;
	  
	  for (list = dataset->datalist; list; list = list->next)
	    if (list->id == key_id)
	      return list->data;
	}
    }
  
  return NULL;
}

gpointer
g_datalist_id_get_data (GData	 **datalist,
			GQuark     key_id)
{
  g_return_val_if_fail (datalist != NULL, NULL);
  
  if (key_id)
    {
      register GData *list;
      
      for (list = *datalist; list; list = list->next)
	if (list->id == key_id)
	  return list->data;
    }
  
  return NULL;
}

void
g_dataset_foreach (gconstpointer    dataset_location,
		   GDataForeachFunc func,
		   gpointer         user_data)
{
  register GDataset *dataset;
  
  g_return_if_fail (dataset_location != NULL);
  g_return_if_fail (func != NULL);
  
  dataset = g_dataset_lookup (dataset_location);
  if (dataset)
    {
      register GData *list;
      
      for (list = dataset->datalist; list; list = list->next)
	func (list->id, list->data, user_data);
    }
}

void
g_datalist_foreach (GData	   **datalist,
		    GDataForeachFunc func,
		    gpointer         user_data)
{
  register GData *list;

  g_return_if_fail (datalist != NULL);
  g_return_if_fail (func != NULL);
  
  for (list = *datalist; list; list = list->next)
    func (list->id, list->data, user_data);
}

void
g_datalist_init (GData **datalist)
{
  g_return_if_fail (datalist != NULL);
  
  *datalist = NULL;
}

static void
g_data_initialize (void)
{
  if (!g_dataset_location_ht)
    {
      g_quark_ht = g_hash_table_new (g_str_hash, g_str_equal);
      g_dataset_location_ht = g_hash_table_new (g_direct_hash, NULL);
      g_dataset_cached = NULL;
      g_dataset_mem_chunk =
	g_mem_chunk_new ("GDataset MemChunk",
			 sizeof (GDataset),
			 sizeof (GDataset) * G_DATASET_MEM_CHUNK_PREALLOC,
			 G_ALLOC_AND_FREE);
      g_data_mem_chunk =
	g_mem_chunk_new ("GData MemChunk",
			 sizeof (GData),
			 sizeof (GData) * G_DATA_MEM_CHUNK_PREALLOC,
			 G_ALLOC_AND_FREE);
    }
}

GQuark
g_quark_try_string (const gchar *string)
{
  g_return_val_if_fail (string != NULL, 0);
  
  if (g_quark_ht)
    return (gulong) g_hash_table_lookup (g_quark_ht, string);
  else
    return 0;
}

GQuark
g_quark_from_string (const gchar *string)
{
  GQuark quark;
  
  g_return_val_if_fail (string != NULL, 0);
  
  if (!g_quark_ht)
    g_data_initialize ();
  
  quark = (gulong) g_hash_table_lookup (g_quark_ht, string);
  if (!quark)
    quark = g_quark_new (g_strdup (string));
  
  return quark;
}

GQuark
g_quark_from_static_string (const gchar *string)
{
  GQuark quark;
  
  g_return_val_if_fail (string != NULL, 0);
  
  if (!g_quark_ht)
    g_data_initialize ();
  
  quark = (gulong) g_hash_table_lookup (g_quark_ht, string);
  if (!quark)
    quark = g_quark_new (string);
  
  return quark;
}

gchar*
g_quark_to_string (GQuark quark)
{
  if (quark > 0 && quark <= g_quark_seq_id)
    return g_quarks[quark - 1];
  else
    return NULL;
}

static inline GQuark
g_quark_new (const gchar *string)
{
  GQuark quark;
  
  if (g_quark_seq_id % G_QUARK_BLOCK_SIZE == 0)
    g_quarks = g_realloc (g_quarks,
			  (g_quark_seq_id + G_QUARK_BLOCK_SIZE) * sizeof (gchar*));
  
  
  g_quarks[g_quark_seq_id] = (gchar*) string;
  g_quark_seq_id++;
  quark = g_quark_seq_id;
  g_hash_table_insert (g_quark_ht, (gchar*) string, GUINT_TO_POINTER (quark));
  
  return quark;
}
