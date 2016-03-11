/* 

$Id: ramdisk.h 1946 2007-10-22 14:56:35Z karpet $

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

struct ramdisk *ramdisk_create(char *, int);

int ramdisk_close(FILE *);

void add_buffer_ramdisk(struct ramdisk *);

sw_off_t ramdisk_tell(FILE *);

size_t ramdisk_write(const void *,size_t, size_t, FILE *);

int ramdisk_seek(FILE *,sw_off_t, int );

size_t ramdisk_read(void *, size_t, size_t, FILE *);

int ramdisk_getc(FILE *);

int ramdisk_putc(int , FILE *);
