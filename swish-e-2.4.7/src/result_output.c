/*
$Id: result_output.c 1736 2005-05-12 15:41:22Z karman $
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
    
    
** Mon May  9 14:50:41 CDT 2005
** added GPL

***************************************************************************************

   -- This module does result output for swish-e
   -- This module implements some methods about the 
   -- "-x fmt" cmd option.
   -- basically: handle output fmts like:  -x "%c|<swishtitle fmt=/%20s/>\n"
   -- 
   -- License: see swish licence file

   -- 2001-01  R. Scherg  (rasc)   initial coding

   -- 2001-02-09 rasc    make propertynames always lowercase!  (may change) 
				 this is get same handling as metanames...
   -- 2001-02-28 rasc    -b and counter corrected...
   -- 2001-03-13 rasc    result header output routine  -H <n>
   -- 2001-04-12 rasc    Module init rewritten

*/


//** should really "compile" the -x format string for each index, which means
//   basically looking up the properties only once for each index.


/* Prints the final results of a search.
   2001-01-01  rasc  Standard is swish 1.x default output

   if option extended format string is set, an alternate
   userdefined result output is possible (format like strftime or printf)
   in this case -d (delimiter is obsolete)
     e.g. : -x "result: COUNT:%c \t URL:%u\n"
*/


/* $$$ Remark / ToDO:
   -- The code is a prototype and needs optimizing:
   -- format control string is parsed on each result entry. (very bad!)
   -- ToDO: build an "action array" from an initial parsing of fmt
   --       ctrl string.
   --       on each entry step thru this action output list
   --       seems to be simple, but has to be done.
   -- but for now: get this stuff running on the easy way. 
   -- (rasc 2000-12)
   $$$ 
*/





#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>

#include "swish.h"
#include "mem.h"
#include "swstring.h"
#include "merge.h"
#include "metanames.h"
#include "search.h"
#include "docprop.h"
#include "error.h"
#include "result_output.h"
#include "parse_conffile.h"  // for the fuzzy to string function


/* private module prototypes */

static void printExtResultEntry(SWISH * sw, FILE * f, char *fmt, RESULT * r);
static char *printResultControlChar(FILE * f, char *s);
static char *printTagAbbrevControl(SWISH * sw, FILE * f, char *s, RESULT * r);
static char *parsePropertyResultControl(char *s, char **propertyname, char **subfmt);
static void printPropertyResultControl(FILE * f, char *propname, char *subfmt, RESULT * r);
static void printStandardResultProperties(FILE *f, RESULT *r);

static struct ResultExtFmtStrList *addResultExtFormatStr(struct ResultExtFmtStrList *rp, char *name, char *fmtstr);


/*
** ----------------------------------------------
** 
**  Module management code starts here
**
** ----------------------------------------------
*/



/* 
  -- init structures for this module
*/

void    initModule_ResultOutput(SWISH * sw)
{
    struct MOD_ResultOutput *md;

    md = (struct MOD_ResultOutput *) emalloc(sizeof(struct MOD_ResultOutput));
    memset(md, 0, sizeof(struct MOD_ResultOutput));  

    sw->ResultOutput = md;

    md->resultextfmtlist = NULL;

    /* cmd options */
    md->extendedformat = NULL;  /* -x :cmd param  */
    md->stdResultFieldDelimiter = NULL; /* -d :old 1.x result output delimiter */

    return;
}


/* 
  -- release all wired memory for this module
  -- 2001-04-11 rasc
*/

void    freeModule_ResultOutput(SWISH * sw)
{
    struct MOD_ResultOutput *md = sw->ResultOutput;
    struct ResultExtFmtStrList *l,
           *ln;


    if (md->stdResultFieldDelimiter)
        efree(md->stdResultFieldDelimiter); /* -d :free swish 1.x delimiter */
    /* was not emalloc!# efree (md->extendedformat);               -x stuff */


    l = md->resultextfmtlist;   /* free ResultExtFormatName */
    while (l)
    {
        efree(l->name);
        efree(l->fmtstr);
        ln = l->next;
        efree(l);
        l = ln;
    }
    md->resultextfmtlist = NULL;


    /* Free display props arrays -- only used for -p list */

    /* First the common part to all the index files */
    if (md->propNameToDisplay)
    {
        int i;

        for( i=0; i < md->numPropertiesToDisplay; i++ )
            efree(md->propNameToDisplay[i]);

        efree(md->propNameToDisplay);
    }
    md->propNameToDisplay=NULL;

    if (md->propIDToDisplay)
    {
        int i;
        IndexFILE *indexf;
        for( i = 0, indexf = sw->indexlist; indexf; i++, indexf = indexf->next)
        {
            efree(md->propIDToDisplay[i]);
        }
        efree(md->propIDToDisplay);
    } 
    md->propIDToDisplay=NULL;

    md->numPropertiesToDisplay=0;
    md->currentMaxPropertiesToDisplay=0;

    /* free module data */
    efree(sw->ResultOutput);
    sw->ResultOutput = NULL;

    return;
}



/*
** ----------------------------------------------
** 
**  Module config code starts here
**
** ----------------------------------------------
*/


/*
 -- Config Directives
 -- Configuration directives for this Module
 -- return: 0/1 = none/config applied
*/

int     configModule_ResultOutput(SWISH * sw, StringList * sl)
{
    struct MOD_ResultOutput *md = sw->ResultOutput;
    char   *w0 = sl->word[0];
    int     retval = 1;



    /* $$$ this will not work unless swish is reading the config file also for search ... */

    if (strcasecmp(w0, "ResultExtFormatName") == 0)
    {                           /* 2001-02-15 rasc */
        /* ResultExt...   name  fmtstring */
        if (sl->n == 3)
        {
            md->resultextfmtlist = (struct ResultExtFmtStrList *) addResultExtFormatStr(md->resultextfmtlist, sl->word[1], sl->word[2]);
        }
        else
            progerr("%s: requires \"name\" \"fmtstr\"", w0);
    }
    else
    {
        retval = 0;             /* not a module directive */
    }

    return retval;
}





/*
** ----------------------------------------------
** 
**  Module code starts here
**
** ----------------------------------------------
*/



/*
   -- Init the print of result entry in extented output format.
   -- The parsed propertynames will be stored for result handling
   -- Only user properties will be stored.
   -- Routine has to be executed prior to search/result storing...
   -- (This behavior is for historic reasons and may change)
   -- ($$ this routine may build the print action list in the future...)
   2001-02-07   rasc
*/

void    initPrintExtResult(SWISH * sw, char *fmt)
{
    FILE   *f;
    char   *propname;
    char   *subfmt;

    f = (FILE *) NULL;          /* no output, just parsing!!! */


    while (*fmt)
    {                           /* loop fmt string */

        switch (*fmt)
        {

        case '%':              /* swish abbrevation controls */
            /* ignore (dummy param), because autoprop */
            fmt = printTagAbbrevControl(sw, f, fmt, NULL);
            break;

        case '<':
            /* -- Property - Control: read Property Tag  <name> */
            /* -- Save User PropertyNames for result handling   */
            // Oct 16, 2001 - moseley: Seem like this should lookup the property
            // and error if not found, plus, it should cache the propID to avoid lookups
            // when returning results.  Would parse the -x format for each index.
            fmt = parsePropertyResultControl(fmt, &propname, &subfmt);

            efree(subfmt);
            efree(propname);
            break;

        case '\\':             /* format controls */
            fmt = printResultControlChar(f, fmt);
            break;


        default:               /* a output character in fmt string */
            fmt++;
            break;
        }

    }

}





/* ------------------------------------------------------------ */




/*
  -- Output the resuult entries in the given order
  -- outputformat depends on some cmd opt settings
  This frees memory as it goes along, so this can't be called from the library.
*/

void    printSortedResults(RESULTS_OBJECT *results, int begin, int maxhits)
{
    SWISH  *sw = results->sw;
    struct MOD_ResultOutput *md = sw->ResultOutput;
    RESULT *r = NULL;
    FILE   *f_out;



    f_out = stdout;

    /* -b is begin +1 */
    if ( begin )
    {
        begin--;
        if ( begin < 0 )
            begin = 0;
    }


    /* Seek, and report errors if trying to seek past eof */

    if ( SwishSeekResult(results, begin) < 0 )
        SwishAbortLastError( results->sw );



    /* If maxhits = 0 then display all */
    if (maxhits <= 0)
        maxhits = -1;


    if ( md->extendedformat ) /* are we using -x for output? */
    {
        while ( (r = SwishNextResult(results)) && maxhits )
        {
            printExtResultEntry(sw, f_out, md->extendedformat, r);
            freefileinfo( &r->fi );
            if ( maxhits > 0)
                maxhits--;
        }
    }


    else /* not using -x and maybe -p (old style) */
    {
        char *format;
        char *delimiter;

        if ((delimiter = (md->stdResultFieldDelimiter)) )
        {
            format = emalloc( (3* strlen( delimiter )) + 100 );
            /* warning -- user data in a sprintf format string */
            sprintf( format, "%%r%s%%p%s%%t%s%%l", delimiter, delimiter, delimiter );
        }
        else
            format = estrdup( "%r %p \"%t\" %l" );


        while ( (r = SwishNextResult(results)) && maxhits )
        {
            printExtResultEntry(sw, f_out, format, r);
            printStandardResultProperties(f_out, r);  /* print any -p properties */
            freefileinfo( &r->fi );
            fprintf(f_out, "\n");

            if ( maxhits > 0)
                maxhits--;
        }

        efree( format );
    }

}





/*
   -- print a result entry in extented output format
   -- Format characters: see switch cases...
   -- f_out == NULL, use STDOUT
   -- fmt = output format
   -- count = current result record counter
   2001-01-01   rasc
*/

static void printExtResultEntry(SWISH * sw, FILE * f_out, char *fmt, RESULT * r)
{
    FILE   *f;
    char   *propname;
    char   *subfmt;


    f = (f_out) ? f_out : stdout;

    while (*fmt)
    {                           /* loop fmt string */

        switch (*fmt)
        {

        case '%':              /* swish abbrevation controls */
            fmt = printTagAbbrevControl(sw, f, fmt, r);
            break;

        case '<':
            /* Property - Control: read and print Property Tag  <name> */
            fmt = parsePropertyResultControl(fmt, &propname, &subfmt);
            printPropertyResultControl(f, propname, subfmt, r);
            efree(subfmt);
            efree(propname);
            break;

        case '\\':             /* print format controls */
            fmt = printResultControlChar(f, fmt);
            break;


        default:               /* just output the character in fmt string */
            if (f)
                fputc(*fmt, f);
            fmt++;
            break;
        }

    }


}







/*  -- parse print control and print it
    --  output on file <f>
    --  *s = "\....."
    -- return: string ptr to char after control sequence.
*/

static char *printResultControlChar(FILE * f, char *s)
{
    char    c,
           *se;

    if (*s != '\\')
        return s;

    c = charDecode_C_Escape(s, &se);
    if (f)
        fputc(c, f);
    return se;
}





/*  -- parse % control and print it
    --  in fact expand shortcut to fullnamed autoproperty tag
    --    output on file <f>, NULL = parse only mode
    --  *s = "%.....
    -- return: string ptr to char after control sequence.
*/

static char *printTagAbbrevControl(SWISH * sw, FILE * f, char *s, RESULT * r)
{
    char   *t;
    char    buf[MAXWORDLEN];

    if (*s != '%')
        return s;
    t = NULL;

    switch (*(++s))
    {
    case 'c':
        t = AUTOPROPERTY_REC_COUNT;
        break;
    case 'd':
        t = AUTOPROPERTY_SUMMARY;
        break;
    case 'D':
        t = AUTOPROPERTY_LASTMODIFIED;
        break;
    case 'I':
        t = AUTOPROPERTY_INDEXFILE;
        break;
    case 'p':
        t = AUTOPROPERTY_DOCPATH;
        break;
    case 'r':
        t = AUTOPROPERTY_RESULT_RANK;
        break;
    case 'l':
        t = AUTOPROPERTY_DOCSIZE;
        break;
    case 'S':
        t = AUTOPROPERTY_STARTPOS;
        break;
    case 't':
        t = AUTOPROPERTY_TITLE;
        break;

    case '%':
        if (f)
            fputc('%', f);
        break;
    default:
        progerr("Formatstring: unknown abbrev '%%%c'", *s);
        break;

    }

    if (t)
    {
        sprintf(buf, "<%s>", t); /* create <...> tag */
        if (f)
            printExtResultEntry(sw, f, buf, r);
        else
            initPrintExtResult(sw, buf); /* parse only ! */
    }
    return ++s;
}




/*  -- parse <tag fmt="..."> control
    --  *s = "<....." format control string
    --       possible subformat:  fmt="...", fmt=/..../, etc. 
    -- return: string ptr to char after control sequence.
    --         **propertyname = Tagname  (or NULL)
    --         **subfmt = NULL or subformat
*/

static char *parsePropertyResultControl(char *s, char **propertyname, char **subfmt)
{
    char   *s1;
    char    c;
    int     len;


    *propertyname = NULL;
    *subfmt = NULL;

    s = str_skip_ws(s);
    if (*s != '<')
        return s;
    s = str_skip_ws(++s);


    /* parse propertyname */

    s1 = s;
    while (*s)
    {                           /* read to end of propertyname */
        if ((*s == '>') || isspace((unsigned char) *s))
        {                       /* delim > or whitespace ? */
            break;              /* break on delim */
        }
        s++;
    }
    len = s - s1;
    *propertyname = (char *) emalloc(len + 1);
    strncpy(*propertyname, s1, len);
    *(*propertyname + len) = '\0';


    if (*s == '>')
        return ++s;             /* no fmt, return */
    s = str_skip_ws(s);


    /* parse optional fmt=<c>...<c>  e.g. fmt="..." */

    if (!strncmp(s, "fmt=", 4))
    {
        s += 4;                 /* skip "fmt="  */
        c = *(s++);             /* string delimiter */
        s1 = s;
        while (*s)
        {                       /* read to end of delim. char */
            if (*s == c)
            {                   /* c or \c */
                if (*(s - 1) != '\\')
                    break;      /* break on delim c */
            }
            s++;
        }

        len = s - s1;
        *subfmt = (char *) emalloc(len + 1);
        strncpy(*subfmt, s1, len);
        *(*subfmt + len) = '\0';
    }


    /* stupid "find end of tag" */

    while (*s && *s != '>')
        s++;
    if (*s == '>')
        s++;

    return s;
}




/*
  -- Print the result value of propertytag <name> on file <f>
  -- if a format is given use it (data type dependend)
  -- string and numeric types are using printfcontrols formatstrings
  -- date formats are using strftime fromat strings.
*/


static void printPropertyResultControl(FILE * f, char *propname, char *subfmt, RESULT * r)
{
    char   *fmt;
    PropValue *pv;
    char   *s;
    int     n;


    pv = getResultPropValue(r, propname, 0);

    /* If returning NULL then it's an invalid property name */
    if (!pv)
    {
	printf("(null)");
	return;
        /* or could just abort, but that's ugly in the middle of output */
        /* it would be nice to check the format strings (and cache the meta name lookups) */
        /* before generating resuls, but need to do that for each index */
        printf("\n"); /* might be in the middle of some text */
        SwishAbortLastError( r->db_results->indexf->sw );
    }


#ifdef USE_DOCPATH_AS_TITLE
    if ( ( PROP_UNDEFINED == pv->datatype ) && strcmp( AUTOPROPERTY_TITLE, propname ) == 0 )
    {
        freeResultPropValue( pv );

        pv = getResultPropValue(r, AUTOPROPERTY_DOCPATH, 0);

        if ( !pv )  /* in this case, let it slide */
            return;

        /* Just display the base name */
        if ( PROP_STRING == pv->datatype )
        {
            char *c = estrdup( str_basename( pv->value.v_str ) );
            efree( pv->value.v_str );
            pv->value.v_str = c;
        }
    }
#endif



    switch (pv->datatype)
    {
        /* use passed or default fmt */

    case PROP_INTEGER:
        fmt = (subfmt) ? subfmt : "%d";
        if (f)
            fprintf(f, fmt, pv->value.v_int);
        break;


    case PROP_ULONG:
        fmt = (subfmt) ? subfmt : "%lu";
        if (f)
            fprintf(f, fmt, pv->value.v_ulong);
        break;


    case PROP_STRING:
        fmt = (subfmt) ? subfmt : "%s";

        /* -- get rid of \n\r in string! */  // there shouldn't be any in the first place, I believe
        for (s = pv->value.v_str; *s; s++)
        {
            if ('\t' != *s && isspace((unsigned char) *s))
                *s = ' ';
        }

        /* $$$ ToDo: escaping of delimiter characters  $$$ */
        /* $$$ Also ToDo, escapeHTML entities (need config directive) */

        if (f)
            fprintf(f, fmt, (char *) pv->value.v_str);

        break;


    case PROP_DATE:
        fmt = (subfmt) ? subfmt : DATE_FORMAT_STRING;
        if (!strcmp(fmt, "%ld"))
        {
            /* special: Print date as serial int (for Bill) */
            if (f)
                fprintf(f, fmt, (long) pv->value.v_date);
        }
        else
        {
            /* fmt is strftime format control! */
            s = (char *) emalloc(MAXWORDLEN + 1);
            n = strftime(s, (size_t) MAXWORDLEN, fmt, localtime(&(pv->value.v_date)));
            if (n && f)
                fprintf(f, s);
            efree(s);
        }
        break;

    case PROP_FLOAT:
        fmt = (subfmt) ? subfmt : "%f";
        if (f)
            fprintf(f, fmt, (double) pv->value.v_float);
        break;

    case PROP_UNDEFINED:
        break;  /* Do nothing */

    default:
        progerr("Swish-e database error.  Unknown property type accessing property '%s'", propname);
        break;

    }


    freeResultPropValue(pv);
}



/*
  -------------------------------------------
  Result config stuff
  -------------------------------------------
*/


/*
  -- some code for  -x fmtByName:
  -- e.g.  ResultExtendedFormat   myformat   "<swishtitle>|....\n"
  --       ResultExtendedFormat   yourformat "%c|%t|%p|<author fmt=/%20s/>\n"
  --
  --    swish -w ... -x myformat ...
  --
  --  2001-02-15 rasc
*/


/*
   -- add name and string to list 
*/

static struct ResultExtFmtStrList *addResultExtFormatStr(struct ResultExtFmtStrList *rp, char *name, char *fmtstr)
{
    struct ResultExtFmtStrList *newnode;


    newnode = (struct ResultExtFmtStrList *) emalloc(sizeof(struct ResultExtFmtStrList));

    newnode->name = (char *) estrdup(name);
    newnode->fmtstr = (char *) estrdup(fmtstr);

    newnode->next = NULL;

    if (rp == NULL)
        rp = newnode;
    else
        rp->nodep->next = newnode;

    rp->nodep = newnode;
    return rp;
}



/* 
   -- check if name is a predefined format
   -- case sensitive
   -- return fmtstring for formatname or NULL
*/


char   *hasResultExtFmtStr(SWISH * sw, char *name)
{
    struct ResultExtFmtStrList *rfl;

    rfl = sw->ResultOutput->resultextfmtlist;
    if (!rfl)
        return (char *) NULL;

    while (rfl)
    {
        if (!strcmp(name, rfl->name))
            return rfl->fmtstr;
        rfl = rfl->next;
    }

    return (char *) NULL;
}





/*
  -------------------------------------------
  result header stuff
  -------------------------------------------
*/

/* $$$ result header stuff should be moved to swish.c (or a module that is not part of the library). */



/*
  -- print a line for the result output header
  -- the verbose level is checked for output
  -- <min_verbose> has to be >= sw->...headerOutVerbose
  -- outherwise nothing is outputted
  -- return: 0/1  (not printed/printed)
  -- 2001-03-13  rasc
*/

int     resultHeaderOut(SWISH * sw, int min_verbose, char *printfmt, ...)
{
    va_list args;

    /* min_verbose to low, no output */
    if (min_verbose > sw->headerOutVerbose)
        return 0;

    /* print header info... */
    va_start(args, printfmt);
    vfprintf(stdout, printfmt, args);
    va_end(args);
    return 1;
}








/*******************************************************************
*   Displays the "old" style properties for -p
*
*   Call with:
*       *RESULT
*
*   I think this could be done in result_output.c by creating a standard
*   -x format (plus properites) for use when there isn't one already,
xxxx
*
*
********************************************************************/
static void printStandardResultProperties(FILE *f, RESULT *r)
{
    int     i;
    IndexFILE *tmp, *indexf = r->db_results->indexf;
    SWISH  *sw = indexf->sw;
    struct  MOD_ResultOutput *md = sw->ResultOutput;
    char   *s;
    char   *propValue;
    int    *metaIDs = NULL;


    if (md->numPropertiesToDisplay == 0)
        return;

    for( i = 0, tmp = sw->indexlist; tmp ; i++, tmp = tmp->next )
    {
        if(tmp == indexf)
        {
           metaIDs = md->propIDToDisplay[i];
           break;
        }
    }

    if(!metaIDs)
        return;

    for ( i = 0; i < md->numPropertiesToDisplay; i++ )
    {
        propValue = s = getResultPropAsString( r, metaIDs[ i ] );

        if (sw->ResultOutput->stdResultFieldDelimiter)
            fprintf(f, "%s", sw->ResultOutput->stdResultFieldDelimiter);
        else
            fprintf(f, " \"");	/* default is to quote the string, with leading space */

        /* print value, handling newlines and quotes */
        while (*propValue)
        {
            /* no longer check for double-quote.  User should pick a good value for -d or use -x */

            if (*propValue == '\n')
                fprintf(f, " ");

            else
                fprintf(f,"%c", *propValue);

            propValue++;
        }

        //fprintf(f,"%s", propValue);

        if (!sw->ResultOutput->stdResultFieldDelimiter)
            fprintf(f,"\"");	/* default is to quote the string */

        efree( s );            
    }
}




void addSearchResultDisplayProperty(SWISH *sw, char *propName)
{
    struct  MOD_ResultOutput *md = sw->ResultOutput;

	/* add a property to the list of properties that will be displayed */
	if (md->numPropertiesToDisplay >= md->currentMaxPropertiesToDisplay)
	{
		if(md->currentMaxPropertiesToDisplay) {
			md->currentMaxPropertiesToDisplay+=2;
			md->propNameToDisplay=(char **)erealloc(md->propNameToDisplay,md->currentMaxPropertiesToDisplay*sizeof(char *));
		} else {
			md->currentMaxPropertiesToDisplay=5;
			md->propNameToDisplay=(char **)emalloc(md->currentMaxPropertiesToDisplay*sizeof(char *));
		}
	}
	md->propNameToDisplay[md->numPropertiesToDisplay++] = estrdup(propName);
}





/* For faster proccess, get de ID of the properties to sort */
int initSearchResultProperties(SWISH *sw)
{
    IndexFILE *indexf;
    int i, j, index_count;
    struct MOD_ResultOutput *md = sw->ResultOutput;
    struct metaEntry *meta_entry;


	/* lookup selected property names */

	if (md->numPropertiesToDisplay == 0)
		return RC_OK;

	/* get number of index files */
	for( index_count = 0, indexf = sw->indexlist; indexf; index_count++, indexf = indexf->next );

	md->propIDToDisplay = (int **) emalloc(index_count * sizeof(int *));

	for( i = 0 ,indexf = sw->indexlist; i < index_count; i++, indexf = indexf->next )
		md->propIDToDisplay[i]=(int *) emalloc(md->numPropertiesToDisplay*sizeof(int));

	for (i = 0; i < md->numPropertiesToDisplay; i++)
	{
		makeItLow(md->propNameToDisplay[i]);

		/* Get ID for each index file */
		for( j = 0, indexf = sw->indexlist; j < index_count; j++, indexf = indexf->next )
		{
		    if ( !(meta_entry = getPropNameByName( &indexf->header, md->propNameToDisplay[i])))
			{
				progerr ("Unknown Display property name \"%s\"", md->propNameToDisplay[i]);
				return (sw->lasterror=UNKNOWN_PROPERTY_NAME_IN_SEARCH_DISPLAY);
			}
			else
			    md->propIDToDisplay[j][i] = meta_entry->metaID;
		}
	}
	return RC_OK;
}



