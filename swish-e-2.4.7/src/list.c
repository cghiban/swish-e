/*
** Copyright (C) 1995, 1996, 1997, 1998 Hewlett-Packard Company
** Originally by Kevin Hughes, kev@kevcom.com, 3/11/94
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
    
** Mon May  9 15:09:27 CDT 2005
** added GPL




**
** 1998-07-04  addfilter   ( R. Scherg)
** 2001-02-28  rasc  -- addfilter removed here
**
*/

#include "swish.h"
#include "list.h"
#include "mem.h"
#include "metanames.h"
#include "swstring.h"
#include "hash.h"


struct swline *newswline_n(char *line, int size)
{
struct swline *newnode;

    newnode = (struct swline *)emalloc(sizeof(struct swline) + size);
    strncpy(newnode->line,line,size);
    newnode->next = NULL;

    return newnode;
}

struct swline *newswline(char *line)
{
struct swline *newnode;
int size = strlen(line);  /* Compute strlen only once */

    newnode = (struct swline *)emalloc(sizeof(struct swline) + size);
    memcpy(newnode->line,line,size + 1);
    newnode->next = NULL;

    return newnode;
}

struct swline *addswline(struct swline *rp, char *line)
{
struct swline *newnode;

    newnode = newswline(line);

    if (rp == NULL)
        rp = newnode;
    else
        rp->other.nodep->next = newnode;
    
    rp->other.nodep = newnode;
    
    return rp;
}

struct swline *dupswline(struct swline *rp)
{
struct swline *tmp=NULL, *tmp2=NULL;
struct swline *newnode;

    while(rp)
    {
        newnode = newswline(rp->line);
        
        if(!tmp)
            tmp = newnode;
        else
            tmp2->next=newnode;
        tmp2 = newnode;
        rp=rp->next;
    }
    tmp->other.nodep = tmp2;  /* Put last in nodep */
    return tmp;
}

void addindexfile(SWISH *sw, char *line)
{
    IndexFILE *head = sw->indexlist;
    IndexFILE *indexf = (IndexFILE *) emalloc(sizeof(IndexFILE));

    memset( indexf, 0, sizeof(IndexFILE) );

    indexf->sw = sw;  /* save parent object */
    indexf->line = estrdup(line);
    init_header(&indexf->header);
    indexf->next = NULL;


    /* Add default meta names -- these will be replaced if reading from an index file */
    add_default_metanames(indexf);

    /* Add index to end of list */

    if ( head == NULL )  /* first entry? */
        sw->indexlist = head = indexf;
    else
        head->nodep->next = indexf;  /* point the previous last one to the new last one */
    
    head->nodep = indexf;  /* set the last pointer */
}


void freeswline(struct swline *tmplist)
{
    struct swline *tmplist2;

    while (tmplist) {
        tmplist2 = tmplist->next;
        efree(tmplist);
        tmplist = tmplist2;
    }
}




void init_header(INDEXDATAHEADER *header)
{

    header->lenwordchars=header->lenbeginchars=header->lenendchars=header->lenignorelastchar=header->lenignorefirstchar=header->lenbumpposchars=MAXCHARDEFINED;

    header->wordchars = (char *)emalloc(header->lenwordchars + 1);
        header->wordchars = SafeStrCopy(header->wordchars,WORDCHARS,&header->lenwordchars);
        sortstring(header->wordchars);  /* Sort chars and remove dups */
        makelookuptable(header->wordchars,header->wordcharslookuptable);

    header->beginchars = (char *)emalloc(header->lenbeginchars + 1);
        header->beginchars = SafeStrCopy(header->beginchars,BEGINCHARS,&header->lenbeginchars);
        sortstring(header->beginchars);  /* Sort chars and remove dups */
        makelookuptable(header->beginchars,header->begincharslookuptable);

    header->endchars = (char *)emalloc(header->lenendchars + 1);
        header->endchars = SafeStrCopy(header->endchars,ENDCHARS,&header->lenendchars);
        sortstring(header->endchars);  /* Sort chars and remove dups */
        makelookuptable(header->endchars,header->endcharslookuptable);

    header->ignorelastchar = (char *)emalloc(header->lenignorelastchar + 1);
        header->ignorelastchar = SafeStrCopy(header->ignorelastchar,IGNORELASTCHAR,&header->lenignorelastchar);
        sortstring(header->ignorelastchar);  /* Sort chars and remove dups */
        makelookuptable(header->ignorelastchar,header->ignorelastcharlookuptable);

    header->ignorefirstchar = (char *)emalloc(header->lenignorefirstchar + 1);
        header->ignorefirstchar = SafeStrCopy(header->ignorefirstchar,IGNOREFIRSTCHAR,&header->lenignorefirstchar);
        sortstring(header->ignorefirstchar);  /* Sort chars and remove dups */
        makelookuptable(header->ignorefirstchar,header->ignorefirstcharlookuptable);


    header->bumpposchars = (char *)emalloc(header->lenbumpposchars + 1);
    header->bumpposchars[0]='\0';

    header->lenindexedon=header->lensavedasheader=header->lenindexn=header->lenindexd=header->lenindexp=header->lenindexa=MAXSTRLEN;
    header->indexn = (char *)emalloc(header->lenindexn + 1);header->indexn[0]='\0';
    header->indexd = (char *)emalloc(header->lenindexd + 1);header->indexd[0]='\0';
    header->indexp = (char *)emalloc(header->lenindexp + 1);header->indexp[0]='\0';
    header->indexa = (char *)emalloc(header->lenindexa + 1);header->indexa[0]='\0';
    header->savedasheader = (char *)emalloc(header->lensavedasheader + 1);header->savedasheader[0]='\0';
    header->indexedon = (char *)emalloc(header->lenindexedon + 1);header->indexedon[0]='\0';

    header->ignoreTotalWordCountWhenRanking = 1;
    header->minwordlimit = MINWORDLIMIT;
    header->maxwordlimit = MAXWORDLIMIT;

    makelookuptable("",header->bumpposcharslookuptable); 

    BuildTranslateChars(header->translatecharslookuptable,(unsigned char *)"",(unsigned char *)"");


    /* this is to ignore numbers */
    header->numberchars_used_flag = 0;  /* not used by default*/


    /* initialize the stemmer/fuzzy structure to None */
    header->fuzzy_data = set_fuzzy_mode( header->fuzzy_data, "None" );
}


void free_header(INDEXDATAHEADER *header)
{
    if(header->lenwordchars) efree(header->wordchars);
    if(header->lenbeginchars) efree(header->beginchars);
    if(header->lenendchars) efree(header->endchars);
    if(header->lenignorefirstchar) efree(header->ignorefirstchar);
    if(header->lenignorelastchar) efree(header->ignorelastchar);
    if(header->lenindexn) efree(header->indexn);
    if(header->lenindexa) efree(header->indexa);
    if(header->lenindexp) efree(header->indexp);
    if(header->lenindexd) efree(header->indexd);
    if(header->lenindexedon) efree(header->indexedon);        
    if(header->lensavedasheader) efree(header->savedasheader);    
    if(header->lenbumpposchars) efree(header->bumpposchars);

    
    /* Free the hashed word arrays */
    free_word_hash_table( &header->hashstoplist );
    free_word_hash_table( &header->hashbuzzwordlist );
    free_word_hash_table( &header->hashuselist );
    

    /* $$$ temporary until metas and props are seperated */
    if ( header->propIDX_to_metaID )
        efree( header->propIDX_to_metaID );

    if ( header->metaID_to_PropIDX )
        efree( header->metaID_to_PropIDX );

    /* free up the stemmer */
    free_fuzzy_mode( header->fuzzy_data );


#ifndef USE_BTREE
    if ( header->TotalWordsPerFile )
        efree( header->TotalWordsPerFile );
#endif

}



