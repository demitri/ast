#if !defined( CMPREGION_INCLUDED ) /* Include this file only once */
#define CMPREGION_INCLUDED
/*
*+
*  Name:
*     cmpregion.h

*  Type:
*     C include file.

*  Purpose:
*     Define the interface to the CmpRegion class.

*  Invocation:
*     #include "cmpregion.h"

*  Description:
*     This include file defines the interface to the CmpRegion class and
*     provides the type definitions, function prototypes and macros,
*     etc.  needed to use this class.
*
*     The CmpRegion class implement a Region which represents a simple interval
*     on each axis of the encapsulated Frame

*  Inheritance:
*     The CmpRegion class inherits from the Region class.

*  Feature Test Macros:
*     astCLASS
*        If the astCLASS macro is undefined, only public symbols are
*        made available, otherwise protected symbols (for use in other
*        class implementations) are defined. This macro also affects
*        the reporting of error context information, which is only
*        provided for external calls to the AST library.

*  Copyright:
*     <COPYRIGHT_STATEMENT>

*  Authors:
*     DSB: David S. Berry (Starlink)

*  History:
*     11-OCT-2004 (DSB):
*        Original version.
*-
*/

/* Include files. */
/* ============== */
/* Interface definitions. */
/* ---------------------- */
#include "region.h"              /* Coordinate regions (parent class) */

#if defined(astCLASS)            /* Protected */
#include "channel.h"             /* I/O channels */
#endif

/* C header files. */
/* --------------- */
#if defined(astCLASS)            /* Protected */
#include <stddef.h>
#endif

/* Macros. */
/* ------- */
/* Boolean operators */
#define AST__AND 1
#define AST__OR 2

/* Type Definitions. */
/* ================= */
/* CmpRegion structure. */
/* ------------------ */
/* This structure contains all information that is unique to each object in
   the class (e.g. its instance variables). */
typedef struct AstCmpRegion {

/* Attributes inherited from the parent class. */
   AstRegion region;             /* Parent class structure */

/* Attributes specific to objects in this class. */
   AstRegion *region1;           /* First component Region */
   AstRegion *region2;           /* Second component Region */
   int oper;                     /* Boolean operator */

} AstCmpRegion;

/* Virtual function table. */
/* ----------------------- */
/* This table contains all information that is the same for all
   objects in the class (e.g. pointers to its virtual functions). */
#if defined(astCLASS)            /* Protected */
typedef struct AstCmpRegionVtab {

/* Properties (e.g. methods) inherited from the parent class. */
   AstRegionVtab region_vtab;    /* Parent class virtual function table */

/* Unique flag value to determine class membership. */
   int *check;                   /* Check value */

/* Properties (e.g. methods) specific to this class. */
} AstCmpRegionVtab;
#endif

/* Function prototypes. */
/* ==================== */
/* Prototypes for standard class functions. */
/* ---------------------------------------- */
astPROTO_CHECK(CmpRegion)          /* Check class membership */
astPROTO_ISA(CmpRegion)            /* Test class membership */

/* Constructor. */
#if defined(astCLASS)            /* Protected. */
AstCmpRegion *astCmpRegion_( void *, void *, int, const char *, ... );
#else
AstCmpRegion *astCmpRegionId_( void *, void *, int, const char *, ... );
#endif

#if defined(astCLASS)            /* Protected */

/* Initialiser. */
AstCmpRegion *astInitCmpRegion_( void *, size_t, int, AstCmpRegionVtab *,
                     const char *, AstRegion *, AstRegion *, int );

/* Vtab initialiser. */
void astInitCmpRegionVtab_( AstCmpRegionVtab *, const char * );

/* Loader. */
AstCmpRegion *astLoadCmpRegion_( void *, size_t, AstCmpRegionVtab *,
                                 const char *, AstChannel * );

#endif

/* Prototypes for member functions. */
/* -------------------------------- */
# if defined(astCLASS)           /* Protected */
#endif

/* Function interfaces. */
/* ==================== */
/* These macros are wrap-ups for the functions defined by this class
   to make them easier to invoke (e.g. to avoid type mis-matches when
   passing pointers to objects from derived classes). */

/* Interfaces to standard class functions. */
/* --------------------------------------- */
/* Some of these functions provide validation, so we cannot use them
   to validate their own arguments. We must use a cast when passing
   object pointers (so that they can accept objects from derived
   classes). */

/* Check class membership. */
#define astCheckCmpRegion(this) astINVOKE_CHECK(CmpRegion,this)

/* Test class membership. */
#define astIsACmpRegion(this) astINVOKE_ISA(CmpRegion,this)

/* Constructor. */
#if defined(astCLASS)            /* Protected. */
#define astCmpRegion astINVOKE(F,astCmpRegion_)
#else
#define astCmpRegion astINVOKE(F,astCmpRegionId_)
#endif

#if defined(astCLASS)            /* Protected */

/* Initialiser. */
#define astInitCmpRegion(mem,size,init,vtab,name,reg1,reg2,oper) \
astINVOKE(O,astInitCmpRegion_(mem,size,init,vtab,name,reg1,reg2,oper))

/* Vtab Initialiser. */
#define astInitCmpRegionVtab(vtab,name) astINVOKE(V,astInitCmpRegionVtab_(vtab,name))
/* Loader. */
#define astLoadCmpRegion(mem,size,vtab,name,channel) \
astINVOKE(O,astLoadCmpRegion_(mem,size,vtab,name,astCheckChannel(channel)))
#endif

/* Interfaces to public member functions. */
/* -------------------------------------- */
/* Here we make use of astCheckCmpRegion to validate CmpRegion pointers
   before use.  This provides a contextual error report if a pointer
   to the wrong sort of Object is supplied. */

#if defined(astCLASS)            /* Protected */
#endif
#endif