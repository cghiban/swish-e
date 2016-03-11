/*
**
** $Id: hash.h 1736 2005-05-12 15:41:22Z karman $
**

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

unsigned string_hash(char *, int);
unsigned int_hash(int, int);
unsigned hash (char *);
unsigned numhash (int);
unsigned bighash (char *);
unsigned bignumhash (int);
unsigned verybighash (char *);

struct swline *add_word_to_hash_table( WORD_HASH_TABLE *table_ptr, char *word, int hash_size);
struct swline * is_word_in_hash_table( WORD_HASH_TABLE table, char *word);
void free_word_hash_table( WORD_HASH_TABLE *table_ptr);


