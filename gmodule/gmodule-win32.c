/* GMODULE - GLIB wrapper code for dynamic module loading
 * Copyright (C) 1998 Tim Janik
 * Copyright (C) 1998 Tor Lillqvist
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <stdio.h>
#include <windows.h>

/* --- functions --- */
static gpointer
_g_module_open (const gchar    *file_name,
		gboolean	bind_lazy)
{
  HINSTANCE handle;
  
  handle = LoadLibrary (file_name);
  if (!handle)
    {
      char error[100];
      sprintf (error, "Error code %d", GetLastError ());
      g_module_set_error (error);
    }
  
  return handle;
}

static gpointer
_g_module_self (void)
{
  HMODULE handle;
  
  handle = GetModuleHandle (NULL);
  if (!handle)
    {
      char error[100];
      sprintf (error, "Error code %d", GetLastError ());
      g_module_set_error (error);
    }
  
  return handle;
}

static void
_g_module_close (gpointer	  handle,
		 gboolean	  is_unref)
{
  if (!FreeLibrary (handle))
    {
      char error[100];
      sprintf (error, "Error code %d", GetLastError ());
      g_module_set_error (error);
    }
}

static gpointer
_g_module_symbol (gpointer	  handle,
		  const gchar	 *symbol_name)
{
  gpointer p;
  
  p = GetProcAddress (handle, symbol_name);
  if (!p)
    {
      char error[100];
      sprintf (error, "Error code %d", GetLastError ());
      g_module_set_error (error);
    }
  return p;
}

static gchar*
_g_module_build_path (const gchar *directory,
		      const gchar *module_name)
{
  if (directory)
    return g_strconcat (directory, "\\", module_name, ".dll", NULL);
  else
    return g_strconcat (module_name, ".dll", NULL);
}