/*
$Id: result_output.h 1736 2005-05-12 15:41:22Z karman $
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


**
** 2001-01  R. Scherg  (rasc)   initial coding
**
*/


#ifndef __HasSeenModule_ResultOutput
#define __HasSeenModule_ResultOutput	1


/*
   -- module data
*/

struct ResultExtFmtStrList
{                               /* -x extended format by defined names */
    char   *name;
    char   *fmtstr;
    struct ResultExtFmtStrList *next;
    struct ResultExtFmtStrList *nodep;

};



/*
   -- global module data structure
*/

struct MOD_ResultOutput {
   /* public:  */
   /* private: don't use outside this module! */
                                /* -x extended format by defined names */
    char   *extendedformat;     /* -x "fmt", holds fmt or NULL */
    char   *stdResultFieldDelimiter; /* -d <c> delimiter , (def: config.h) v1.x output style */

    /* ResultExtendedFormat predefined List see: -x */
    struct ResultExtFmtStrList *resultextfmtlist;

    int     numPropertiesToDisplay;
    int     currentMaxPropertiesToDisplay;
    char  **propNameToDisplay;
    int   **propIDToDisplay;
};


void addSearchResultDisplayProperty (SWISH *, char* );



void initModule_ResultOutput (SWISH *sw);
void freeModule_ResultOutput (SWISH *sw);
int configModule_ResultOutput (SWISH *sw, StringList *sl);


void initPrintExtResult (SWISH *sw, char *fmt);

void printSortedResults(RESULTS_OBJECT *results, int begin, int maxhits);

int initSearchResultProperties(SWISH *sw);

char *hasResultExtFmtStr (SWISH *sw, char *name);


int resultHeaderOut (SWISH *sw, int min_verbose, char *prtfmt, ...);
void resultPrintHeader (SWISH *sw, int min_verbose, INDEXDATAHEADER *h, 
				char *pathname, int merged);


#endif

