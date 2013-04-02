/*
 * getline.c --- replacement for GNU getline if not available in libc
 *
 * Copyright  (C)  2011  Thomas Jahns <jahns@dkrz.de>
 *
 * Version: 0.0.1
 * Author: Thomas Jahns <jahns@dkrz.de>
 * Maintainer: Thomas Jahns <jahns@dkrz.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>

/* read until end of line or end of file */
ssize_t
getline(char **linebuf, size_t *linebuf_size, FILE *fp)
{
  size_t len, old_len, buf_size;
  char *buf;
  ssize_t status = 0;

  if ( feof(fp) ) return -1;

  if (!linebuf_size || !linebuf)
  {
    errno = EINVAL;
    return -1;
  }
  if (!*linebuf)
  {
    buf_size = 80;
    buf = malloc(buf_size);
    if (!buf) return -1;
  }
  else
  {
    buf_size = *linebuf_size;
    buf = *linebuf;
  }
  if ((status = (fgets(buf, buf_size, fp) != NULL)))
  {
    len = strlen(buf);
    if (buf[len-1] != '\n')
    {
      do {
        void *temp = realloc(buf, buf_size * 2);
        if (!temp)
        {
          status = -1;
          break;
        }
        buf = temp;
        buf_size *= 2;
        old_len = len;
      } while(fgets(buf + len, buf_size - len, fp)
              && (len += strlen(buf + len)) > old_len
              && buf[len-1] != '\n');
    }
    status = len > SSIZE_MAX ? SSIZE_MAX : len;
  }
  if (linebuf_size)
    *linebuf_size = buf_size;
  *linebuf = buf;
  return status;
}

