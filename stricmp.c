/*----------------------------------------------------------------------*\
 | Some systems don't have these functions.								|
\*----------------------------------------------------------------------*/

#include <ctype.h>
#include "stricmp.h"

#ifndef HAVE_STRICMP

int stricmp (const char *s, const char *t) {
	int d = 0;
	do {
		d = toupper(*s) - toupper(*t);
		} while (*s++ && *t++ && !d);
	return (d);
	}

#endif

#ifndef HAVE_MEMICMP

int memicmp (const void *ss, const void *tt, size_t n) {
	const char *s = (const char *) ss;
	const char *t = (const char *) tt;
	int d = 0;
	while (n-- > 0)
		if (d = toupper(*s++) - toupper(*t++)) break;
	return (d);
	}

#endif

/*----------------------------------------------------------------------*\
\*----------------------------------------------------------------------*/
