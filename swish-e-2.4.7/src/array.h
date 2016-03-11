/* 
$Id: array.h 1946 2007-10-22 14:56:35Z karpet $

    This file is part of Swish-e.

    Swish-e is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Swish-e is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along  with Swish-e; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
    
    See the COPYING file that accompanies the Swish-e distribution for details
    of the GNU GPL and the special exception available for linking against
    the Swish-e library.
    
** Mon May  9 18:19:34 CDT 2005
** added GPL

*/


typedef struct ARRAY_Page
{
    sw_off_t           next;    /* Next Page */

    sw_off_t           page_number;
    int                modified;
    int                in_use;

    struct ARRAY_Page  *next_cache;

    unsigned char data[0];        /* Page data */
} ARRAY_Page;

#define ARRAY_CACHE_SIZE 97

typedef struct ARRAY
{
    sw_off_t root_page;
    int page_size;
    struct ARRAY_Page *cache[ARRAY_CACHE_SIZE];
    int levels;

    FILE *fp;
} ARRAY;

ARRAY *ARRAY_Create(FILE *fp);
ARRAY *ARRAY_Open(FILE *fp, sw_off_t root_page);
sw_off_t ARRAY_Close(ARRAY *bt);
int ARRAY_Put(ARRAY *b, int index, unsigned long value);
unsigned long ARRAY_Get(ARRAY *b, int index);
