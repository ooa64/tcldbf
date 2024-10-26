/*----------------------------------------------------------------------*\
 | Some systems don't have these functions.								|
\*----------------------------------------------------------------------*/

#include <stddef.h>

#ifdef _WIN32
	#define HAVE_STRICMP
	#define HAVE_MEMICMP
#endif

#ifndef HAVE_STRICMP
extern int stricmp (const char *s, const char *t);
#endif

#ifndef HAVE_MEMICMP
extern int memicmp (const void *s, const void *t, size_t n);
#endif

/*----------------------------------------------------------------------*\
\*----------------------------------------------------------------------*/
