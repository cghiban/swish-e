/*

$Id: db_write.c 1945 2007-10-22 14:54:07Z karpet $

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
    
** Mon May  9 15:51:39 CDT 2005
** added GPL

**
** 2001-05-07 jmruiz init coding
**
*/

#include "swish.h"
#include "mem.h"
#include "swstring.h"
#include "index.h"
#include "hash.h"
#include "date_time.h"
#include "compress.h"
#include "error.h"
#include "metanames.h"
#include "db.h"
#include "db_native.h"
// #include "db_berkeley_db.h"



#ifndef min
#define min(a, b)    (a) < (b) ? a : b
#endif


/* General write DB routines - Common to all DB */



static int    write_hash_words_to_header(SWISH *sw, int header_ID, struct swline **hash, void *DB);




/* Header routines */

#define write_header_int(sw,id,num,DB) {unsigned long itmp = (num); itmp = PACKLONG(itmp); DB_WriteHeaderData((sw),(id), (unsigned char *)&itmp, sizeof(long), (DB));}
#define write_header_int2(sw,id,num1,num2,DB)\
{  \
        unsigned long itmp[2]; \
        itmp[0] = (num1); \
        itmp[1] = (num2); \
        itmp[0] = PACKLONG(itmp[0]); \
        itmp[1] = PACKLONG(itmp[1]); \
        DB_WriteHeaderData((sw),(id), (unsigned char *)itmp, sizeof(long) * 2, (DB)); \
}
#define write_header_int4(sw,id,num1,num2,num3,num4,DB)\
{  \
        unsigned long itmp[4]; \
        itmp[0] = (num1); \
        itmp[1] = (num2); \
        itmp[2] = (num3); \
        itmp[3] = (num4); \
        itmp[0] = PACKLONG(itmp[0]); \
        itmp[1] = PACKLONG(itmp[1]); \
        itmp[2] = PACKLONG(itmp[2]); \
        itmp[3] = PACKLONG(itmp[3]); \
        DB_WriteHeaderData((sw),(id), (unsigned char *)itmp, sizeof(long) * 4, (DB)); \
}

void    write_header(SWISH *sw, int merged_flag )
{
    IndexFILE       *indexf     = sw->indexlist;       /* first element in the list */
    INDEXDATAHEADER *header     = &indexf->header;
    char            *filename   = indexf->line;
    void            *DB         = indexf->DB;
    char            *c;
    char            *tmp;


    /* $$$ this isn't portable */
    c = (char *) strrchr(filename, '/');
    if (!c || (c && !*(c + 1)))
        c = filename;
    else
        c += 1;

    DB_InitWriteHeader(sw, DB);

    DB_WriteHeaderData(sw, INDEXHEADER_ID, (unsigned char *)INDEXHEADER, strlen(INDEXHEADER) +1, DB);
    DB_WriteHeaderData(sw, INDEXVERSION_ID, (unsigned char *)INDEXVERSION, strlen(INDEXVERSION) + 1, DB);
    write_header_int(sw, MERGED_ID, merged_flag, DB);
    DB_WriteHeaderData(sw, NAMEHEADER_ID, (unsigned char *)header->indexn, strlen(header->indexn) + 1, DB);
    DB_WriteHeaderData(sw, SAVEDASHEADER_ID, (unsigned char *)c, strlen(c) + 1, DB);

    write_header_int4(sw, COUNTSHEADER_ID,
            header->totalwords,
            header->totalfiles,
            header->total_word_positions + indexf->total_word_positions_cur_run, /* Total this run's total words (not unique) with any previous */
            header->removedfiles, DB);

    tmp = getTheDateISO();
    DB_WriteHeaderData(sw, INDEXEDONHEADER_ID, (unsigned char *)tmp, strlen(tmp) + 1,DB);
    efree(tmp);
    DB_WriteHeaderData(sw, DESCRIPTIONHEADER_ID, (unsigned char *)header->indexd, strlen(header->indexd) + 1, DB);
    DB_WriteHeaderData(sw, POINTERHEADER_ID, (unsigned char *)header->indexp, strlen(header->indexp) + 1, DB);
    DB_WriteHeaderData(sw, MAINTAINEDBYHEADER_ID, (unsigned char *)header->indexa, strlen(header->indexa) + 1,DB);
    write_header_int(sw, DOCPROPENHEADER_ID, 1, DB);

    write_header_int(sw, FUZZYMODEHEADER_ID, (int)fuzzy_mode_value(header->fuzzy_data), DB);

    write_header_int(sw, IGNORETOTALWORDCOUNTWHENRANKING_ID, header->ignoreTotalWordCountWhenRanking, DB);
    DB_WriteHeaderData(sw, WORDCHARSHEADER_ID, (unsigned char *)header->wordchars, strlen(header->wordchars) + 1, DB);
    write_header_int(sw, MINWORDLIMHEADER_ID, header->minwordlimit, DB);
    write_header_int(sw, MAXWORDLIMHEADER_ID, header->maxwordlimit, DB);
    DB_WriteHeaderData(sw, BEGINCHARSHEADER_ID, (unsigned char *)header->beginchars, strlen(header->beginchars) + 1, DB);
    DB_WriteHeaderData(sw, ENDCHARSHEADER_ID, (unsigned char *)header->endchars, strlen(header->endchars) + 1, DB);
    DB_WriteHeaderData(sw, IGNOREFIRSTCHARHEADER_ID, (unsigned char *)header->ignorefirstchar, strlen(header->ignorefirstchar) + 1, DB);
    DB_WriteHeaderData(sw, IGNORELASTCHARHEADER_ID, (unsigned char *)header->ignorelastchar, strlen(header->ignorelastchar) + 1,DB);
    /* Removed - Patents
    write_header_int(FILEINFOCOMPRESSION_ID, header->applyFileInfoCompression, DB);
    */



    /* Jose Ruiz 06/00 Added this line to delimite the header */
    write_integer_table_to_header(sw, TRANSLATECHARTABLE_ID, header->translatecharslookuptable, sizeof(header->translatecharslookuptable) / sizeof(int), DB);

    /* Other header stuff */

    /* StopWords */
    write_hash_words_to_header(sw, STOPWORDS_ID, header->hashstoplist.hash_array, DB);


    /* Metanames */
    write_MetaNames(sw, METANAMES_ID, header, DB);

    /* BuzzWords */
    write_hash_words_to_header(sw, BUZZWORDS_ID, header->hashbuzzwordlist.hash_array, DB);

#ifndef USE_BTREE
    /* Write the total words per file array, if used */
    if ( !header->ignoreTotalWordCountWhenRanking )
        write_integer_table_to_header(sw, TOTALWORDSPERFILE_ID, header->TotalWordsPerFile, header->totalfiles, DB);
#endif

    write_header_int(sw, TOTALWORDS_REMOVED_ID, header->removed_word_positions, DB);

    DB_EndWriteHeader(sw, DB);
}


/* Jose Ruiz 11/00
** Function to write a word to the index DB
*/
void    write_word(SWISH * sw, ENTRY * ep, IndexFILE * indexf)
{
    sw_off_t    wordID;

    wordID = DB_GetWordID(sw, indexf->DB);

    DB_WriteWord(sw, ep->word,wordID,indexf->DB);
        /* Store word offset for futher hash computing */
    ep->u1.wordID = wordID;

}

#ifdef USE_BTREE
/* 04/2002 jmruiz
** Routine to update wordID
*/
void    update_wordID(SWISH * sw, ENTRY * ep, IndexFILE * indexf)
{
    sw_off_t    wordID;

    wordID = DB_GetWordID(sw, indexf->DB);

    DB_UpdateWordID(sw, ep->word,wordID,indexf->DB);
        /* Store word offset for futher hash computing */
    ep->u1.wordID = wordID;
}

void    delete_worddata(SWISH * sw, sw_off_t wordID, IndexFILE * indexf)
{
    DB_DeleteWordData(sw,wordID,indexf->DB);
}

#endif

/* Jose Ruiz 11/00
** Function to write all word's data to the index DB
*/

void build_worddata(SWISH * sw, ENTRY * ep)
{
    int     curmetaID,
            sz_worddata;
    unsigned long    tmp,
            curmetanamepos;
    int     metaID;
    int     chunk_size;
    unsigned char *compressed_data,
           *p,*q;
    LOCATION *l, *next;


    curmetaID=0;
    curmetanamepos=0L;
    q=sw->Index->worddata_buffer;

    /* Write tfrequency */
    q = compress3(ep->tfrequency,q);

    /* Write location list */
    for(l=ep->allLocationList;l;)
    {
        compressed_data = (unsigned char *) l;

        /* Get next element */
        next = *(LOCATION **)compressed_data;

        /* Jump pointer to next element */
        p = compressed_data + sizeof(LOCATION *);

        metaID = uncompress2(&p);

        memcpy((char *)&chunk_size,(char *)p,sizeof(chunk_size));
        p += sizeof(chunk_size);

        if(curmetaID!=metaID)
        {
            if(curmetaID)
            {
                /* Write in previous meta (curmetaID)
                ** file offset to next meta */
                tmp=q - sw->Index->worddata_buffer;
                PACKLONG2(tmp,sw->Index->worddata_buffer+curmetanamepos);
            }
                /* Check for enough memory */
                /*
                ** MAXINTCOMPSIZE is for the worst case metaID
                **
                ** sizeof(long) is to leave four bytes to
                ** store the offset of the next metaname
                ** (it will be 0 if no more metanames).
                **
                ** 1 is for the trailing '\0'
                */

            tmp=q - sw->Index->worddata_buffer;

            if((long)(tmp + MAXINTCOMPSIZE + sizeof(long) + 1) >= (long)sw->Index->len_worddata_buffer)
            {
                sw->Index->len_worddata_buffer=sw->Index->len_worddata_buffer*2+MAXINTCOMPSIZE+sizeof(long)+1;
                sw->Index->worddata_buffer=(unsigned char *) erealloc(sw->Index->worddata_buffer,sw->Index->len_worddata_buffer);
                q=sw->Index->worddata_buffer+tmp;   /* reasign pointer inside buffer */
            }

            /* store metaID in buffer */
            curmetaID=metaID;
            q = compress3(curmetaID,q);


            /* preserve position for offset to next
            ** metaname. We do not know its size
            ** so store it as a packed long */
            curmetanamepos=q - sw->Index->worddata_buffer;

            /* Store 0 and increase pointer */
            tmp=0L;
            PACKLONG2(tmp,q);
            q+=sizeof(unsigned long);

        }


        /* Store all data for this chunk */
        /* First check for enough space
        **
        ** 1 is for the trailing '\0'
        */

        tmp=q - sw->Index->worddata_buffer;

        if((long)(tmp + chunk_size + 1) >= (long)sw->Index->len_worddata_buffer)
        {
            sw->Index->len_worddata_buffer=sw->Index->len_worddata_buffer*2+chunk_size+1;
            sw->Index->worddata_buffer=(unsigned char *) erealloc(sw->Index->worddata_buffer,sw->Index->len_worddata_buffer);
            q=sw->Index->worddata_buffer+tmp;   /* reasign pointer inside buffer */
        }

        /* Copy it and advance pointer */
        memcpy(q,p,chunk_size);
        q += chunk_size;

        /* End of chunk mark -> Write trailing '\0' */
        *q++ = '\0';

        l = next;
    }

    /* Write in previous meta (curmetaID)
    ** file offset to end of metas */
    tmp=q - sw->Index->worddata_buffer;
    PACKLONG2(tmp,sw->Index->worddata_buffer+curmetanamepos);

    sz_worddata = q - sw->Index->worddata_buffer;

    /* Adjust word positions.
    ** if ignorelimit was set and some new stopwords weee found, positions
    ** are recalculated
    ** Also call it even if we have not set IgnoreLimit to calesce word chunks
    ** and remove trailing 0 from chunks to save some bytes
    */
    adjustWordPositions(sw->Index->worddata_buffer, &sz_worddata, sw->indexlist->header.totalfiles, sw->Index->IgnoreLimitPositionsArray);

    sw->Index->sz_worddata_buffer = sz_worddata;
}


/* 04/2002 jmruiz
** New simpler routine to write worddata
**
** 10/2002 jmruiz
** Add extra compression for worddata. Call to remove_worddata_longs
*/
void write_worddata(SWISH * sw, ENTRY * ep, IndexFILE * indexf )
{
    int zlib_size;

    /* Get some extra compression */
    remove_worddata_longs(sw->Index->worddata_buffer,&sw->Index->sz_worddata_buffer);

    if(sw->compressPositions)
        zlib_size = compress_worddata(sw->Index->worddata_buffer, sw->Index->sz_worddata_buffer,sw->Index->swap_locdata);
    else
        zlib_size = sw->Index->sz_worddata_buffer;

    /* Write worddata */
    DB_WriteWordData(sw, ep->u1.wordID,sw->Index->worddata_buffer,zlib_size, sw->Index->sz_worddata_buffer - zlib_size ,indexf->DB);

}



/* 04/2002 jmruiz
** Routine to merge two buffers of worddata
*/
void add_worddata(SWISH *sw, unsigned char *olddata, int sz_olddata)
{
int maxtotsize;
unsigned char stack_buffer[32000];  /* Just to try malloc/free fragmentation */
unsigned char *newdata;
int sz_newdata;
int tfreq1, tfreq2;
unsigned char *p1, *p2, *p;
int curmetaID_1,curmetaID_2,metadata_length_1,num_metaids1;
unsigned long nextposmetaname_1,nextposmetaname_2, curmetanamepos, curmetanamepos_1, curmetanamepos_2, tmp;
int last_filenum, filenum, tmpval, frequency;
unsigned int *posdata;
#define POSDATA_STACK 2000
unsigned int stack_posdata[POSDATA_STACK];  /* Just to avoid the overhead of malloc/free */
unsigned char r_flag, *w_flag;
unsigned char *q;

    /* First of all, ckeck for size in buffer */
    /* Olddata is extra compressed. longs offsets where stored as compressed
    ** numbers to save space. So, we need to compute how many meta_ID
    ** are presents to calculate a safe size for olddata with packedlongs */
    p1=olddata;
    num_metaids1=0;
    uncompress2(&p1);   /* Jump tfreq */
    do
    {
        num_metaids1++;
        uncompress2(&p1);   /* Jump metaid */
        metadata_length_1 = uncompress2(&p1);
        p1 += metadata_length_1;
    } while ((p1 - olddata) != sz_olddata);
    maxtotsize = sw->Index->sz_worddata_buffer + (sz_olddata + num_metaids1 * sizeof(long));

    if(maxtotsize > sw->Index->len_worddata_buffer)
    {
         sw->Index->len_worddata_buffer = maxtotsize + 2000;
         sw->Index->worddata_buffer = (unsigned char *) erealloc(sw->Index->worddata_buffer,sw->Index->len_worddata_buffer);
    }
    /* Preserve new data in a local copy - sw->Index->worddata_buffer is the final destination
    ** of data
    */
    if(sw->Index->sz_worddata_buffer > (int)sizeof(stack_buffer))
        newdata = (unsigned char *) emalloc(sw->Index->sz_worddata_buffer);
    else
        newdata = stack_buffer;
    sz_newdata = sw->Index->sz_worddata_buffer;
    memcpy(newdata,sw->Index->worddata_buffer, sz_newdata);

    /* Set pointers to all buffers */
    p1 = olddata;
    p2 = newdata;
    q = p = sw->Index->worddata_buffer;

    /* Now read tfrequency */
    tfreq1 = uncompress2(&p1); /* tfrequency - number of files with this word */
    tfreq2 = uncompress2(&p2); /* tfrequency - number of files with this word */
    /* Write tfrequency */
    p = compress3(tfreq1 + tfreq2, p);

    /* Now look for MetaIDs */
    curmetaID_1 = uncompress2(&p1);
    curmetaID_2 = uncompress2(&p2);

    /* Old data is compressed in a different more optimized schema */
    metadata_length_1 = uncompress2(&p1);
    nextposmetaname_1 = p1 - olddata + metadata_length_1;

    curmetanamepos_1 = p1 - olddata;
    nextposmetaname_2 = UNPACKLONG2(p2);
    p2 += sizeof(long);

    curmetanamepos_2 = p2 - newdata;

    while(curmetaID_1 && curmetaID_2)
    {
        p = compress3(min(curmetaID_1,curmetaID_2),p);

        curmetanamepos = p - sw->Index->worddata_buffer;

        /* Store 0 and increase pointer */
        tmp=0L;
        PACKLONG2(tmp,p);
        p+=sizeof(unsigned long);

        if(curmetaID_1 == curmetaID_2)
        {
            /* Both buffers have the same metaID - In this case I have to know
            the number of the filenum of the last hit of the original buffer to adjust the
            filenum counter in the second buffer */
            last_filenum = 0;
            do
            {
                /* Read on all items */
                uncompress_location_values(&p1,&r_flag,&tmpval,&frequency);
                last_filenum += tmpval;

                if(frequency > POSDATA_STACK)
                    posdata = (unsigned int *) emalloc(frequency * sizeof(int));
                else
                    posdata = stack_posdata;

                /* Read and discard positions just to advance pointer */
                uncompress_location_positions(&p1,r_flag,frequency,posdata);
                if(posdata!=stack_posdata)
                    efree(posdata);

                if ((p1 - olddata) == sz_olddata)
                {
                    curmetaID_1 = 0;   /* No more metaIDs for olddata */
                    break;   /* End of olddata */
                }

                if ((unsigned long)(p1 - olddata) == nextposmetaname_1)
                {
                    break;
                }
            } while(1);
            memcpy(p,olddata + curmetanamepos_1, p1 - (olddata + curmetanamepos_1));
            p += p1 - (olddata + curmetanamepos_1);
            /* Values for next metaID if exists */
            if(curmetaID_1)
            {
                curmetaID_1 = uncompress2(&p1);  /* Next metaID */
                metadata_length_1 = uncompress2(&p1);
                nextposmetaname_1 = p1 - olddata + metadata_length_1;
                curmetanamepos_1 = p1 - olddata;
            }

            /* Now add the new values adjusting with last_filenum just the first
            ** filenum in olddata*/
            /* Read first item */
            uncompress_location_values(&p2,&r_flag,&tmpval,&frequency);
            filenum = tmpval;  /* First filenum in chunk */
            if(frequency > POSDATA_STACK)
                posdata = (unsigned int *) emalloc(frequency * sizeof(int));
            else
                posdata = stack_posdata;

            /* Read positions */
            uncompress_location_positions(&p2,r_flag,frequency,posdata);

            compress_location_values(&p,&w_flag,filenum - last_filenum,frequency,posdata);
            compress_location_positions(&p,w_flag,frequency,posdata);

            if(posdata!=stack_posdata)
                efree(posdata);

            /* Copy rest of data */
            memcpy(p,p2,nextposmetaname_2 - (p2 - newdata));
            p += nextposmetaname_2 - (p2 - newdata);
            p2 += nextposmetaname_2 - (p2 - newdata);

            if ((p2 - newdata) == sz_newdata)
            {
                curmetaID_2 = 0;   /* No more metaIDs for newdata */
            }
            /* Values for next metaID if exists */
            if(curmetaID_2)
            {
                curmetaID_2 = uncompress2(&p2);  /* Next metaID */
                nextposmetaname_2 = UNPACKLONG2(p2);
                p2 += sizeof(long);
                curmetanamepos_2 = p2 - newdata;
            }
        }
        else if (curmetaID_1 < curmetaID_2)
        {
            memcpy(p,p1,nextposmetaname_1 - (p1 - olddata));
            p += nextposmetaname_1 - (p1 - olddata);
            p1 = olddata + nextposmetaname_1;
            if ((p1 - olddata) == sz_olddata)
            {
                curmetaID_1 = 0;   /* No more metaIDs for newdata */
            }
            else
            {
                curmetaID_1 = uncompress2(&p1);  /* Next metaID */
                metadata_length_1 = uncompress2(&p1);
                nextposmetaname_1 = p1 - olddata + metadata_length_1;
                curmetanamepos_1 = p1 - olddata;
            }
        }
        else  /* curmetaID_1 > curmetaID_2 */
        {
            memcpy(p,p2,nextposmetaname_2 - (p2 - newdata));
            p += nextposmetaname_2 - (p2 - newdata);
            p2 = newdata + nextposmetaname_2;
            if ((p2 - newdata) == sz_newdata)
            {
                curmetaID_2 = 0;   /* No more metaIDs for newdata */
            }
            else
            {
                curmetaID_2 = uncompress2(&p2);  /* Next metaID */
                nextposmetaname_2 = UNPACKLONG2(p2);
                p2 += sizeof(long);
                curmetanamepos_2 = p2 - newdata;
            }
        }
        /* Put nextmetaname offset */
        PACKLONG2(p - sw->Index->worddata_buffer, sw->Index->worddata_buffer + curmetanamepos);

    } /* while */

    /* Add the rest of the data if exists */
    while(curmetaID_1)
    {
        p = compress3(curmetaID_1,p);

        curmetanamepos = p - sw->Index->worddata_buffer;
                /* Store 0 and increase pointer */
        tmp=0L;
        PACKLONG2(tmp,p);
        p += sizeof(unsigned long);

        memcpy(p,p1,nextposmetaname_1 - (p1 - olddata));
        p += nextposmetaname_1 - (p1 - olddata);
        p1 = olddata + nextposmetaname_1;
        if ((p1 - olddata) == sz_olddata)
        {
            curmetaID_1 = 0;   /* No more metaIDs for olddata */
        }
        else
        {
            curmetaID_1 = uncompress2(&p1);  /* Next metaID */
            metadata_length_1 = uncompress2(&p1);
            nextposmetaname_1 = p1 - olddata + metadata_length_1;
            curmetanamepos_1 = p1 - olddata;
        }
        PACKLONG2(p - sw->Index->worddata_buffer, sw->Index->worddata_buffer + curmetanamepos);
    }


    while(curmetaID_2)
    {
        p = compress3(curmetaID_2,p);

        curmetanamepos = p - sw->Index->worddata_buffer;
            /* Store 0 and increase pointer */
        tmp=0L;
        PACKLONG2(tmp,p);
        p += sizeof(unsigned long);

        memcpy(p,p2,nextposmetaname_2 - (p2 - newdata));
        p += nextposmetaname_2 - (p2 - newdata);
        p2 = newdata + nextposmetaname_2;
        if ((p2 - newdata) == sz_newdata)
        {
            curmetaID_2 = 0;   /* No more metaIDs for olddata */
        }
        else
        {
            curmetaID_2 = uncompress2(&p2);  /* Next metaID */
            nextposmetaname_2 = UNPACKLONG2(p2);
            p2+= sizeof(long);
            curmetanamepos_2= p2 - newdata;
        }
        PACKLONG2(p - sw->Index->worddata_buffer, sw->Index->worddata_buffer + curmetanamepos);
    }


    if(newdata != stack_buffer)
        efree(newdata);

    /* Save the new size */
    sw->Index->sz_worddata_buffer = p - sw->Index->worddata_buffer;
}

/* Writes the list of metaNames into the DB index
 *  (should maybe be in metanames.c)
*/

void    write_MetaNames(SWISH *sw, int id, INDEXDATAHEADER * header, void *DB)
{
    struct metaEntry *entry = NULL;
    int     i,
            sz_buffer,
            len;
    unsigned char *buffer,*s;
    int     fields;

    /* Use new metaType schema - see metanames.h */
    // Format of metaname is
    //   <len><metaName><metaType><Alias><rank_bias>
    //   len, metaType, alias, and rank_bias are compressed numbers
    //   metaName is the ascii name of the metaname
    //
    // The list of metanames is delimited by a 0

    fields = 5;  // len, metaID, metaType, alias, rank_bias


    /* Compute buffer size */
    for (sz_buffer = 0 , i = 0; i < header->metaCounter; i++)
    {
        entry = header->metaEntryArray[i];
        len = strlen(entry->metaName);
        sz_buffer += len + fields * MAXINTCOMPSIZE; /* compress can use MAXINTCOMPSIZE bytes in worse case,  */
    }

    sz_buffer += MAXINTCOMPSIZE;  /* Add extra MAXINTCOMPSIZE for the number of metanames */

    s = buffer = (unsigned char *) emalloc(sz_buffer);

    s = compress3(header->metaCounter,s);   /* store the number of metanames */

    for (i = 0; i < header->metaCounter; i++)
    {
        entry = header->metaEntryArray[i];
        len = strlen(entry->metaName);
        s = compress3(len, s);
        memcpy(s,entry->metaName,len);
        s += len;
        s = compress3(entry->metaID, s);
        s = compress3(entry->metaType, s);
        s = compress3(entry->alias+1, s);  /* keep zeros away from compress3, I believe */
        s = compress3(entry->sort_len, s);
        s = compress3(entry->rank_bias+RANK_BIAS_RANGE+1, s);
    }
    DB_WriteHeaderData(sw, id,buffer,s-buffer,DB);
    efree(buffer);
}



/* Write a the hashlist of words into the index header file (used by stopwords and buzzwords
*/

static int    write_hash_words_to_header(SWISH *sw, int header_ID, struct swline **hash, void *DB)
{
    int     hashval,
            len,
            num_words,
            sz_buffer;
    char   *buffer, *s;
    struct swline *sp = NULL;

    /* Let's count the words */

    if ( !hash )
        return 0;

    for (sz_buffer = 0, num_words = 0 , hashval = 0; hashval < HASHSIZE; hashval++)
    {
        sp = hash[hashval];
        while (sp != NULL)
        {
            num_words++;
            sz_buffer += MAXINTCOMPSIZE + strlen(sp->line);
            sp = sp->next;
        }
    }

    if(num_words)
    {
        sz_buffer += MAXINTCOMPSIZE;  /* Add MAXINTCOMPSIZE for the number of words */

        s = buffer = (char *)emalloc(sz_buffer);

        s = (char *)compress3(num_words, (unsigned char *)s);

        for (hashval = 0; hashval < HASHSIZE; hashval++)
        {
            sp = hash[hashval];
            while (sp != NULL)
            {
                len = strlen(sp->line);
                s = (char *)compress3(len,(unsigned char *)s);
                memcpy(s,sp->line,len);
                s +=len;
                sp = sp->next;
            }
        }
        DB_WriteHeaderData(sw, header_ID, (unsigned char *)buffer, s - buffer, DB);
        efree(buffer);
    }
    return 0;
}



int write_integer_table_to_header(SWISH *sw, int id, int table[], int table_size, void *DB)
{
    int     i,
            tmp;
    char   *s;
    char   *buffer;

    s = buffer = (char *) emalloc((table_size + 1) * MAXINTCOMPSIZE);

    s = (char *)compress3(table_size,(unsigned char *)s);   /* Put the number of elements */
    for (i = 0; i < table_size; i++)
    {
        tmp = table[i] + 1;
        s = (char *)compress3(tmp, (unsigned char *)s); /* Put all the elements */
    }

    DB_WriteHeaderData(sw, id, (unsigned char *)buffer, s-buffer, DB);

    efree(buffer);
    return 0;
}





void setTotalWordsPerFile(IndexFILE *indexf, int idx,int wordcount)
{
#ifdef USE_BTREE
        DB_WriteTotalWordsPerFile(indexf->sw, idx, wordcount, indexf->DB);
#else
INDEXDATAHEADER *header = &indexf->header;

        if ( !header->TotalWordsPerFile || idx >= header->TotalWordsPerFileMax )
        {
            header->TotalWordsPerFileMax += 20000;  /* random guess -- could be a config setting */
            if(! header->TotalWordsPerFile)
               header->TotalWordsPerFile = emalloc( header->TotalWordsPerFileMax * sizeof(int) );
            else
               header->TotalWordsPerFile = erealloc( header->TotalWordsPerFile, header->TotalWordsPerFileMax * sizeof(int) );
        }

        header->TotalWordsPerFile[idx] = wordcount;
#endif
}









/*------------------------------------------------------*/
/*---------- General entry point of DB module ----------*/

void   *DB_Create (SWISH *sw, char *dbname)
{
   return sw->Db->DB_Create(sw, dbname);
}

void    DB_Remove(SWISH *sw, void *DB)
{
   sw->Db->DB_Remove(DB);
}

int     DB_InitWriteHeader(SWISH *sw, void *DB)
{
   return sw->Db->DB_InitWriteHeader(DB);
}

int     DB_WriteHeaderData(SWISH *sw, int id, unsigned char *s, int len, void *DB)
{
   return sw->Db->DB_WriteHeaderData(id, s,len,DB);
}

int     DB_EndWriteHeader(SWISH *sw, void *DB)
{
   return sw->Db->DB_EndWriteHeader(DB);
}



int     DB_InitWriteWords(SWISH *sw, void *DB)
{
   return sw->Db->DB_InitWriteWords(DB);
}

sw_off_t    DB_GetWordID(SWISH *sw, void *DB)
{
   return sw->Db->DB_GetWordID(DB);
}

int     DB_WriteWord(SWISH *sw, char *word, sw_off_t wordID, void *DB)
{
   return sw->Db->DB_WriteWord(word, wordID, DB);
}

#ifdef USE_BTREE
int     DB_UpdateWordID(SWISH *sw, char *word, sw_off_t wordID, void *DB)
{
   return sw->Db->DB_UpdateWordID(word, wordID, DB);
}

int     DB_DeleteWordData(SWISH *sw, sw_off_t wordID, void *DB)
{
   return sw->Db->DB_DeleteWordData(wordID, DB);
}

#endif

int     DB_WriteWordHash(SWISH *sw, char *word, sw_off_t wordID, void *DB)
{
   return sw->Db->DB_WriteWordHash(word, wordID, DB);
}

long    DB_WriteWordData(SWISH *sw, sw_off_t wordID, unsigned char *worddata, int data_size, int saved_bytes, void *DB)
{
   return sw->Db->DB_WriteWordData(wordID, worddata, data_size, saved_bytes, DB);
}

int     DB_EndWriteWords(SWISH *sw, void *DB)
{
   return sw->Db->DB_EndWriteWords(DB);
}


int     DB_InitWriteProperties(SWISH *sw, void *DB)
{
   return sw->Db->DB_InitWriteProperties(DB);
}


int     DB_WriteFileNum(SWISH *sw, int filenum, unsigned char *filedata,int sz_filedata, void *DB)
{
   return sw->Db->DB_WriteFileNum(filenum, filedata, sz_filedata, DB);
}

int     DB_RemoveFileNum(SWISH *sw, int filenum, void *DB)
{
   return sw->Db->DB_RemoveFileNum(filenum, DB);
}


#ifdef USE_PRESORT_ARRAY
int     DB_InitWriteSortedIndex(SWISH *sw, void *DB, int n_props)
{
   return sw->Db->DB_InitWriteSortedIndex(DB, n_props);
}

int     DB_WriteSortedIndex(SWISH *sw, int propID, int *data, int sz_data,void *DB)
{
   return sw->Db->DB_WriteSortedIndex(propID, data, sz_data,DB);
}


#else
int     DB_InitWriteSortedIndex(SWISH *sw, void *DB)
{
   return sw->Db->DB_InitWriteSortedIndex(DB);
}

int     DB_WriteSortedIndex(SWISH *sw, int propID, unsigned char *data, int sz_data,void *DB)
{
   return sw->Db->DB_WriteSortedIndex(propID, data, sz_data,DB);
}
#endif

int     DB_EndWriteSortedIndex(SWISH *sw, void *DB)
{
   return sw->Db->DB_EndWriteSortedIndex(DB);
}


void DB_WriteProperty( SWISH *sw, IndexFILE *indexf, FileRec *fi, int propID, char *buffer, int buf_len, int uncompressed_len, void *db)
{
    sw->Db->DB_WriteProperty( indexf, fi, propID, buffer, buf_len, uncompressed_len, db );
}

void    DB_WritePropPositions(SWISH *sw, IndexFILE *indexf, FileRec *fi, void *db)
{
    sw->Db->DB_WritePropPositions( indexf, fi, db);
}


void    DB_Reopen_PropertiesForRead(SWISH *sw, void *DB )
{
    sw->Db->DB_Reopen_PropertiesForRead(DB);
}


#ifdef USE_BTREE

int    DB_WriteTotalWordsPerFile(SWISH *sw, int idx, int wordcount, void *DB)
{
    return sw->Db->DB_WriteTotalWordsPerFile(sw, idx, wordcount, DB);
}


#endif


