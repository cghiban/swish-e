/*
$Id: docprop.c 2291 2009-03-31 01:56:00Z karpet $
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
    
** Mon May  9 15:51:39 CDT 2005
** added GPL

*********************************************************************************


** Functions to manage the index's Document Properties
**
** File Created.
** Mark Gaulin 11/24/98
**
** change sprintf to snprintf to avoid corruption,
** and use MAXSTRLEN from swish.h instead of literal "200"
** SRE 11/17/99
**
** 04/00 Jose ruiz
** storeDocProperties and readNextDocPropEntry modified to store
** the int numbers compressed. This makes integers "portable"
**
** 04/00 Jose Ruiz
** Added sorting results by property
**
** 07/00 and 08/00 - Jose Ruiz
** Many modifications to make all functions thread safe
**
** 08/00 - Added ascending and descending capabilities in results sorting
**
** 2001-01    rasc    getResultPropertyByName rewritten, datatypes for properties.
** 2001-02    rasc    isAutoProperty
**                    printSearchResultProperties changed
** 2001-03-15 rasc    Outputdelimiter var name changed
** 2001-06-08 wsm     Store propValue at end of docPropertyEntry to save memory
** 2001-06-14 moseley Most of the code rewritten, and propfile added
**
** 2001-09 jmruiz - ReadAllDocPropertiesFromDisk rewriten to be used
**                  by merge.c
**
*
* Doc Properties:
*
*   Do not expect the properties to be null terminated.  There's an associated
*   proplen with wach property.
*
*   When indexing:
*       parser.c:flush_buffer calls addDocProperties()
*
*       addDocProperties() scans all metanames looking for the flag that
*           indicates the text is inside that tag, and if so calls addDocProperty()
*
*       addDocProperty() then will either add or create a new property containing
*           the text passed in.  If it's new (which is normally the case) then
*           CreateProperty() is called.  That returns the actual property structure.
*           addDocProperty() will create or extend the list of properties connected
*           to the file, as needed.
*
*       After parsing is complete (all words indexed), index.c calls:
*
*       WritePropertiesToDisk() takes the list of properties assigned to the file (FileRec)
*           struct and for each prop compresses with zlib and then call
*           DB_WriteProperty() which writes the property to disk.
*
*       See db_native.c for how the data is written to disk.
*
*
*
*   Reading properties:
*       There's a few ways props are read.  First the APIs:
*
*       SWISH::API provides SwishProperty()
*           which calls a low-level function getResultPropValue() directly so
*           that integers and strings can be created.  There's also
*           SwishResultPropertyStr() which always returns a string.
*
*       The C library also provides two functions:
*           SwishResultPropertyStr() (same as above) turns all props to strings.
*           SwishResultPropertyULong() allow access numeric access to, well, numeric
*           props.  You need to know ahead of time if the property is numeric/date
*           before calling the ULong() function.  The ULong() function uses
*           the getResultPropValue() (below) to get at the raw properyt.
*
*       SwishResultPropertyStr() calls  getDocProperty() which is a general interface
*       to the properties:
*
*       getDocProperty() will return "internal" properties like rank, or
*           read the disk for properties stored at indexing time.  For those
*           "internal" properties getDocProperty() calls CreateProperty()
*           directly.  CreateProperty() converts numbers or strings into a
*           "propEntry" structure.
*
*           For on-disk properties getDocProperty() calls
*           ReadSingleDocPropertiesFromDisk() which calls DB_ReadProperty() to
*           read the raw data from disk, uncompresses if needed and then calls
*           CreateProperty() to convert the data into a real proprty.
*
*       As noted above SWISH::API uses getResultPropValue() which calls
*       getDocProperty() but returns a slightly different structure "PropValue"
*       which is a union to hold the different types of properties in their raw form.
*       This just allows the same return value to be used for both strings and integers.
*
*
*   Properties created using CreateProperty() (and the functions that call it) need
*   to be freed by calling freeProperty().
*
*   SwishResultPropertyStr() is a little odd in that it caches strings in the 
*   db_results structure.  This is done so the caller doesn't have to worry
*   about calling freeProperty().   Users of this function need to copy
*   the strings locally as soon as possible.  For example, I think this would not work:
*
*       char *propA = SwishResultPropertyStr( resultA, "foo" );
*       char *propB = SwishResultPropertyStr( resultB, "foo" );
*       printf("The two props are %s and %s, propA, propB);  # Won't work!
*
*   Both propA and propB now point to the same place -- the place used to cache
*   prop "foo".
*
*   Properties are read by swish in a number of places:
*
*   pre_sort.c calls ReadSingleDocPropertiesFromDisk() to build a sort array.
*   It's only sorting on-disk properties.
*
*   result_sort.c calls getDocProperty() because it can be sorting both on-disk
*   and "internal" properties.
*
*   proplimit.c also calls ReadSingleDocPropertiesFromDisk(), if needed.
*
*   merge.c calls ReadAllDocPropertiesFromDisk(), which is a wrapper around the
*   ReadSingleDocPropertiesFromDisk() call.  Merge needs all the props loaded.
*
*
*
*/

#include <limits.h>     // for ULONG_MAX
#include "swish.h"
#include "swstring.h"
#include "file.h"
#include "hash.h"
#include "mem.h"
#include "merge.h"
#include "error.h"
#include "search.h"
#include "index.h"
#include "docprop.h"
#include "error.h"
#include "compress.h"
#include "metanames.h"
#include "result_output.h"
#include "result_sort.h"
#include "entities.h"
#include "db.h"
#include "rank.h"
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

/*******************************************************************
*   Convert a propValue to a unsigned long
*
********************************************************************/
union _conv_ {
      char            c[sizeof(unsigned long)];
      unsigned long   l;
};
unsigned long convPropValue2ULong(unsigned char *propValue)
{
      union _conv_ u;
      memcpy(u.c, propValue, sizeof(unsigned long));
      return u.l;
}


/*******************************************************************
*   Free a property entry
*
********************************************************************/

void freeProperty( propEntry *prop )
{
    if ( prop )
        efree(prop);
}




/*******************************************************************
*   Free all properties in the docProperties structure
*
********************************************************************/


void freeDocProperties(docProperties *docProperties)
{
    int i;

    for( i = 0; i < docProperties->n; i++ )
    {
        freeProperty( docProperties->propEntry[i] );
        docProperties->propEntry[i] = NULL;
    }

    efree(docProperties);
    docProperties = NULL;

}


/*******************************************************************
*   Frees a FileRec (struct file), which just frees
*   the properties and property index
*   Doesn't free the FileRec itself.
*
*   move here from swish.c since all FileRec really holds is property info
*
********************************************************************/

void freefileinfo(FileRec *fi)
{

    if ( fi->docProperties )
    {
        freeDocProperties( fi->docProperties );
        fi->docProperties = NULL;
    }

    if ( fi->prop_index )
    {
        efree( fi->prop_index );
        fi->prop_index = NULL;
    }
}

/*******************************************************************
*   Converts a property into a string, based on it's type.
*   Numbers are zero filled
*
*   Call with:
*       *metaEntry
*       *propEntry
*
*   Returns:
*       malloc's a new string.  Caller must call free().
*       if passed in prop is null then returns empty string.
*
*
********************************************************************/

char *DecodeDocProperty( struct metaEntry *meta_entry, propEntry *prop )
{
    char *s;
    unsigned long i;

    if ( !meta_entry )
        progerr("DecodeDocProperty passed NULL meta_entry");


    if ( !prop )
        return estrdup("");


    if ( is_meta_string(meta_entry) )      /* check for ascii/string data */
        return (char *)bin2string(prop->propValue,prop->propLen);


    if ( is_meta_date(meta_entry) )
    {
        s=emalloc(30);
        /* i = *(unsigned long *) prop->propValue; */  /* read binary */ 
        i = convPropValue2ULong(prop->propValue); /* read binary */
        i = UNPACKLONG(i);     /* Convert the portable number */
        strftime(s,30, DATE_FORMAT_STRING, (struct tm *)localtime((time_t *)&i));
        return s;
    }



    if ( is_meta_number(meta_entry) )
    {
        s=emalloc(14);
        /* i=*(unsigned long *)prop->propValue; */ /* read binary */
        i = convPropValue2ULong(prop->propValue); /* read binary */
        i = UNPACKLONG(i);     /* Convert the portable number */
        sprintf(s,"%lu",i);
        return s;
    }

    progwarn("Invalid property type for property '%s'\n", meta_entry->metaName );
    return estrdup("");
}

/*******************************************************************
*   Returns a property (really the head of the list)
*   for the specified property
*
*   Call with:
*       *RESULT
*       *metaEntry - pointer to related meta entry
*       metaID - OR, if metaEntry is NULL uses this to lookup metaEntry
*       max_size - limit size of property loaded
*
*   Returns:
*       *propEntry
*
*   Warning:
*       Only returns first property in list (which is the last property added)
*
*   Notes:
*       with PROPFILE, caller is expected to destroy the property
*
*
********************************************************************/

propEntry *getDocProperty( RESULT *result, struct metaEntry **meta_entry, int metaID, int max_size )
{
    IndexFILE *indexf = result->db_results->indexf;
    int     error_flag;
    unsigned long num;


    /* Grab the meta structure for this ID, unless one was passed in */

    if ( *meta_entry )
        metaID = (*meta_entry)->metaID;

    else if ( !(*meta_entry = getPropNameByID(&indexf->header, metaID )) )
        return NULL;


    /* This is a memory leak if not using PROPFILE */


    /* Some properties are generated during a search */
    if ( is_meta_internal( *meta_entry ) )
    {
        if ( is_meta_entry( *meta_entry, AUTOPROPERTY_RESULT_RANK ) )
        {
           /* return raw rank if flag set */
            if (result->db_results->results->sw->ReturnRawRank) {
                num = PACKLONG( result->rank );
                return CreateProperty( *meta_entry, (unsigned char *)&num, sizeof( num ), 1, &error_flag );
            }

            int scale_factor = result->db_results->results->rank_scale_factor;
            unsigned long rank_num;

            /* scale_factor is zero while sorting, so use the raw rank */
            /* otherwise, scale for display */

            if ( scale_factor )
            {
                rank_num = (unsigned long) (result->rank * scale_factor)/10000;

                if ( rank_num >= 999)
                    rank_num = 1000;
                else if ( rank_num < 1)
                    rank_num = 1;
            }
            else
                rank_num = result->rank;

            num = PACKLONG( rank_num );
            return CreateProperty( *meta_entry, (unsigned char *)&num, sizeof( num ), 1, &error_flag );
        }

        if ( is_meta_entry( *meta_entry, AUTOPROPERTY_REC_COUNT ) )
        {
            num = PACKLONG( (unsigned long)result->db_results->results->cur_rec_number );
            return CreateProperty( *meta_entry, (unsigned char *)&num, sizeof( num ), 1, &error_flag );
        }

        if ( is_meta_entry( *meta_entry, AUTOPROPERTY_FILENUM ) )
        {
            num = PACKLONG( (unsigned long)result->filenum );
            return CreateProperty( *meta_entry, (unsigned char *)&num, sizeof( num ), 1, &error_flag );
        }


        if ( is_meta_entry( *meta_entry, AUTOPROPERTY_INDEXFILE ) )
            return CreateProperty( *meta_entry, (unsigned char *)result->db_results->indexf->line, strlen( result->db_results->indexf->line ), 0, &error_flag );
    }


    return ReadSingleDocPropertiesFromDisk(indexf, &result->fi, metaID, max_size );
}


/*******************************************************************
*   Returns a string for the property ID supplied
*   Numbers are zero filled
*
*   Call with:
*       *RESULT
*       metaID
*
*   Returns:
*       malloc's a new string.  Caller must call free().
*
*   Bugs:
*       Only returns first property in list (which is the last property)
*       This function is called by dump.c and by result_output.c to
*       display the old -p style property listings.
*
********************************************************************/

char *getResultPropAsString(RESULT *result, int ID)
{
    char *s = NULL;
    propEntry *prop;
    struct metaEntry *meta_entry = NULL;


    if( !result )
        return estrdup("");  // when would this happen?



    if ( !(prop = getDocProperty(result, &meta_entry, ID, 0 )) )
        return estrdup("");

    /* $$$ Ignores possible other properties that are linked to this one */
    s = DecodeDocProperty( meta_entry, prop );

    freeProperty( prop );

    return s;
}

/*******************************************************************
*   SwishResultPropertyStr - Returns a string for the property *name* supplied
*   Numbers are zero filled  (why??)
*
*   ** Library interface call **
*
*   Call with:
*       *RESULT
*       char * property name
*
*   Returns:
*       A string -- caller does not need to free as the strings are
*       cleaned up on every call
*
*
********************************************************************/

char *SwishResultPropertyStr(RESULT *result, char *pname)
{
    char                *s = NULL;
    propEntry           *prop;
    struct metaEntry    *meta_entry = NULL;
    IndexFILE           *indexf;
    DB_RESULTS          *db_results;

    if( !result )
        progerr("SwishResultPropertyStr was called with a NULL result");


    db_results = result->db_results;
    indexf = result->db_results->indexf;


    /* Ok property name? */

    if ( !(meta_entry = getPropNameByName( &indexf->header, pname )) )
    {
        set_progerr(UNKNOWN_PROPERTY_NAME_IN_SEARCH_DISPLAY, indexf->sw, "Invalid property name '%s'", pname );
        return "(null)";
    }

    /* reset error level */
    result->db_results->indexf->sw->lasterror = 0;




    /* Does this results have this property? */

    if ( !(prop = getDocProperty(result, &meta_entry, 0, 0 )) )
        return "";

    s = DecodeDocProperty( meta_entry, prop );

    freeProperty( prop );

    if ( !*s )  /* blank string? */
    {
        efree( s );
        return "";
    }


    if ( ! db_results->prop_string_cache )
    {
        db_results->prop_string_cache = (char **)emalloc( indexf->header.metaCounter * sizeof( char *) );
        memset( db_results->prop_string_cache, 0, indexf->header.metaCounter * sizeof( char *) );
    }

    /* Free previous, if needed  -- note the metaIDs start at one */

    else if ( db_results->prop_string_cache[ meta_entry->metaID-1 ] )
        efree( db_results->prop_string_cache[ meta_entry->metaID-1 ] );

    db_results->prop_string_cache[ meta_entry->metaID-1 ] = s;
    return s;
}




/*******************************************************************
*   SwishResultPropertyULong - Returns an unsigned long for the property *name* supplied
*
*   ** Library interface call **
*
*   Call with:
*       *RESULT
*       char * property name
*
*   Returns:
*       unsigned long
*       ULONG_MAX on error
*
*
********************************************************************/

unsigned long SwishResultPropertyULong(RESULT *result, char *pname)
{
    PropValue           *pv;
    unsigned long       value = ULONG_MAX;


    /* Fetch the property */
    pv = getResultPropValue (result, pname, 0 );

    if ( !pv )
        return ULONG_MAX;  /* bad property name */

    /* Make sure it's of the correct type */
    if ( (PROP_ULONG != pv->datatype) && (PROP_DATE != pv->datatype) )
    {
        if ( PROP_UNDEFINED != pv->datatype )
            set_progerr(INVALID_PROPERTY_TYPE, result->db_results->indexf->sw,
                    "Property '%s' is not numeric", pname );
        value = ULONG_MAX;
    }
    else
        value = pv->value.v_ulong;

    freeResultPropValue( pv );

    return value;
}



/*******************************************************************
*   Returns a property as a *propValue, which is a union of different
*   data types, with a flag to indicate the type
*   Can be called with either a metaname, or a metaID.
*
*   Call with:
*       *RESULT
*       *metaName -- String name of meta entry
*       metaID    -- OR - meta ID number
*
*       Note that the ID is not really used anyplace, but
*       could be used to save the prop->id lookup.
*
*   Returns:
*       pointer to a propValue structure if found -- caller MUST free
*       Returns NULL if propertyName doesn't exist.
*       Jan 14, 2004:
*       Returns a PropValue PROP_UNDEFINED if result has not property
*       If returning NULL (i.e. bad property name) sets a swish-e error.
*       Caller is responsible for checking.
*
*   Note:
*       Feb 13, 2002 - now defined properties that just don't exist
*       for the document return a blank *string* even for numeric
*       and date properties.  This it to prevent "(NULL)" from displaying.
*       They used to return NULL, but since currently only result_output.c
*       uses this function, it's not a problem.
*
*
********************************************************************/

PropValue *getResultPropValue (RESULT *r, char *pname, int ID )
{
    PropValue *pv;
    struct metaEntry *meta_entry = NULL;
    propEntry *prop;

    /* Die on null result */
    if( !r )
        progerr("Called getResultPropValue with NULL result");

    /* Lookup by property name, if supplied */
    if ( pname )
        if ( !(meta_entry = getPropNameByName( &r->db_results->indexf->header, pname )) )
        {
            set_progerr(UNKNOWN_PROPERTY_NAME_IN_SEARCH_DISPLAY, r->db_results->indexf->sw,
                    "Invalid property name '%s'", pname );
            return NULL;
        }

    /* reset error level */
    r->db_results->indexf->sw->lasterror = 0;


    /* create a propvalue to return to caller */
    pv = (PropValue *) emalloc (sizeof (PropValue));
    pv->datatype = PROP_UNDEFINED;
    pv->destroy = 0;



    /* This will return false if the result does not have a value for this property */
    prop = getDocProperty( r, &meta_entry, ID, 0 );

    if ( !prop )
        return pv;  /* returning PROP_UNDEFINED */


    if ( is_meta_string(meta_entry) )      /* check for ascii/string data */
    {
        pv->datatype = PROP_STRING;
        pv->destroy++;       // caller must free this
        pv->value.v_str = (char *)bin2string(prop->propValue,prop->propLen);
        freeProperty( prop );
        return pv;
    }


    if ( is_meta_number(meta_entry) )
    {
        unsigned long i;
        /* i = *(unsigned long *) prop->propValue;*/  /* read binary */
        i = convPropValue2ULong(prop->propValue); /* read binary */
        i = UNPACKLONG(i);     /* Convert the portable number */
        pv->datatype = PROP_ULONG;
        pv->value.v_ulong = i;
        freeProperty( prop );
        return pv;
    }


    if ( is_meta_date(meta_entry) )
    {
        unsigned long i;
        /* i = *(unsigned long *) prop->propValue; */ /* read binary */
        i = convPropValue2ULong(prop->propValue); /* read binary */
        i = UNPACKLONG(i);     /* Convert the portable number */
        pv->datatype = PROP_DATE;
        pv->value.v_date = (time_t)i;
        freeProperty( prop );
        return pv;
    }



    /* If here, then it's an unknown property type and abort! */
    progerr("Swish-e database error.  Unknown property type '%d'", meta_entry->metaType );
    return NULL;  /* make compier happy */

}

/*******************************************************************
*   Destroys a "pv" returned from getResultPropValue
*
*
********************************************************************/
void    freeResultPropValue(PropValue *pv)
{
    if ( !pv ) return;

    if ( pv->datatype == PROP_STRING && pv->destroy )
        efree( pv->value.v_str );

    efree(pv);
}

/*******************************************************************
*   Converts a string into a string for saving as a property
*   Which means will either return a duplicated string,
*   or a packed unsigned long.
*
*   Call with:
*       *metaEntry
*       **encodedStr (destination)
*       *string
*       *error_flag - integer to indicate the difference between an error and a blank property
*
*   Returns:
*       malloc's a new string, stored in **encodedStr.  Caller must call free().
*       length of encoded string, or zero if an error
*       (zero length strings are not for encoding anyway, I guess)
*
*   QUESTION: ???
*       should this return a *docproperty instead?
*       numbers are unsigned longs.  What if someone
*       wanted to store signed numbers?
*
*   ToDO:
*       What about convert entities here?
*
********************************************************************/
static int EncodeProperty( struct metaEntry *meta_entry, char **encodedStr, char *propstring, int *error_flag )
{
    unsigned long int num;
    char     *newstr;
    char     *badchar;
    char     *tmpnum;
    char     *string;


    string = propstring;

    *error_flag = 0;

    /* skip leading white space */
    while ( isspace( (int)*string ))
        string++;

    if ( !string || !*string )
    {
        // progwarn("Null string passed to EncodeProperty for meta '%s'", meta_entry->metaName);
#ifdef BLANK_PROP_VALUE
        string = BLANK_PROP_VALUE;  // gets dup'ed below
#else
        return 0;
#endif
    }


    /* make a working copy */
    string = estrdup( string );

    /* remove trailing white space  */
    {
        int i = strlen( string );

        while ( i  && isspace( (int)string[i-1]) )
            string[--i] = '\0';
    }


    if (is_meta_number( meta_entry ) || is_meta_date( meta_entry ))
    {
        int j;

        newstr = emalloc( sizeof( num ) + 1 );
        num = strtoul( string, &badchar, 10 ); // would base zero be more flexible?

        if ( num == ULONG_MAX )
        {
            progwarnno("EncodeProperty - Attempted to convert '%s' to a number", string );
            efree(string);
            (*error_flag)++;
            return 0;
        }

        if ( *badchar ) // I think this is how it works...
        {
            progwarn("EncodeProperty - Invalid char '%c' found in string '%s'", badchar[0], string);
            efree(string);
            (*error_flag)++;
            return 0;
        }
        /* I'll bet there's an easier way */
        num = PACKLONG(num);
        tmpnum = (char *)&num;

        for ( j=0; j <= (int)sizeof(num)-1; j++ )
            newstr[j] = (unsigned char)tmpnum[j];

        newstr[ sizeof(num) ] = '\0';

        *encodedStr = newstr;

        efree(string);

        return (int)sizeof(num);
    }


    if ( is_meta_string(meta_entry) )
    {
        /* replace all non-printing chars with a space -- this is questionable */
        // yep, sure is questionable -- isprint() kills 8859-1 chars.

        if ( !is_meta_nostrip(meta_entry) )
        {
            char *source, *dest;
            dest = string;
            for( source = string; *source; source++ )
            {
                /* Used to replace (<=' ') even spaces with a single space */
                if ( (int)((unsigned char)*source) < (int)' ' )
                {
                    if ( dest > string && *(dest - 1) != ' ' )
                    {
                        *dest = ' ';
                        dest++;
                    }
                    continue;
                }

                *dest = *source;
                dest++;
            }
            *dest = '\0';
        }

        *encodedStr = string;
        return (int)strlen( string );
    }


    progwarn("EncodeProperty called but doesn't know the property type :(");
    return 0;
}

/*******************************************************************
*   Creates a document property
*
*   Call with:
*       *metaEntry
*       *propValue  - string to add
*       *propLen    - length of string to add, but can be limited by metaEntry->max_size
*       preEncoded  - flag saying the data is already encoded
*                     (that's for filesize, last modified, start position)
*       *error_flag - integer to indicate the difference between an error and a blank property
*
*   Returns:
*       pointer to a newly created document property
*       NULL indicates property could not be created
*
*
********************************************************************/

propEntry *CreateProperty(struct metaEntry *meta_entry, unsigned char *propValue, int propLen, int preEncoded, int *error_flag )
{
    propEntry *docProp;


    /* convert string to a document property, if not already encoded */
    if ( !preEncoded )
    {
        char *tmp;

        propLen = EncodeProperty( meta_entry, &tmp, (char *)propValue, error_flag );

        if ( !propLen )  /* Error detected in encode */
            return NULL;

        /* Limit length */
        if ( is_meta_string(meta_entry) && meta_entry->max_len && propLen > meta_entry->max_len )
            propLen = meta_entry->max_len;

        propValue = (unsigned char *)tmp;
    }

    /* Now create the property $$ could be -1 */
    /* This is creating more memory than needed (4 extra bytes) */
    docProp=(propEntry *) emalloc(sizeof(propEntry) + propLen);

    memcpy(docProp->propValue, propValue, propLen);
    docProp->propLen = propLen;

    /* sizeof(propEntry) already contains space for part of the propLen, so throw a null in */
    /* This *hack* should all compare of the properites using strcoll, which expects a null */
    /* most places expect propValue to be exactly propLen long.  The null is not written to disk */
    docProp->propValue[propLen] = '\0';


    /* EncodeProperty creates a new string */
        if ( !preEncoded )
            efree( propValue );


    return docProp;
}

/*******************************************************************
*   Appends a string onto a current property
*
*   Call with:
*       *propEntry
*       *string
*       length of string
*
*   Will limit property length, if needed.
*
*******************************************************************/
propEntry *append_property( struct metaEntry *meta_entry, propEntry *p, char *txt, int length )
{
    int     newlen;
    int     add_a_space = 0;
    char   *str = NULL;
    int     error_flag = 0;

    length = EncodeProperty( meta_entry, &str, txt, &error_flag );

    if ( !length )
        return p;

    /* When appending, we separate by a space -- could be a config setting */
    if ( !isspace( (int)*str ) && !isspace( (int)p->propValue[p->propLen-1] ) )
        add_a_space++;


    /* Any room to add the property? */
    if ( meta_entry->max_len &&  (int)p->propLen + add_a_space >=  meta_entry->max_len )
    {
        if ( str )
            efree( str );

        return p;
    }


    newlen = p->propLen + length + add_a_space;

    /* limit length */
    if ( meta_entry->max_len && newlen >= meta_entry->max_len )
    {
        newlen = meta_entry->max_len;
        length = meta_entry->max_len - p->propLen - add_a_space;
    }


    /* Now reallocate the property */
    p = (propEntry *) erealloc(p, sizeof(propEntry) + newlen);

    if ( add_a_space )
        p->propValue[p->propLen++] = ' ';

    memcpy( (void *)&(p->propValue[p->propLen]), str, length );
    p->propLen = newlen;

    if (str)
        efree(str);

    return p;
}


/*******************************************************************
*   Scans the properties (metaEntry's), and adds a doc property to any that are flagged
*   Limits size, if needed (for StoreDescription)
*   Pass in text properties (not pre-encoded binary properties)
*
*   Call with:
*       *INDEXDATAHEADER (to get to the list of metanames)
*       **docProperties - pointer to list of properties
*       *propValue  - string to add
*       *propLen    - length of string to add
*
*   Returns:
*       void, but will warn on failed properties
*
*
********************************************************************/
void addDocProperties( INDEXDATAHEADER *header, docProperties **docProperties, unsigned char *propValue, int propLen, char *filename )
{
    struct metaEntry *m;
    int     i;

    for ( i = 0; i < header->metaCounter; i++)
    {
        m = header->metaEntryArray[i];

        if ( (m->metaType & META_PROP) && m->in_tag )
            if ( !addDocProperty( docProperties, m, propValue, propLen, 0 ) )
                progwarn("Failed to add property '%s' in file '%s'", m->metaName, filename );
    }
}




/*******************************************************************
*   Adds a document property to the list of properties.
*   Creates or extends the list, as necessary
*
*   Call with:
*       **docProperties - pointer to list of properties
*       *metaEntry
*       *propValue  - string to add
*       *propLen    - length of string to add
*       preEncoded  - flag saying the data is already encoded
*                     (that's for filesize, last modified, start position)
*
*   Returns:
*       true if added property
*       sets address of **docProperties, if list changes size
*
*
********************************************************************/

int addDocProperty( docProperties **docProperties, struct metaEntry *meta_entry, unsigned char *propValue, int propLen, int preEncoded )
{
    struct docProperties *dp = *docProperties;
    propEntry *docProp;
    int i;
    int error_flag;


    /* Allocate or extend the property array, if needed */

    if( !dp )
    {
        dp = (struct docProperties *) emalloc(sizeof(struct docProperties) + (meta_entry->metaID + 1) * sizeof(propEntry *));
        *docProperties = dp;

        dp->n = meta_entry->metaID + 1;

        for( i = 0; i < dp->n; i++ )
            dp->propEntry[i] = NULL;
    }

    else /* reallocate if needed */
    {
        if( dp->n <= meta_entry->metaID )
        {
            dp = (struct docProperties *) erealloc(dp,sizeof(struct docProperties) + (meta_entry->metaID + 1) * sizeof(propEntry *));

            *docProperties = dp;
            for( i = dp->n; i <= meta_entry->metaID; i++ )
                dp->propEntry[i] = NULL;

            dp->n = meta_entry->metaID + 1;
        }
    }

    /* Un-encoded STRINGS get appended to existing properties */
    /* Others generate a warning */
    if ( dp->propEntry[meta_entry->metaID] )
    {
        if ( is_meta_string(meta_entry) )
        {
            dp->propEntry[meta_entry->metaID] = append_property( meta_entry, dp->propEntry[meta_entry->metaID], (char *)propValue, propLen );
            return 1;
        }
        else // Will this come back and bite me?
        {
            progwarn("Warning: Attempt to add duplicate property." );
            return 0;
        }
    }


    /* create the document property */
    /* Ignore some errors */

    if ( !(docProp = CreateProperty( meta_entry, propValue, propLen, preEncoded, &error_flag )) )
        return error_flag ? 0 : 1;

    dp->propEntry[meta_entry->metaID] = docProp;

    return 1;
}

/* #define DEBUGPROP 1 */

#ifdef DEBUGPROP
static int insidecompare = 0;
#endif

/*******************************************************************
*   Compares two properties for sorting
*
*   Call with:
*       *metaEntry
*       *docPropertyEntry1
*       *docPropertyEntry2
*
*   Returns:
*       0 - two properties are the same
*      -1 - docPropertyEntry1 < docPropertyEntry2
*      +1 - docPropertyEntry1 > docPropertyEntry2
*
*
********************************************************************/
int Compare_Properties( struct metaEntry *meta_entry, propEntry *p1, propEntry *p2 )
{


#ifdef DEBUGPROP
    if ( !insidecompare++ )
    {
        printf("comparing properties for meta %s: returning: %d\n", meta_entry->metaName, Compare_Properties( meta_entry, p1, p2) );
        dump_single_property( p1, meta_entry );
        dump_single_property( p2, meta_entry );
        insidecompare = 0;
    }
#endif


    if ( !p1 && p2 )
        return -1;


    if ( !p1 && !p2 )
        return 0;

    if ( p1 && !p2 )
        return +1;


    if (is_meta_number( meta_entry ) || is_meta_date( meta_entry ))
        return memcmp( (const void *)p1->propValue, (const void *)p2->propValue, p1->propLen );


    if ( is_meta_string(meta_entry) )
    {
        int rc;
        int len = Min( p1->propLen, p2->propLen );

#ifdef HAVE_STRCOLL
        /* note that the propValue should be null terminated.  See CreateProperty()  */
        /* could use a strncoll() and one for case ignore */
#ifdef DEBUGPROP
        if ( is_meta_use_strcoll(meta_entry) )
            printf("using strcol %s <=> %s = %d\n",  (const char *)p1->propValue, (const char *)p2->propValue,strcoll( (const char *)p1->propValue, (const char *)p2->propValue ));
#endif
        if ( is_meta_use_strcoll(meta_entry) )
            return strcoll( (const char *)p1->propValue, (const char *)p2->propValue );
#endif

        rc = is_meta_ignore_case( meta_entry)
             ? strncasecmp( (char *)p1->propValue, (char *)p2->propValue, len )
             : strncmp( (char *)p1->propValue, (char *)p2->propValue, len );

#ifdef DEBUGPROP
        printf("using %s  %s <=> %s = %d\n", (is_meta_ignore_case( meta_entry) ? "strncasecmp" : "strncmp"),  (const char *)p1->propValue, (const char *)p2->propValue, rc );
#endif


        if ( rc != 0 )
            return rc;

        return p1->propLen - p2->propLen;
    }

    return 0;  /* This should be an error here */

}

/*******************************************************************
*   Duplicate a property that's already in memory and return it.
*
*   Caller must destroy
*
*********************************************************************/

static propEntry *duplicate_in_mem_property( docProperties *props, int metaID, int max_size )
{
    propEntry      *docProp;
    struct          metaEntry meta_entry;
    int             propLen;
    int             error_flag;

    if ( metaID >= props->n )
        return NULL;

    if ( !(docProp = props->propEntry[ metaID ]) )
        return NULL;


    meta_entry.metaName = "(default)";  /* for error message, I think */
    meta_entry.metaID   = metaID;


    /* Duplicate the property */
    propLen = docProp->propLen;

    /* Limit size,if possible - should really check if it's a string */
    if ( max_size && (max_size < propLen ))
        propLen = max_size;

    /* Duplicate the property */
    return CreateProperty( &meta_entry, docProp->propValue, propLen, 1, &error_flag );
}


#ifdef HAVE_ZLIB

/*******************************************************************
*   Allocate or reallocate the property buffer
*
*   The buffer is kept around to avoid reallocating for every prop of every doc
*
*
*
*********************************************************************/

unsigned char *allocatePropIOBuffer(SWISH *sw, unsigned long buf_needed )
{
    unsigned long total_size;

    if ( !buf_needed )
        progerr("Asked for too small of a buffer size!");


    if ( !sw->Prop_IO_Buf ||  buf_needed > sw->PropIO_allocated )
    {
        /* don't reallocate because we don't need to memcpy */
        if ( sw->Prop_IO_Buf )
            efree( sw->Prop_IO_Buf );


        total_size = buf_needed > sw->PropIO_allocated + RD_BUFFER_SIZE
                    ? buf_needed
                    : sw->PropIO_allocated + RD_BUFFER_SIZE;

        sw->Prop_IO_Buf = emalloc( total_size );
        sw->PropIO_allocated = total_size;  /* keep track of structure size */
    }


    return sw->Prop_IO_Buf;
}

#endif


/*******************************************************************
*   Uncompress a Property
*
*   Call with:
*       SWISH
*       *input_buf          - buffer address
*       buf_len             - size of buffer
*       *uncompressed_size  - size of original prop, or zero if not compressed.
*
*   Returns:
*       buffer address of uncompressed property
*       uncompressed_size is set to length of buffer
*
*
*********************************************************************/

static unsigned char *uncompress_property( SWISH *sw, unsigned char *input_buf, int buf_len, int *uncompressed_size )
{

#ifndef HAVE_ZLIB

    if ( *uncompressed_size )
        progerr("The index was created with zlib compression.\n This version of swish was not compiled with zlib");

    *uncompressed_size = buf_len;
    return input_buf;

#else
    unsigned char   *PropBuf;
    int             zlib_status = 0;
    uLongf          buf_size = (uLongf)*uncompressed_size;



    if ( *uncompressed_size == 0 ) /* wasn't compressed */
    {
        *uncompressed_size = buf_len;
        return input_buf;
    }



    /* make sure we have enough space */

    PropBuf = allocatePropIOBuffer( sw, *uncompressed_size );


    zlib_status = uncompress(PropBuf, &buf_size, input_buf, buf_len );
    if ( zlib_status != Z_OK )
    {
        // $$$ make sure this works ok if returning null $$$
        progwarn("Failed to uncompress Property. zlib uncompress returned: %d.  uncompressed size: %d buf_len: %d\n",
            zlib_status, buf_size, buf_len );
        return NULL;
    }


    *uncompressed_size = (int)buf_size;


    return PropBuf;


#endif

}




/*******************************************************************
*   Reads a single doc property - this is used for sorting
*
*   Caller needs to destroy returned property
*
*   Call with:
*       indexf  - which index to read from
*       FileRec - which contains filenum (key part 1)
*       metaID  - which prop (key part 2)
*       max_size- to limit size of property
*
*   Returns:
*       *propEntry - caller *must* destroy
*
*
*********************************************************************/
propEntry *ReadSingleDocPropertiesFromDisk( IndexFILE *indexf, FileRec *fi, int metaID, int max_size )
{
    SWISH           *sw = indexf->sw;
    int             propLen;
    int             error_flag;
    struct          metaEntry meta_entry;
    unsigned char  *buf;
    int             buf_len;            /* size on disk */
    int             uncompressed_len;   /* size uncompressed */
    propEntry      *docProp;
    unsigned char  *propbuf;
    INDEXDATAHEADER *header = &indexf->header;
    int             count;
    int             propIDX;


    /* initialize the first time called */
    if ( header->property_count == 0 )
        init_property_list(header);

    if ( (count = header->property_count) <= 0)
        return NULL;


    /* Map the propID to an index number */
    propIDX = header->metaID_to_PropIDX[metaID];

    if ( propIDX < 0 )
        progerr("Mapped propID %d to invalid property index", metaID );


    /* limit size if requested and is a string property - hope this isn't too slow */

    if ( max_size )
    {
        struct metaEntry *m = getPropNameByID(header, metaID); /* might be better if the caller passes the metaEntry */

        /* Reset to zero if the property is not a string */
        if ( !is_meta_string( m ) )
            max_size = 0;
    }





    /* already loaded? -- if so, duplicate the property for the given length */
    /* This should only happen if ReadAllDocPropertiesFromDisk() was called, and only with db_native.c */

    if ( fi->docProperties )
        return duplicate_in_mem_property( fi->docProperties, metaID, max_size );


    /* Otherwise, read from disk */

    if ( !(buf = (unsigned char*)DB_ReadProperty( sw, indexf, fi, metaID, &buf_len, &uncompressed_len, indexf->DB )))
        return NULL;

        if ( !(propbuf = uncompress_property( sw, buf, buf_len, &uncompressed_len )) )
            return NULL;

        propLen = uncompressed_len; /* just to be clear ;) */

    /* Limit size,if possible  */
    if ( max_size && (max_size < propLen ))
        propLen = max_size;


    meta_entry.metaName = "(default)";  /* for error message, I think */
    meta_entry.metaID   = metaID;

    docProp = CreateProperty( &meta_entry, propbuf, propLen, 1, &error_flag );

        efree( buf );
        return docProp;
}



/*******************************************************************
*   Reads the doc properties from disk
*
*   Maybe should return void, and just set?
*   Or maybe should take a filenum, and instead take a position?
*
*   The original idea (and the way it was written) was to use the seek
*   position of the first property, and the total length of all properties
*   then read all the properties in one fread call.
*   The plan was to call it in result_output.c, so all the props would get loaded
*   in one shot.
*   That design probably has little effect on performance.  Now we just call
*   ReadSingleDocPropertiesFromDisk for each prop.
*
*   Now, this is really just a way to populate the fi->docProperties structure.
*
*   2001-09 jmruiz Modified to be used by merge.c
*********************************************************************/

docProperties *ReadAllDocPropertiesFromDisk( IndexFILE *indexf, int filenum )
{
    FileRec         fi;
    propEntry      *new_prop;
    int             count;
    struct          metaEntry meta_entry;
    docProperties   *docProperties=NULL;
    INDEXDATAHEADER *header = &indexf->header;
    int             propIDX;



    /* Get a place to cache the pointers */
    memset(&fi,0, sizeof( FileRec ));
    fi.filenum = filenum;


    meta_entry.metaName = "(default)";  /* for error message, I think */


    /* initialize the first time called */
    if ( header->property_count == 0 )
        init_property_list(header);

    if ( (count = header->property_count) <= 0)
        return NULL;


    for ( propIDX = 0; propIDX < count; propIDX++ )
    {
        meta_entry.metaID = header->propIDX_to_metaID[propIDX];

        new_prop = ReadSingleDocPropertiesFromDisk( indexf, &fi, meta_entry.metaID, 0);

        if ( !new_prop )
            continue;

        // would be better if we didn't need to create a new property just to free one
        // this routine is currently only used by merge and dump.c

        addDocProperty(&docProperties, &meta_entry, new_prop->propValue, new_prop->propLen, 1 );

        efree( new_prop );
    }

    /* Free the prop seek location cache */
    if ( fi.prop_index )
        efree( fi.prop_index );

    return docProperties;
}





