/* Program to convert an image file to inline C data
 *
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * Developed by Havoc Pennington <hp@redhat.com>
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

#include "config.h"
#include "gdk-pixbuf-private.h"
#include <glib/gstdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

void
output_int (FILE *outfile, guint32 num, const char *comment)
{
  guchar bytes[4];

  /* Note, most significant bytes first */
  bytes[0] = num >> 24;
  bytes[1] = num >> 16;
  bytes[2] = num >> 8;
  bytes[3] = num;

  g_fprintf (outfile, "  /* %s (%u) */\n  0x%.2x, 0x%.2x, 0x%.2x, 0x%.2x,\n",
          comment, num,
          bytes[0], bytes[1], bytes[2], bytes[3]);  
}

void
output_bool (FILE *outfile, gboolean val, const char *comment)
{
  g_fprintf (outfile, "  /* %s (%s) */\n  0x%.2x,\n",
          comment,
          val ? "TRUE" : "FALSE",
          val ? 1 : 0);
}

void
output_pixbuf (FILE *outfile, gboolean ext_symbols,
               const char *varname,
               GdkPixbuf *pixbuf)
{
  const char *modifier;
  const guchar *p;
  const guchar *end;
  gboolean has_alpha;
  int column;
  
  modifier = "static ";
  if (ext_symbols)
    modifier = "";
  
  g_fprintf (outfile, "%sconst guchar ", modifier);
  fputs (varname, outfile);
  fputs ("[] =\n", outfile);
  fputs ("{\n", outfile);

  p = gdk_pixbuf_get_pixels (pixbuf);
  end = p + gdk_pixbuf_get_rowstride (pixbuf) * gdk_pixbuf_get_height (pixbuf);
  has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

  /* Sync the order of writing with the order of reading in
   * gdk-pixbuf-data.c
   */
  output_int (outfile, GDK_PIXBUF_INLINE_MAGIC_NUMBER, "File magic");
  output_int (outfile, GDK_PIXBUF_INLINE_RAW, "Format of following stuff");
  output_int (outfile, gdk_pixbuf_get_rowstride (pixbuf), "Rowstride");
  output_int (outfile, gdk_pixbuf_get_width (pixbuf), "Width");
  output_int (outfile, gdk_pixbuf_get_height (pixbuf), "Height");

  output_bool (outfile, has_alpha, "Has an alpha channel");

  output_int (outfile, gdk_pixbuf_get_colorspace (pixbuf), "Colorspace (0 == RGB, no other options implemented)");

  output_int (outfile, gdk_pixbuf_get_n_channels (pixbuf), "Number of channels");

  output_int (outfile, gdk_pixbuf_get_bits_per_sample (pixbuf), "Bits per sample");

  fputs ("  /* Image data */\n", outfile);
  
  /* Copy the data in the pixbuf */
  column = 0;
  while (p != end)
    {
      guchar r, g, b, a;
      
      r = *p;
      ++p;
      g = *p;
      ++p;
      b = *p;
      ++p;
      if (has_alpha)
        {
          a = *p;
          ++p;
        }
      else
        a = 0;

      
      if (has_alpha)
        g_fprintf (outfile, "  0x%.2x, 0x%.2x, 0x%.2x, 0x%.2x", r, g, b, a);
      else
        g_fprintf (outfile, "  0x%.2x, 0x%.2x, 0x%.2x", r, g, b);

      if (p != end)
        fputs (",", outfile);
      else
        fputs ("\n", outfile);
      
      ++column;
      
      if (column > 2)
        {
          fputs ("\n", outfile);
          column = 0;
        }
    }

  fputs ("};\n\n", outfile);
}

void
usage (void)
{
  g_fprintf (stderr, "Usage: make-inline-pixbuf [--extern-symbols] OUTFILE varname1 imagefile1 varname2 imagefile2 ...\n");
  exit (1);
}

int
main (int argc, char **argv)
{
  gboolean ext_symbols = FALSE;
  FILE *outfile;
  gchar *outfilename;
  int i;
  
  g_type_init ();
  
  
  if (argc < 4)
    usage ();

  i = 1;
  if (strcmp (argv[i], "--extern-symbols") == 0)
    {
      ext_symbols = TRUE;
      ++i;
      if (argc < 5)
        usage ();
    }

#ifdef G_OS_WIN32
  outfilename = g_locale_to_utf8 (argv[i], -1, NULL, NULL, NULL)
#else
  outfilename = argv[i];
#endif

  outfile = g_fopen (outfilename, "w");
  if (outfile == NULL)
    {
      g_fprintf (stderr, "Failed to open output file `%s': %s\n",
		 argv[i], strerror (errno));
      exit (1);
    }

  ++i;

  fputs ("/* This file was automatically generated by the make-inline-pixbuf program.\n"
         " * It contains inline RGB image data.\n"
         " */\n\n", outfile);

  /* Check for matched pairs of images/names */
  if (((argc - i) % 2) != 0)
    usage ();
  
  while (i < argc)
    {
      GdkPixbuf *pixbuf = NULL;
      GError *error;
      gchar *infilename;
      
      g_assert ((i + 1) < argc);

      error = NULL;
#ifdef G_OS_WIN32
      infilename = g_locale_to_utf8 (argv[i+1], -1, NULL, NULL, &error);
#else
      infilename = argv[i+1];
#endif
      if (infilename)
	pixbuf = gdk_pixbuf_new_from_file (infilename, &error);

      if (pixbuf == NULL)
        {
          g_fprintf (stderr, "%s\n", error->message);
          fclose (outfile);
          g_remove (outfilename);
          exit (1);
        }

      output_pixbuf (outfile, ext_symbols, argv[i], pixbuf);
      
      g_object_unref (pixbuf);
      
      i += 2;
    }
  
  fclose (outfile);

  return 0;
}
