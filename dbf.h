#ifndef _DBF_H
#define _DBF_H

#include <tcl.h>

/*----------------------------------------------------------------------*\
 | Windows needs to know which symbols to export.  Unix does not.		|
 | BUILD_dbf should be undefined for Unix.								|
\*----------------------------------------------------------------------*/

#ifdef BUILD_dbf
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLEXPORT
#endif /* BUILD_dbf */

/* Only the _Init function is exported */

EXTERN int	Dbf_Init _ANSI_ARGS_((Tcl_Interp * interp));

#endif /* _DBF_H */
