/*
*class++
*  Name:
*     Interval

*  Purpose:
*     A region representing an interval on one or more axes of a Frame.

*  Constructor Function:
c     astInterval
f     AST_INTERVAL

*  Description:
*     The Interval class implements a Region which represents upper
*     and/or lower limits on one or more axes of a Frame. For a point to
*     be within the region represented by the Interval, the point must
*     satisfy all the restrictions placed on all the axes. The point is
*     outside the region if it fails to satisfy any one of the restrictions.
*     Each axis may have either an upper limit, a lower limit, both or
*     neither. If both limits are supplied but are in reverse order (so
*     that the lower limit is greater than the upper limit), then the
*     interval is an excluded interval, rather than an included interval.

*  Inheritance:
*     The Interval class inherits from the Region class.

*  Attributes:
*     The Interval class does not define any new attributes beyond
*     those which are applicable to all Regions.

*  Functions:
c     The Interval class does not define any new functions beyond those
f     The Interval class does not define any new routines beyond those
*     which are applicable to all Regions.

*  Copyright:
*     <COPYRIGHT_STATEMENT>

*  Authors:
*     DSB: David S. Berry (Starlink)

*  History:
*     29-OCT-2004 (DSB):
*        Original version.
*class--
*/

/* Module Macros. */
/* ============== */
/* Set the name of the class we are implementing. This indicates to
   the header files that define class interfaces that they should make
   "protected" symbols available. */
#define astCLASS Interval

/* Macros which return the maximum and minimum of two values. */
#define MAX(aa,bb) ((aa)>(bb)?(aa):(bb))
#define MIN(aa,bb) ((aa)<(bb)?(aa):(bb))

/* Macro to check for equality of floating point values. We cannot
   compare bad values directory because of the danger of floating point
   exceptions, so bad values are dealt with explicitly. */
#define EQUAL(aa,bb) (((aa)==AST__BAD)?(((bb)==AST__BAD)?1:0):(((bb)==AST__BAD)?0:(fabs((aa)-(bb))<=1.0E9*MAX((fabs(aa)+fabs(bb))*DBL_EPSILON,DBL_MIN))))

/* Include files. */
/* ============== */
/* Interface definitions. */
/* ---------------------- */
#include "error.h"               /* Error reporting facilities */
#include "memory.h"              /* Memory allocation facilities */
#include "object.h"              /* Base Object class */
#include "pointset.h"            /* Sets of points/coordinates */
#include "region.h"              /* Abstract coordinate regions (parent class) */
#include "channel.h"             /* I/O channels */
#include "box.h"                 /* Box Regions */
#include "nullregion.h"          /* Null Regions */
#include "wcsmap.h"              /* Definitons of AST__DPI etc */
#include "interval.h"            /* Interface definition for this class */
#include "ellipse.h"             /* Interface definition for ellipse class */
#include "mapping.h"             /* Position mappings */
#include "unitmap.h"             /* Unit Mappings */
#include "cmpmap.h"              /* Compound Mappings */

/* Error code definitions. */
/* ----------------------- */
#include "ast_err.h"             /* AST error codes */

/* C header files. */
/* --------------- */
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Module Variables. */
/* ================= */
/* Define the class virtual function table and its initialisation flag
   as static variables. */
static AstIntervalVtab class_vtab;    /* Virtual function table */
static int class_init = 0;       /* Virtual function table initialised? */

/* Pointers to parent class methods which are extended by this class. */
static AstPointSet *(* parent_transform)( AstMapping *, AstPointSet *, int, AstPointSet * );
static AstMapping *(* parent_simplify)( AstMapping * );
static int (* parent_overlap)( AstRegion *, AstRegion * );

/* External Interface Function Prototypes. */
/* ======================================= */
/* The following functions have public prototypes only (i.e. no
   protected prototypes), so we must provide local prototypes for use
   within this module. */
AstInterval *astIntervalId_( void *, const double[], const double[], void *, const char *, ... );

/* Prototypes for Private Member Functions. */
/* ======================================== */
static AstMapping *Simplify( AstMapping * );
static AstPointSet *BndBaseMesh( AstRegion *, double *, double * );
static AstPointSet *RegBaseMesh( AstRegion * );
static AstPointSet *Transform( AstMapping *, AstPointSet *, int, AstPointSet * );
static AstRegion *GetDefUnc( AstRegion * );
static int GetBounded( AstRegion * );
static int Overlap( AstRegion *, AstRegion * );
static int RegPins( AstRegion *, AstPointSet *, AstRegion *, int ** );
static void Cache( AstInterval * );
static void Copy( const AstObject *, AstObject * );
static void Delete( AstObject * );
static void Dump( AstObject *, AstChannel * );
static void RegBaseBox( AstRegion *this, double *, double * );

/* Member functions. */
/* ================= */

static AstPointSet *BndBaseMesh( AstRegion *this, double *lbnd, double *ubnd ){
/*
*  Name:
*     BndBaseMesh

*  Purpose:
*     Return a PointSet containing points spread around part of the boundary 
*     of a Region.

*  Type:
*     Private function.

*  Synopsis:
*     #include "interval.h"
*     AstPointSet *BndBaseMesh( AstRegion *this, double *lbnd, double *ubnd )

*  Class Membership:
*     Interval method (over-rides the astBndBaseMesh method inherited from
*     the Region class).

*  Description:
*     This function returns a PointSet containing a set of points on the
*     boundary of the intersection between the supplied Region and the
*     supplied box. The points refer to the base Frame of the 
*     encapsulated FrameSet. If the boundary of the supplied Region does
*     not intersect the supplied box, then a PointSet containing a single 
*     bad point is returned.

*  Parameters:
*     this
*        Pointer to the Region.
*     lbnd
*        Pointer to an array holding the lower limits of the axis values 
*        within the required box.
*     ubnd
*        Pointer to an array holding the upper limits of the axis values 
*        within the required box.

*  Returned Value:
*     Pointer to the PointSet. The axis values in this PointSet will have 
*     associated accuracies derived from the uncertainties which were
*     supplied when the Region was created.
*     
*    If the Region does not intersect the supplied box, the returned
*    PointSet will contain a single point with a value of AST__BAD on
*    every axis.

*  Notes:
*    - A NULL pointer is returned if an error has already occurred, or if
*    this function should fail for any reason.
*/

/* Local Variables: */
   AstBox *box;
   AstPointSet *mesh1;  
   AstPointSet *pset;  
   AstPointSet *result;  
   double **ptr;
   double *lbnd_new;
   double *p;
   double *ubnd_new;
   double lb;
   double ub;
   int allGood;
   int ic;
   int ip;
   int nc;
   int np;
   int overlap;             

/* Initialise */
   result = NULL;

/* Check the local error status. */
   if ( !astOK ) return result;

/* If the Interval is effectively a Box, invoke the astBndBaseMesh
   function on the equivalent Box. A pointer to the equivalent Box will
   be stored in the Interval structure. */
   box = ((AstInterval *) this)->box;
   if( box ) {
      result = astBndBaseMesh( box, lbnd, ubnd );

/* If the Interval is not equivalent to a Box... */
   } else {

/* Create a Box with the supplied bounds. */
      box = astBox( this, 1, lbnd, ubnd, NULL, "" );

/* Get a mesh covering this box. */
      mesh1 = astRegBaseMesh( box );

/* Transform this mesh using this Interval as a Mapping. Mesh points
   outside the Interval will be set bad in the returned PointSet. */
      pset = astTransform( this, mesh1, 1, NULL );

/* Find the bounding box of the good points in the returned PoinSet. */
      np = astGetNpoint( pset );
      nc = astGetNcoord( pset );
      ptr = astGetPoints( pset );
      lbnd_new = astMalloc( sizeof( double )*(size_t) nc );
      ubnd_new = astMalloc( sizeof( double )*(size_t) nc );
      if( astOK ) {
         allGood = 1;
         overlap = 1;
         for( ic = 0; ic < nc; ic++ ) {
            lb = DBL_MAX;
            ub = -DBL_MAX;
            p = ptr[ ic ];
            for( ip = 0; ip < np; ip++, p++ ) {
               if( *p == AST__BAD ) {
                  allGood = 0;
               } else {
                  if( *p < lb ) lb = *p;               
                  if( *p > ub ) ub = *p;               
               }
            }
            if( lb > ub ) {
               overlap = 0;
               break;
            } else {
               lbnd_new[ ic ] = lb;         
               ubnd_new[ ic ] = ub;         
            }
         }

/* If the boundary of the supplied Region does not intersect the box,
   return a PointSet containing a single bad position. */
         if( !overlap || allGood ) {
            result = astPointSet( 1, nc, "" );
            ptr = astGetPoints( result );
            if( astOK ) {
               for( ic = 0; ic < nc; ic++ ) ptr[ ic ][ 0 ] = AST__BAD;
            }

/* Otherwise, create another Box with the bounds found above, and create
   a mesh covering this new Box. */
         } else {
            astAnnul( box );
            box = astBox( this, 1, lbnd_new, ubnd_new, NULL, "" );
            result = astRegBaseMesh( box );
         }
      }

/* Free resources. */
      box = astAnnul( box );
      mesh1 = astAnnul( mesh1 );
      pset = astAnnul( pset );
      lbnd_new = astFree( lbnd_new );
      ubnd_new = astFree( ubnd_new );
   }

/* Return NULL if an error occurred. */
   if( !astOK ) result = astAnnul( result );

/* Return the required pointer. */
   return result;
}

static void Cache( AstInterval *this ){
/*
*  Name:
*     Cache

*  Purpose:
*     Calculate intermediate values and cache them in the Interval structure.

*  Type:
*     Private function.

*  Synopsis:
*     #include "interval.h"
*     void Cache( AstInterval *this )

*  Class Membership:
*     Interval member function 

*  Description:
*     This function uses the PointSet stored in the parent Region to calculate 
*     some intermediate values which are useful in other methods. These
*     values are stored within the Interval structure.

*  Parameters:
*     this
*        Pointer to the Interval.

*/

/* Local Variables: */
   AstFrame *bfrm;          /* Interval base Frame */
   AstFrame *cfrm;          /* Interval current Frame */
   AstBox *bbox;            /* Equivalent base Box */
   AstRegion *map;          /* Interval base->current Mapping */
   AstRegion *reg;          /* Pointer to this Region structure */
   AstRegion *unc;          /* Pointer to uncertainty Region */
   double **ptr;            /* Pointer to data holding all axis limits */
   double *lbnd;            /* Pointer to array of lower axis limits */
   double *ubnd;            /* Pointer to array of upper axis limits */
   int i;                   /* Axis index */
   int isBox;               /* Is this Interval equivalent to a Box? */
   int nc;                  /* Number of base Frame axes */
   int neg;                 /* Is the equivalent Box negated? */

/* Check the global error status. */
   if ( !astOK ) return;

/* Get a pointer to the Region structure */
   reg = (AstRegion *) this;

/* The Interval structure contains a pointer to an equivalent Box
   structure. This Box structure is created below if the Interval is
   equivalent to a Box. Initialise the pointer now to indicate that the
   Interval is not equivalent to a Box. */
   this->box = NULL;

/* Get the number of axes in the base Frame of the FrameSet encapsulated
   by the parent Region structure. */
   nc = astGetNin( reg->frameset );

/* Get a pointer to the array holding the axis limits held in the PointSet 
   encapsulated in the parent Region structure. */
   ptr = astGetPoints( reg->points );

/* Allocate memory to hold the limits organised per point rather than per
   axis. */
   lbnd = astMalloc( sizeof( double )*(size_t)nc );
   ubnd = astMalloc( sizeof( double )*(size_t)nc );

/* Check these pointers can be used safely. */
   if( astOK ) {

/* See if the Interval is effectively a (possibly negated) Box. Assume it
   is to begin with.  */
      isBox = 1;

/* Initialisation to prevent compiler warnings. */
      neg = 0;   

/* Check the limits on every axis. */
      for( i = 0; i < nc; i++ ) {

/* Copy the axis limits into the allocated arrays (these are needed by the
   Box constructor later on). */
         lbnd[ i ] = ptr[ i ][ 0 ];
         ubnd[ i ] = ptr[ i ][ 1 ];

/* The Interval is not a Box if any axis limit is missing. In this case
   use -DBL_MAX or +DBL_MAX as the limit to be stored in the Interval
   structure. */
         if( lbnd[ i ] == AST__BAD ) {
            isBox = 0;
            lbnd[ i ] = -DBL_MAX;
         }

         if( ubnd[ i ] == AST__BAD ) {
            isBox = 0;
            ubnd[ i ] = DBL_MAX;
         }

/* If this is the first axis, note if the axis interval is included or
   excluded. This is determined by whether the "lower limit" is greater
   than or less than the "upper limit". If the axis interval is excluded 
   (lower limit greater than upper limit), then any equivalent Box will be 
   a negated Box (i.e. will represent the outside of a box rather than 
   the inside). */
         if( i == 0 ){
            neg = ( lbnd[ i ] > ubnd[ i ]  );

/* The Interval is not a Box if the limits for this axis are not the same
   way round as those of the first axis. */
         } else {
   
            if( neg ) {
               if( lbnd[ i ] < ubnd[ i ]  ) isBox = 0;
            } else {
               if( lbnd[ i ] > ubnd[ i ]  ) isBox = 0;
            }

         }
      }

/* If the Interval is effectively an unnegated Box, create the equivalent Box, 
   and store a pointer to it in the Interval structure. */
      if( isBox && !neg ) {
         bfrm = astGetFrame( reg->frameset, AST__BASE );
         cfrm = astGetFrame( reg->frameset, AST__BASE );
         map = astGetMapping( reg->frameset, AST__BASE, AST__CURRENT );
         unc = astTestUnc( reg ) ? astGetUnc( reg, AST__BASE ) : NULL;

         bbox = astBox( bfrm, 1, lbnd, ubnd, unc, "" );
         if( astIsAUnitMap( map ) ){
            this->box = astClone( bbox );
         } else {
            this->box = astMapRegion( bbox, map, cfrm );
         }

         if( unc ) unc = astAnnul( unc );
         bfrm = astAnnul( bfrm );
         cfrm = astAnnul( cfrm );
         map = astAnnul( map );
         bbox = astAnnul( bbox );

      }

/* Store the axis limits in the Interval structure. */
      this->lbnd = lbnd;
      this->ubnd = ubnd;

   }
}

static int GetBounded( AstRegion *this ) {
/*
*  Name:
*     GetBounded

*  Purpose:
*     Is the Region bounded?

*  Type:
*     Private function.

*  Synopsis:
*     #include "interval.h"
*     int GetBounded( AstRegion *this ) 

*  Class Membership:
*     Interval method (over-rides the astGetBounded method inherited from
*     the Region class).

*  Description:
*     This function returns a flag indicating if the Region is bounded.
*     The implementation provided by the base Region class is suitable
*     for Region sub-classes representing the inside of a single closed 
*     curve (e.g. Circle, Interval, Box, etc). Other sub-classes (such as
*     CmpRegion, PointList, etc ) may need to provide their own
*     implementations.

*  Parameters:
*     this
*        Pointer to the Region.

*  Returned Value:
*     Non-zero if the Region is bounded. Zero otherwise.

*/

/* Local Variables: */
   int result;                /* Returned result */

/* Initialise */
   result = 0;

/* Check the global error status. */
   if ( !astOK ) return result;

/* The unnegated Interval is bounded only if there is an equivalent Box
   structure stored in the Interval structure. */
   if( ((AstInterval *) this)->box ) result = 1;

/* Return the required pointer. */
   return result;
}

static AstRegion *GetDefUnc( AstRegion *this_region ) {
/*
*  Name:
*     GetDefUnc

*  Purpose:
*     Obtain a pointer to the default uncertainty Region for a given Region.

*  Type:
*     Private function.

*  Synopsis:
*     #include "interval.h"
*     AstRegion *GetDefUnc( AstRegion *this ) 

*  Class Membership:
*     Interval member function (over-rides the astGetDefUnc protected
*     method inherited from the Region class).

*  Description:
*     This function returns a pointer to a Region which represents the
*     default uncertainty associated with a position on the boundary of the 
*     given Region. The returned Region refers to the base Frame within the 
*     FrameSet encapsulated by the supplied Region.

*  Parameters:
*     this
*        Pointer to the Region.

*  Returned Value:
*     A pointer to the Region. This should be annulled (using astAnnul)
*     when no longer needed.

*  Notes:
*     - A NULL pointer will be returned if this function is invoked
*     with the global error status set, or if it should fail for any
*     reason.
*/

/* Local Variables: */
   AstInterval *this;         /* Pointer to Interval structure */
   AstFrame *bfrm;            /* Base Frame of supplied Region */
   AstRegion *result;         /* Returned pointer */
   double *lbnd;              /* Ptr. to array holding axis lower bounds */
   double *ubnd;              /* Ptr. to array holding axis upper bounds */
   int i;                     /* Axis index */
   int nax;                   /* Number of base Frame axes */

/* Initialise */
   result = NULL;

/* Check the global error status. */
   if ( !astOK ) return result;

/* Get a pointer to the Interval structure. */
   this = (AstInterval *) this_region;

/* If this Interval is equivalent to a Box, get the default uncertainty
   for the equivalent Box and return it. */
   if( this->box ) {
      result = astGetDefUnc( this->box );

/* Otherwise, we use a box covering 1.0E-6 of each axis interval, centred on 
   the origin. */
   } else {

/* Get a pointer to the base Frame. */
      bfrm = astGetFrame( this_region->frameset, AST__BASE );

/* Get the number of base Frame axes. */
      nax = astGetNaxes( bfrm );

/* Allocate arrays to hold the bounds of the uncertainty Box. */
      lbnd = astMalloc( sizeof( double)*(size_t) nax );
      ubnd = astMalloc( sizeof( double)*(size_t) nax );
      if( astOK ) {

/* Do each axis in turn */
         for( i = 0; i < nax; i++ ) {

/* If this axis has both limits, use 1.0E-6 of the difference between the
   limits. */
            if( this->lbnd[ i ] != -DBL_MAX && 
                this->ubnd[ i ] != DBL_MAX ) {
               ubnd[ i ] = 0.5E-6*fabs( this->ubnd[ i ] - this->lbnd[ i ] );
               lbnd[ i ] = -ubnd[ i ];

/* Otherwise use zero. */
            } else {
               ubnd[ i ] = 0.0;
               lbnd[ i ] = 0.0;
            }
         }

/* Create the Box. */
         result = (AstRegion *) astBox( bfrm, 1, lbnd, ubnd, NULL, "" );
      }

/* Free resources. */
      lbnd = astFree( lbnd );
      ubnd = astFree( ubnd );
      bfrm = astAnnul( bfrm );
   }

/* Return NULL if an error occurred. */
   if( !astOK ) result = astAnnul( result );

/* Return the required pointer. */
   return result;
}

void astInitIntervalVtab_(  AstIntervalVtab *vtab, const char *name ) {
/*
*+
*  Name:
*     astInitIntervalVtab

*  Purpose:
*     Initialise a virtual function table for a Interval.

*  Type:
*     Protected function.

*  Synopsis:
*     #include "interval.h"
*     void astInitIntervalVtab( AstIntervalVtab *vtab, const char *name )

*  Class Membership:
*     Interval vtab initialiser.

*  Description:
*     This function initialises the component of a virtual function
*     table which is used by the Interval class.

*  Parameters:
*     vtab
*        Pointer to the virtual function table. The components used by
*        all ancestral classes will be initialised if they have not already
*        been initialised.
*     name
*        Pointer to a constant null-terminated character string which contains
*        the name of the class to which the virtual function table belongs (it 
*        is this pointer value that will subsequently be returned by the Object
*        astClass function).
*-
*/

/* Local Variables: */
   AstMappingVtab *mapping;      /* Pointer to Mapping component of Vtab */
   AstRegionVtab *region;        /* Pointer to Region component of Vtab */

/* Check the local error status. */
   if ( !astOK ) return;

/* Initialize the component of the virtual function table used by the
   parent class. */
   astInitRegionVtab( (AstRegionVtab *) vtab, name );

/* Store a unique "magic" value in the virtual function table. This
   will be used (by astIsAInterval) to determine if an object belongs
   to this class.  We can conveniently use the address of the (static)
   class_init variable to generate this unique value. */
   vtab->check = &class_init;

/* Initialise member function pointers. */
/* ------------------------------------ */
/* Store pointers to the member functions (implemented here) that provide
   virtual methods for this class. */

/* Save the inherited pointers to methods that will be extended, and
   replace them with pointers to the new member functions. */
   mapping = (AstMappingVtab *) vtab;
   region = (AstRegionVtab *) vtab;

   parent_transform = mapping->Transform;
   mapping->Transform = Transform;

   parent_simplify = mapping->Simplify;
   mapping->Simplify = Simplify;

   parent_overlap = region->Overlap;
   region->Overlap = Overlap;

   region->GetBounded = GetBounded;
   region->GetDefUnc = GetDefUnc;
   region->RegPins = RegPins;
   region->RegBaseMesh = RegBaseMesh;
   region->BndBaseMesh = BndBaseMesh;
   region->RegBaseBox = RegBaseBox;

/* Store replacement pointers for methods which will be over-ridden by
   new member functions implemented here. */

/* Declare the copy constructor, destructor and class dump
   functions. */
   astSetDelete( vtab, Delete );
   astSetCopy( vtab, Copy );
   astSetDump( vtab, Dump, "Interval", "Axis intervals" );
}

static int Overlap( AstRegion *this, AstRegion *that ){
/*
*  Name:
*     Overlap

*  Purpose:
*     Test if two regions overlap each other.

*  Type:
*     Private function.

*  Synopsis:
*     #include "interval.h"
*     int Overlap( AstRegion *this, AstRegion *that ) 

*  Class Membership:
*     Interval member function (over-rides the astOverlap method inherited 
*     from the Region class).

*  Description:
*     This function returns an integer value indicating if the two
*     supplied Regions overlap. The two Regions are converted to a commnon
*     coordinate system before performing the check. If this conversion is 
*     not possible (for instance because the two Regions represent areas in
*     different domains), then the check cannot be performed and a zero value 
*     is returned to indicate this.

*  Parameters:
*     this
*        Pointer to the first Region.
*     that
*        Pointer to the second Region.

*  Returned Value:
*     astOverlap()
*        A value indicating if there is any overlap between the two Regions.
*        Possible values are:
*
*        0 - The check could not be performed because the second Region
*            could not be mapped into the coordinate system of the first 
*            Region.
*
*        1 - There is no overlap between the two Regions.
*
*        2 - The first Region is completely inside the second Region.
*
*        3 - The second Region is completely inside the first Region.
*
*        4 - There is partial overlap between the two Regions.
*
*        5 - The Regions are identical.
*
*        6 - The second Region is the negation of the first Region.

*  Notes:
*     - The returned values 5 and 6 do not check the value of the Closed 
*     attribute in the two Regions. 
*     - A value of zero will be returned if this function is invoked with the 
*     AST error status set, or if it should fail for any reason.

*/

/* Local Variables: */
   AstFrame *frm;
   AstFrameSet *fs;
   AstMapping *map;
   AstMapping *map1;
   AstMapping *map2;
   AstMapping *map3;
   AstMapping *smap;
   AstMapping *tmap;
   AstPointSet *pset_that;
   AstRegion *unc_temp;
   AstRegion *unc_that;
   AstRegion *unc_this;
   const char *class;
   double **ptr_that;
   double **ptr_this;
   double *lbndu_that;
   double *lbndu_this;
   double *ubndu_that;
   double *ubndu_this;
   double err;
   double err_that;
   double err_this;
   double lb_that;
   double lb_this;
   double tmp;
   double ub_that;
   double ub_this;
   int ic;
   int inc_that;
   int inc_this;
   int lb_equal;
   int nc;
   int neg_that;
   int neg_this;
   int ov;
   int result;     
   int ub_equal;

   static int newResult[ 5 ][ 5 ] = { { 1, 1, 1, 1, 1},
                                      { 1, 2, 4, 4, 2},
                                      { 1, 4, 3, 4, 3},
                                      { 1, 4, 4, 4, 4},
                                      { 1, 2, 3, 4, 5} };

/* Initialise */
   result = 0;

/* Check the inherited status. */
   if ( !astOK ) return result;

/* If both Regions are Intervals, we provide a specialised implementation.
   The implementation in the parent Region class assumes that at least one of 
   the two Regions can be represented using a finite mesh of points on the 
   boundary which is not the case with Intervals. The implementation in this 
   class sees if the Mapping between the base Frames of the Intervals allows 
   the axis limits to be transferred from one Frame ot the other. */
   if( astIsAInterval( this ) && astIsAInterval( that ) ) {

/* Get a FrameSet which connects the Frame represented by the second Interval
   to the Frame represented by the first Interval. Check that the conection is 
   defined. */
      fs = astConvert( that, this, "" );
      if( fs ) {

/* Get a pointer to the Mapping from base to current Frame in the second 
   Interval */
         map1 = astGetMapping( that->frameset, AST__BASE, AST__CURRENT );

/* Get the Mapping from the current Frame of the second Interval to the
   current Frame of the first Interval. */
         map2 = astGetMapping( fs, AST__BASE, AST__CURRENT );

/* Get a pointer to the Mapping from current to base Frame in the first
   Interval. */
         map3 = astGetMapping( this->frameset, AST__CURRENT, AST__BASE );

/* Combine these Mappings to get the Mapping from the base Frame of the
   second Interval to the base Frame of the first Interval. */
         tmap = (AstMapping *) astCmpMap( map1, map2, 1, "" );
         map = (AstMapping *) astCmpMap( tmap, map3, 1, "" );

/* Simplify this Mapping. */
         smap = astSimplify( map );

/* We can only proceed if the simplified Mapping is a WinMap, ZoomMap, UnitMap,
   ShiftMap or PermMap. These are linear Mappings in which each output axis 
   depends on only one input axis. */
         class = astGetClass( smap );
         if( !strcmp( class, "WinMap" ) ||
             !strcmp( class, "ZoomMap" ) ||
             !strcmp( class, "UnitMap" ) ||
             !strcmp( class, "ShiftMap" ) ||
             !strcmp( class, "PermMap" ) ) {

/* Get the uncertainty Regions for both Intervals, expressed in the base
   Frames of the Intervals. */
            unc_this = astGetUnc( this, AST__BASE );
            unc_temp = astGetUnc( that, AST__BASE );

/* Map the uncertainty Region for the second Interval from the base Frame
   of the second Interval into the base Frame of the first Interval. */
            frm = astGetFrame( this->frameset, AST__BASE );
            unc_that = astMapRegion( unc_temp, smap, frm );

/* Get the bounding boxes of the two uncertainty Regions in the base
   Frame of the first Interval. */
            nc = astGetNaxes( frm );
            lbndu_this = astMalloc( sizeof( double )*(size_t)nc );
            ubndu_this = astMalloc( sizeof( double )*(size_t)nc );
            astRegCurBox( unc_this, lbndu_this, ubndu_this ); 

            lbndu_that = astMalloc( sizeof( double )*(size_t)nc );
            ubndu_that = astMalloc( sizeof( double )*(size_t)nc );
            astRegCurBox( unc_that, lbndu_that, ubndu_that ); 

/* Transform the PointSet holding the limits for the second Interval into
   the Frame of the first Interval. */
            pset_that = astTransform( smap, that->points, 1, NULL );

/* Get pointers for accesing the limits of the two Intervals, expressed
   in a common Frame (the base Frame of the first Interval). */
            ptr_that = astGetPoints( pset_that );
            ptr_this = astGetPoints( this->points );
            if( astOK ) {

/* Check the limits on each base Frame axis in turn. */
               for( ic = 0; ic < nc; ic++ ) {

/* Get the widths of the two uncertainty boxes on this axis. */
                  err_this = astAxDistance( frm, ic + 1, lbndu_this[ ic ],
                                            ubndu_this[ ic ] );
                  err_that = astAxDistance( frm, ic + 1, lbndu_that[ ic ],
                                            ubndu_that[ ic ] );

/* Add this together in quadrature to get the tolerance for two values on
   the current axis to be considered equal. */
                  err = sqrt( err_that*err_that + err_this*err_this );

/* Get the limits on this axis from both Intervals. */
                  lb_this = ptr_this[ ic ][ 0 ];
                  ub_this = ptr_this[ ic ][ 1 ];
                  lb_that = ptr_that[ ic ][ 0 ];
                  ub_that = ptr_that[ ic ][ 1 ];

/* Replace any missing limits with suitable extreme values */
                  if( lb_this == AST__BAD ) lb_this = -DBL_MAX;
                  if( ub_this == AST__BAD ) ub_this = DBL_MAX;
                  if( lb_that == AST__BAD ) lb_that = -DBL_MAX;
                  if( ub_that == AST__BAD ) ub_that = DBL_MAX;

/* If the bounds are the wrong way round (indicating an excluded rather
   than an included axis range), swap them. Also set a flag indicating if 
   the limits define an included or excluded range. */
                  inc_this = ( lb_this <= ub_this );
                  if( !inc_this ) {
                     tmp = lb_this;
                     lb_this = ub_this;
                     ub_this = tmp;
                  } 

                  inc_that = ( lb_that <= ub_that );
                  if( !inc_that ) {
                     tmp = lb_that;
                     lb_that = ub_that;
                     ub_that = tmp;
                  } 

/* Are the lower limits from the two Intervals effectively equal? Take care 
   about DBL_MAX values causing overflow. */
                  lb_equal = ( lb_this == lb_that );
                  if( !lb_equal && fabs(lb_this) != DBL_MAX &&
                                   fabs(lb_that) != DBL_MAX ) {
                     lb_equal = ( fabs( lb_this - lb_that) <= err );
                  }
                                       
/* Are the upper limits from the two Intervals effectively equal? */
                  ub_equal = ( ub_this == ub_that );
                  if( !ub_equal && fabs(ub_this) != DBL_MAX &&
                                   fabs(ub_that) != DBL_MAX ) {
                     ub_equal = ( fabs( ub_this - ub_that) <= err );
                  }

/* If both the limits on this axis are effectively equal for the two Intervals,
   set "ov" to 5 if both Interval ranges are inclusive or both are exclusive,
   and set "ov" to 6 if one Interval range is exclusive and the other is
   inclusive. */
                  if( lb_equal && ub_equal ) {
                     ov = ( inc_this == inc_that ) ? 5 : 6;
 
/* See if the limits on this axis indicate overlap for the two Intervals. "ov" 
   is set to 1 if there is no overlap, 2 if the first Interval range is 
   completely inside the second Interval range, 3 if the second Interval
   range is completely inside the first Interval range, and 4 if there is 
   partial overlap between the Interval ranges. */
                  } else if( inc_this ) {
                     if( inc_that ) {
                        if( lb_that <= lb_this && ub_that >= ub_this ) {
                           ov = 2;
                        } else if( lb_that >= lb_this && ub_that <= ub_this ) {
                           ov = 3;
                        } else if( ub_that >= lb_this && lb_that <= ub_this ) {
                           ov = 4;
                        } else {
                           ov = 1;
                        }

                     } else {

                        if( lb_that <= lb_this && ub_that >= ub_this ) {
                           ov = 1;
                        } else if( lb_that >= ub_this || ub_that <= lb_this ) {
                           ov = 2;
                        } else if( lb_this == -DBL_MAX && ub_this == DBL_MAX ) {
                           ov = 3;
                        } else {
                           ov = 4;
                        }
                     }

                  } else {

                     if( inc_that ) {
                        if( lb_this <= lb_that && ub_this >= ub_that ) {
                           ov = 1;
                        } else if( lb_this >= ub_that || ub_this <= lb_that ) {
                           ov = 3;
                        } else if( lb_that == -DBL_MAX && ub_that == DBL_MAX ) {
                           ov = 2;
                        } else {
                           ov = 4;
                        }

                     } else {
                        ov = 4;
                     }
                  }

/* The returned value is initialised on the basis of the first axis
   overlap. */
                  if( ic == 0 ) {
                     result = ov;

/* For subsequent axes, combine the old result value with the new ov value
   to get the new result value. */
                  } else {
                     result = newResult[ result - 1 ][ ov - 1 ];
                  }

/* If we now know there is no overlap, there is no point in checking any
   remaining axes. */
                  if( result == 1 ) break;

               }

/* The above logic assumed that neither of the Intervals has been negated. 
   Decide on the value to return, taking into account whether either of
   the Intervals has been negated. */
               neg_this = astGetNegated( this );
               neg_that = astGetNegated( that );

               if( result == 1 ) {
                  if( neg_this ) {
                     result = neg_that ? 4 : 3;
                  } else if( neg_that ){ 
                     result = 2;
                  }

               } else if( result == 2) {
                  if( neg_this ) {
                     result = neg_that ? 3 : 4;
                  } else if( neg_that ){ 
                     result = 1;
                  }

               } else if( result == 3) {
                  if( neg_this ) {
                     result = neg_that ? 2 : 1;
                  } else if( neg_that ){ 
                     result = 4;
                  }

               } else if( result == 4) {
                  result = 4;

               } else if( result == 5) {
                  if( neg_this ) {
                     result = neg_that ? 5 : 6;
                  } else if( neg_that ){ 
                     result = 6;
                  }
               }
            }

/* Free resources. */
            pset_that = astAnnul( pset_that );
            unc_this = astAnnul( unc_this );
            unc_that = astAnnul( unc_that );
            unc_temp = astAnnul( unc_temp );
            frm = astAnnul( frm );
            lbndu_this = astFree( lbndu_this );
            ubndu_this = astFree( ubndu_this );
            lbndu_that = astFree( lbndu_that );
            ubndu_that = astFree( ubndu_that );
         }

         smap = astAnnul( smap );
         map = astAnnul( map );
         tmap = astAnnul( tmap );
         map3 = astAnnul( map3 );
         map2 = astAnnul( map2 );
         map1 = astAnnul( map1 );
         fs = astAnnul( fs );
      }
   }

/* If overlap could not be determined using the above implementation, try 
   using the implementation inherited from the parent Region class. */
   if( !result ) result = (*parent_overlap)( this, that );

/* If not OK, return zero. */
   if( !astOK ) result = 0;

/* Return the result. */
   return result;
}

static void RegBaseBox( AstRegion *this, double *lbnd, double *ubnd ){
/*
*  Name:
*     RegBaseBox

*  Purpose:
*     Returns the bounding box of an un-negated Region in the base Frame of 
*     the encapsulated FrameSet.

*  Type:
*     Private function.

*  Synopsis:
*     #include "interval.h"
*     void RegBaseBox( AstRegion *this, double *lbnd, double *ubnd )

*  Class Membership:
*     Interval member function (over-rides the astRegBaseBox protected
*     method inherited from the Region class).

*  Description:
*     This function returns the upper and lower axis bounds of a Region in 
*     the base Frame of the encapsulated FrameSet, assuming the Region
*     has not been negated. That is, the value of the Negated attribute
*     is ignored.

*  Parameters:
*     this
*        Pointer to the Region.
*     lbnd
*        Pointer to an array in which to return the lower axis bounds
*        covered by the Region in the base Frame of the encpauslated
*        FrameSet. It should have at least as many elements as there are 
*        axes in the base Frame.
*     ubnd
*        Pointer to an array in which to return the upper axis bounds
*        covered by the Region in the base Frame of the encapsulated
*        FrameSet. It should have at least as many elements as there are 
*        axes in the base Frame.

*  Notes:
*     - An error is reported if the Interval is unbounded.

*/

/* Local Variables: */
   AstBox *box;                  /* The equivalent Box */

/* Check the global error status. */
   if ( !astOK ) return;

/* If the Interval is effectively a Box, invoke the astRegBaseBox
   function on the equivalent Box. A pointer to the equivalent Box will
   be stored in the Interval structure. */
   box = ((AstInterval *) this)->box;
   if( box ) {
      astRegBaseBox( box, lbnd, ubnd );

/* If the Interval is not equivalent to a Box, report an error. */
   } else {
      astError( AST__INTER, "astRegBaseBox(%s): The %s given is "
                "unbounded and therefore no bounding box can be "
                "found (internal AST programming error).", 
                astGetClass( this ) );
   }
}

static AstPointSet *RegBaseMesh( AstRegion *this_region ){
/*
*  Name:
*     RegBaseMesh

*  Purpose:
*     Return a PointSet containing a mesh of points on the boundary of a 
*     Region in its base Frame.

*  Type:
*     Private function.

*  Synopsis:
*     #include "interval.h"
*     AstPointSet *astRegBaseMesh( AstRegion *this )

*  Class Membership:
*     Interval member function (over-rides the astRegBaseMesh protected
*     method inherited from the Region class).

*  Description:
*     This function returns a PointSet containing a mesh of points on the
*     boundary of the Region. The points refer to the base Frame of
*     the encapsulated FrameSet.

*  Parameters:
*     this
*        Pointer to the Region.

*  Returned Value:
*     Pointer to the PointSet. The axis values in this PointSet will have 
*     associated accuracies derived from the accuracies which were
*     supplied when the Region was created.

*  Notes:
*    - A NULL pointer is returned if an error has already occurred, or if
*    this function should fail for any reason.

*/

/* Local Variables: */
   AstBox *box;                  /* The equivalent Box */
   AstPointSet *result;          /* Returned pointer */

/* Initialise */
   result = NULL;

/* Check the global error status. */
   if ( !astOK ) return result;

/* If the Interval is effectively a Box, invoke the astRegBaseMesh
   function on the equivalent Box. A pointer to the equivalent Box will
   be stored in the Interval structure. */
   box = ((AstInterval *) this_region )->box;
   if( box ) {
      result = astRegBaseMesh( box );

/* If the Interval is not equivalent to a Box, report an error. */
   } else {
      astError( AST__INTER, "astRegBaseMesh(%s): The %s given is "
                "unbounded and therefore no boundary mesh can be "
                "produced (internal AST programming error).", 
                astGetClass( this_region ) );
   }

/* Return a pointer to the output PointSet. */
   return result;
}

static int RegPins( AstRegion *this_region, AstPointSet *pset, AstRegion *unc,
                    int **mask ){
/*
*  Name:
*     RegPins

*  Purpose:
*     Check if a set of points fall on the boundary of a given Interval.

*  Type:
*     Private function.

*  Synopsis:
*     #include "interval.h"
*     int RegPins( AstRegion *this, AstPointSet *pset, AstRegion *unc,
*                  int **mask )

*  Class Membership:
*     Interval member function (over-rides the astRegPins protected
*     method inherited from the Region class).

*  Description:
*     This function returns a flag indicating if the supplied set of
*     points all fall on the boundary of the given Interval. 
*
*     Some tolerance is allowed, as specified by the uncertainty Region
*     stored in the supplied Interval "this", and the supplied uncertainty
*     Region "unc" which describes the uncertainty of the supplied points.

*  Parameters:
*     this
*        Pointer to the Interval.
*     pset
*        Pointer to the PointSet. The points are assumed to refer to the 
*        base Frame of the FrameSet encapsulated by "this".
*     unc
*        Pointer to a Region representing the uncertainties in the points
*        given by "pset". The Region is assumed to represent the base Frame 
*        of the FrameSet encapsulated by "this". Zero uncertainity is assumed 
*        if NULL is supplied.
*     mask
*        Pointer to location at which to return a pointer to a newly
*        allocated dynamic array of ints. The number of elements in this
*        array is equal to the value of the Npoint attribute of "pset".
*        Each element in the returned array is set to 1 if the
*        corresponding position in "pset" is on the boundary of the Region
*        and is set to zero otherwise. A NULL value may be supplied
*        in which case no array is created. If created, the array should
*        be freed using astFree when no longer needed.

*  Returned Value:
*     Non-zero if the points all fall on the boundary of the given
*     Region, to within the tolerance specified. Zero otherwise.

*/

/* Local Variables: */
   AstBox *box;                  /* The equivalent Box */
   int result;                   /* The returned value */

/* Initialise */
   result = 0;
   if( mask ) *mask = NULL;

/* Check the global error status. */
   if ( !astOK ) return result;

/* If the Interval is effectively a Box, invoke the astRegPins
   function on the equivalent Box. A pointer to the equivalent Box will
   be stored in the Interval structure. */
   box = ((AstInterval *) this_region )->box;
   if( box ) {
      result = astRegPins( box, pset, unc, mask );

/* If the Interval is not equivalent to a Box, report an error. */
   } else {
      astError( AST__INTER, "astRegPins(%s): The %s given is "
                "unbounded and therefore the boundary mesh cannot be "
                "checked (internal AST programming error).", 
                astGetClass( this_region ) );
   }

/* Return the result. */
   return result;
}

static AstMapping *Simplify( AstMapping *this_mapping ) {
/*
*  Name:
*     Simplify

*  Purpose:
*     Simplify the Mapping represented by a Region.

*  Type:
*     Private function.

*  Synopsis:
*     #include "interval.h"
*     AstMapping *Simplify( AstMapping *this )

*  Class Membership:
*     Interval method (over-rides the astSimplify method inherited
*     from the Region class).

*  Description:
*     This function invokes the parent Region Simplify method, and then
*     performs any further region-specific simplification.
*
*     If the Mapping from base to current Frame is not a UnitMap, this
*     will include attempting to fit a new Region to the boundary defined
*     in the current Frame.

*  Parameters:
*     this
*        Pointer to the original Region.

*  Returned Value:
*     A pointer to the simplified Region. A cloned pointer to the
*     supplied Region will be returned if no simplication could be
*     performed.

*  Notes:
*     - A NULL pointer value will be returned if this function is
*     invoked with the AST error status set, or if it should fail for
*     any reason.
*/

/* Local Variables: */
   AstBox *box2;              /* Box used to determine 1-to-1 axis correspondance */
   AstBox *box;               /* Box used to determine 1-to-1 axis correspondance */
   AstInterval *this_interval;/* Pointer to Interval structure */
   AstMapping *bfrm;          /* Pointer to base Frame in supplied Interval */
   AstMapping *cfrm;          /* Pointer to current Frame in supplied Interval */
   AstMapping *map;           /* Base -> current Mapping after parent simplification */
   AstMapping *result;        /* Result pointer to return */
   AstPointSet *pset2;        /* PointSet containing current Frame test points */
   AstPointSet *pset3;        /* PointSet containing base Frame test points */
   AstPointSet *psetb;        /* PointSet holding base positions */
   AstPointSet *psetc;        /* PointSet holding current positions */
   AstRegion *new;            /* Pointer to Region simplfied by parent class */
   AstRegion *sreg;           /* Pointer to simplified Box */
   AstRegion *this;           /* Pointer to supplied Region structure */
   AstRegion *unc;            /* Pointer to uncertainty Region */
   double **ptr2;             /* Pointer axis values in "pset2" */
   double **ptr3;             /* Pointer axis values in "pset3" */
   double **ptr;              /* Pointer to base Frame values defining Interval */
   double **ptrb;             /* Pointer to "psetb" axis values */
   double **sptr;             /* Pointer to simplified Interval bounds */
   double *lbnd;              /* Pointer to array of base Frame lower bounds */
   double *slbnd;             /* Pointer to array of current Frame lower bounds */
   double *subnd;             /* Pointer to array of current Frame upper bounds */
   double *ubnd;              /* Pointer to array of base Frame upper bounds */
   double d;                  /* Distance between axis values */
   double lb;                 /* Lower bound on axis values */
   double lwid;               /* Axis width below the Interval lower limit */
   double maxd;               /* Maximum currenrt Frame axis offset between test points */
   double tmp;                /* Temporary storage for swapping variable values */
   double ub;                 /* Upperbound on axis values */
   double uwid;               /* Axis width above the Interval upper limit */
   int bax;                   /* Base Frame axis index corresponding to "ic" */
   int ic;                    /* Axis index */
   int jc;                    /* Axis index */
   int nc;                    /* No. of base Frame axis values per point */
   int simpler;               /* Has some simplication taken place? */
   int snc;                   /* No. of current Frame axis values per point */

/* Initialise. */
   result = NULL;

/* Check the global error status. */
   if ( !astOK ) return result;

/* Get a pointer to the supplied Region structure. */
   this = (AstRegion *) this_mapping;

/* Get a pointer to the supplied Interval structure. */
   this_interval = (AstInterval *) this;

/* If this Interval is equivalent to a Box, use the astTransform method of
   the equivalent Box. */
   if( ((AstInterval *) this)->box ) {
      result = astSimplify( ((AstInterval *) this)->box );

/* Otherwise, we use a new implementation appropriate for unbounded
   intervals. */
   } else {

/* Invoke the parent Simplify method inherited from the Region class. This
   will simplify the encapsulated FrameSet and uncertainty Region. */
      new = (AstRegion *) (*parent_simplify)( this_mapping );

/* Note if any simplification took place. This is assumed to be the case
   if the pointer returned by the above call is different to the supplied
   pointer. */
      simpler = ( new != this );

/* If the Mapping from base to current Frame is not a UnitMap, we attempt
   to simplify the Interval by re-defining it within its current Frame. */
      map = astGetMapping( new->frameset, AST__BASE, AST__CURRENT );
      if( !astIsAUnitMap( map ) ){

/* Take a copy of the Interval bounds (defined in the base Frame of the
   Intervals FrameSet) and replace any missing limits with arbitrary
   non-BAD values. This will give us a complete set of bounds defining a
   box within the base Frame of the Interval. */
         ptr = astGetPoints( new->points );
         nc = astGetNcoord( new->points );
   
         lbnd = astMalloc( sizeof( double )*(size_t) nc );
         ubnd = astMalloc( sizeof( double )*(size_t) nc );
   
         if( astOK ) {
            for( ic = 0; ic < nc; ic++ ) {
               lbnd[ ic ] = ptr[ ic ][ 0 ];
               ubnd[ ic ] = ptr[ ic ][ 1 ];
   
/* Ensure we have a good upper bound for this axis. */
               if( ubnd[ ic ] == AST__BAD ) {
                  if( lbnd[ ic ] == AST__BAD ) {
                     ubnd[ ic ] = 1.0;
   
                  } else if( lbnd[ ic ] > 0.0 ) {
                     ubnd[ ic ] = lbnd[ ic ]*1.01;
   
                  } else if( lbnd[ ic ] < 0.0 ) {
                     ubnd[ ic ] = lbnd[ ic ]*0.99;
                  
                  } else {
                     ubnd[ ic ] = 1.0;
                  }
               }
   
/* Ensure we have a good lower bound for this axis. */
               if( lbnd[ ic ] == AST__BAD ) {
                  if( ubnd[ ic ] > 0.0 ) {
                     lbnd[ ic ] = ubnd[ ic ]*0.99;
   
                  } else if( ubnd[ ic ] < 0.0 ) {
                     lbnd[ ic ] = ubnd[ ic ]*1.01;
                  
                  } else {
                     lbnd[ ic ] = 1.0;
                  }
               }
            }
         }

/* Transform the box corners found above into the current frame and then back
   into the base Frame, and ensure that the box encloses both the original
   and the new bounds. PermMaps with fewer outputs than inputs can cause the 
   resulting base Frame positions to differ significantly from the original. */
         psetb =astPointSet( 2, nc,"" );
         ptrb =astGetPoints( psetb );
         if( astOK ) {
            for( ic = 0; ic < nc; ic++ ) {
               ptrb[ ic ][ 0 ] = lbnd[ ic ];
               ptrb[ ic ][ 1 ] = ubnd[ ic ];
            }
         }
         psetc = astTransform( map, psetb, 1, NULL );
         astTransform( map, psetc, 0, psetb );
         if( astOK ) {
            for( ic = 0; ic < nc; ic++ ) {
               lb = ptrb[ ic ][ 0 ];
               if( lb != AST__BAD ) {
                  if( lb < lbnd[ ic ] ) lbnd[ ic ] = lb;
                  if( lb > ubnd[ ic ] ) ubnd[ ic ] = lb;
               }
               ub = ptrb[ ic ][ 1 ];
               if( ub != AST__BAD ) {
                  if( ub < lbnd[ ic ] ) lbnd[ ic ] = ub;
                  if( ub > ubnd[ ic ] ) ubnd[ ic ] = ub;
               }
            }
         }
         psetb = astAnnul( psetb );
         psetc = astAnnul( psetc );

/* Limit this box to not exceed the limits imposed by the Interval.*/
         for( ic = 0; ic < nc; ic++ ) {
            lb = this_interval->lbnd[ ic ] ;
            ub = this_interval->ubnd[ ic ] ;
            if( lb <= ub ) {
               if( lbnd[ ic ] < lb ) {
                  lbnd[ ic ] = lb;
               } else if( lbnd[ ic ] > ub ) {
                  lbnd[ ic ] = ub;
               }
               if( ubnd[ ic ] < lb ) {
                  ubnd[ ic ] = lb;
               } else if( ubnd[ ic ] > ub ) {
                  ubnd[ ic ] = ub;
               }
            } else {
               lwid = lb - lbnd[ ic ];
               uwid = ubnd[ ic ] - ub;
               if( lwid > uwid ) {
                 if( lbnd[ ic ] > lb ) lbnd[ ic ] = lb;
                 if( ubnd[ ic ] > lb ) ubnd[ ic ] = lb;
               } else {
                 if( lbnd[ ic ] < ub ) lbnd[ ic ] = ub;
                 if( ubnd[ ic ] < ub ) ubnd[ ic ] = ub;
               }
            }

/* Ensure the bounds are not equal */
            if( lbnd[ ic ] == 0.0 && ubnd[ ic ] == 0.0 ) {
               ubnd[ ic ] = 1.0;

            } else if( EQUAL( lbnd[ ic ], ubnd[ ic ] ) ) {
               ubnd[ ic ] = MAX( ubnd[ ic ], lbnd[ ic ] )*( 1.0E6*DBL_EPSILON );
            }
         }
   
/* Create a new Box representing the box found above. */
         bfrm = astGetFrame( new->frameset, AST__BASE );
         unc = astTestUnc( new ) ? astGetUnc( new, AST__BASE ) : NULL;
         box = astBox( bfrm, 1, lbnd, ubnd, unc, "" );
         if( unc ) unc = astAnnul( unc );
   
/* Modify this Box so that it has the same current Frame as this
   Interval. */
         cfrm = astGetFrame( new->frameset, AST__CURRENT );
         box2 = astMapRegion( box, map, cfrm );
   
/* Try simplifying the Box. */
         sreg = (AstRegion *) astSimplify( box2 );
   
/* Only proceed if the Box was simplified */
         if( sreg != (AstRegion *) box2 ) {

/* If the simplified Box is a NullRegion return it. */
            if( astIsANullRegion( sreg ) ) {
               astAnnul( new );
               new = astClone( sreg );
               simpler = 1;
              
/* If the simplified Box is a Box or an Interval... */
            } else if( astIsABox( sreg ) || astIsAInterval( sreg ) ) {
   
/* Get the bounds of the simplified Box. We assume that the base and
   current Frames in the simplified Box are the same. */
               snc = astGetNin( sreg->frameset );
               slbnd = astMalloc( sizeof( double )*(size_t)snc );
               subnd = astMalloc( sizeof( double )*(size_t)snc );
               if(  astIsAInterval( sreg ) ) {
                  sptr = astGetPoints( sreg->points );
                  if( astOK ) {
                     for( ic = 0; ic < snc; ic++ ) {
                        slbnd[ ic ] = sptr[ ic ][ 0 ];
                        subnd[ ic ] = sptr[ ic ][ 1 ];
                     }
                  }
               } else {
                  astRegBaseBox( sreg, slbnd, subnd );
               }
      
/* Now create a PointSet containing one point for each axis in the
   current (or equivalently, base ) Frame of the simplified Box, plus an
   extra point. */
               pset2 = astPointSet( snc + 1, snc, "" );
               ptr2 = astGetPoints( pset2 );
      
/* Put the lower bounds of the simplified Box into the first point in
   this PointSet. The remaining points are displaced from this first point
   along each axis in turn. The length of each displacement is determined
   by the length of the box on the axis. */
               if( astOK ) {
                  for( ic = 0; ic < snc; ic++ ) {
                     for( jc = 0; jc < snc + 1; jc++ ) {
                        ptr2[ ic ][ jc ] = slbnd[ ic ];
                     }
                     ptr2[ ic ][ ic + 1 ] = subnd[ ic ];
                  }
               }
      
/* Transform this PointSet into the base Frame of this Interval using the
   inverse of the base->current Mapping. */
               pset3 = astTransform( map, pset2, 0, NULL );
               ptr3 = astGetPoints( pset3 );
               if( astOK ) {
      
/* Now consider each axis of the Interval's current Frame (i.e. each base 
   Frame axis in the simplified Box). */
                  for( ic = 0; ic < snc; ic++ ) {
      
/* Given that the Box simplified succesfully, we know that there is a one
   to one connection between the axes of the base and current Frame in this
   Interval, but we do not yet know which base Frame axis corresponds to
   which current Frame axis (and the number of base and current Frame axes
   need not be equal). We have two points on a line parallel to current
   Frame axis number "ic" (points zero and "ic+1" in "pset2"). Look at the 
   corresponding base Frame positions (in "pset3), and see which base Frame 
   axis they are parallel to. We look for the largest base Frame axis 
   increment (this allows small non-zero displacements to occur on the 
   other axes due to rounding errors). */
                     maxd = -DBL_MAX;
                     bax = -1;
                     for( jc = 0; jc < nc; jc++ ) {
                        d = astAxDistance( bfrm, jc + 1, ptr3[ jc ][ 0 ],
                                           ptr3[ jc ][ ic + 1 ] );
                        if( d != AST__BAD && d > maxd ) {
                           maxd = d;
                           bax = jc;
                        }
                     }
      
/* If the largest base Frame axis increment is zero, it must mean that
   the current Frame axis is not present in the base Frame. The only
   plausable cause of this is if the base->current Mapping contains a
   PermMap which introduces an extra axis, in which case the axis will
   have a fixed value (any other Mapping arrangement would have prevented 
   the Box from simplifying). Therefore, set upper and lower limits for
   this axis to the same value. */
                     if( maxd <= 0.0 ) {
                        slbnd[ ic ] = 0.5*( slbnd[ ic ] + subnd[ ic ] );
                        subnd[ ic ] = slbnd[ ic ];
      
/* If we have found a base Frame axis which corresponds to the current
   Frame axis "ic", then look to see which limits are specified for the
   base Frame axis, and transfer missing limits to the current Frame. */
                     } else {
                        if( ptr[ bax ][ 0 ] == AST__BAD ) slbnd[ ic ] = AST__BAD;
                        if( ptr[ bax ][ 1 ] == AST__BAD ) subnd[ ic ] = AST__BAD;
      
/* If the original limits were equal, ensure the new limits are equal
   (the code above modified the upper limit to ensure it was different to
   the lower limit). */
                        if( ptr[ bax ][ 1 ] == ptr[ bax ][ 0 ] ) {
                           subnd[ ic ] = slbnd[ ic ];
      
/* If the original interval was an inclusion (ubnd > lbnd), ensure the new 
   interval is also an inclusion by swapping the limits if required. */
                        } else if( ptr[ bax ][ 1 ] > ptr[ bax ][ 0 ] ) {
                           if( subnd[ ic ] < slbnd[ ic ] ) {
                              tmp = subnd[ ic ];
                              subnd[ ic ] = slbnd[ ic ];
                              slbnd[ ic ] = tmp;
                           }
      
/* If the original interval was an exclusion (ubnd < lbnd), ensure the new 
   interval is also an exlusion by swapping the limits if required. */
                        } else if( ptr[ bax ][ 1 ] < ptr[ bax ][ 0 ] ) {
                           if( subnd[ ic ] > slbnd[ ic ] ) {
                              tmp = subnd[ ic ];
                              subnd[ ic ] = slbnd[ ic ];
                              slbnd[ ic ] = tmp;
                           }
                        } 
                     }            
                  }
      
/* Create the simplified Interval from the current Frame limits found
   above, and use it in place of the original. */
                  unc = astTestUnc( new ) ? astGetUnc( new, AST__CURRENT ) : NULL;
                  astAnnul( new );
                  new = (AstRegion *) astInterval( cfrm, slbnd, subnd, unc, "" );
                  if( unc ) unc = astAnnul( unc );
                  simpler = 1;
      
               }
      
/* Free resources */
               pset2 = astAnnul( pset2 );
               pset3 = astAnnul( pset3 );
               slbnd = astFree( slbnd );
               subnd = astFree( subnd );
            }
         }

/* Free resources */
         bfrm = astAnnul( bfrm );
         cfrm = astAnnul( cfrm );
         box = astAnnul( box );
         box2 = astAnnul( box2 );
         sreg = astAnnul( sreg );
   
      }
   
/* Free resources */
      map = astAnnul( map );
   
/* If any simplification could be performed, copy Region attributes from 
   the supplied Region to the returned Region, and return a pointer to it.
   Otherwise, return a clone of the supplied pointer. */
      if( simpler ){
         astRegOverlay( new, this );
         result = (AstMapping *) new;
   
      } else {
         new = astAnnul( new );
         result = astClone( this );
      }
   }

/* If an error occurred, annul the returned pointer. */
   if ( !astOK ) result = astAnnul( result );

/* Return the result. */
   return result;
}

static AstPointSet *Transform( AstMapping *this_mapping, AstPointSet *in,
                               int forward, AstPointSet *out ) {
/*
*  Name:
*     Transform

*  Purpose:
*     Apply a Interval to transform a set of points.

*  Type:
*     Private function.

*  Synopsis:
*     #include "interval.h"
*     AstPointSet *Transform( AstMapping *this, AstPointSet *in,
*                             int forward, AstPointSet *out )

*  Class Membership:
*     Interval member function (over-rides the astTransform protected
*     method inherited from the Region class).

*  Description:
*     This function takes a Interval and a set of points encapsulated in a
*     PointSet and transforms the points by setting axis values to
*     AST__BAD for all points which are outside the region. Points inside
*     the region are copied unchanged from input to output.

*  Parameters:
*     this
*        Pointer to the Interval.
*     in
*        Pointer to the PointSet holding the input coordinate data.
*     forward
*        A non-zero value indicates that the forward coordinate transformation
*        should be applied, while a zero value requests the inverse
*        transformation.
*     out
*        Pointer to a PointSet which will hold the transformed (output)
*        coordinate values. A NULL value may also be given, in which case a
*        new PointSet will be created by this function.

*  Returned Value:
*     Pointer to the output (possibly new) PointSet.

*  Notes:
*     -  The forward and inverse transformations are identical for a
*     Region.
*     -  A null pointer will be returned if this function is invoked with the
*     global error status set, or if it should fail for any reason.
*     -  The number of coordinate values per point in the input PointSet must
*     match the number of axes in the Frame represented by the Interval.
*     -  If an output PointSet is supplied, it must have space for sufficient
*     number of points and coordinate values per point to accommodate the
*     result. Any excess space will be ignored.
*/

/* Local Variables: */
   AstInterval *this;            /* Pointer to Interval structure */
   AstPointSet *pset_tmp;        /* Pointer to PointSet holding base Frame positions*/
   AstPointSet *result;          /* Pointer to output PointSet */
   AstRegion *reg;               /* Pointer to Region structure */
   AstRegion *unc;               /* Uncertainty Region */
   double **ptr_lims;            /* Pointer to limits array */
   double **ptr_out;             /* Pointer to output coordinate data */
   double **ptr_tmp;             /* Pointer to base Frame coordinate data */
   double *lbnd_unc;             /* Lower bounds of uncertainty Region */
   double *ubnd_unc;             /* Upper bounds of uncertainty Region */
   double lb;                    /* Base Frame axis lower bound */
   double p;                     /* Input base Frame axis value */
   double ub;                    /* Base Frame axis upper bound */
   double wid;                   /* Half width of uncertainy Region */
   int coord;                    /* Zero-based index for coordinates */
   int ncoord_out;               /* No. of coordinates per output point */
   int ncoord_tmp;               /* No. of coordinates per base Frame point */
   int neg;                      /* Has the Region been negated? */
   int npoint;                   /* No. of points */
   int pass;                     /* Does this point pass the axis test? */
   int point;                    /* Loop counter for points */
   int setbad;                   /* Set the output point bad? */

/* Check the global error status. */
   if ( !astOK ) return NULL;

/* Obtain pointers to the Region and to the Interval. */
   reg = (AstRegion *) this_mapping;
   this = (AstInterval *) this_mapping;

/* If this Interval is equivalent to a Box, use the astTransform method of
   the equivalent Box. */
   if( this->box ) {
      result = astTransform( this->box, in, forward, out );

/* Otherwise, we use a new implementation appropriate for unbounded
   intervals. */
   } else {

/* Apply the parent mapping using the stored pointer to the Transform member
   function inherited from the parent Region class. This function validates
   all arguments and generates an output PointSet if necessary,
   containing a copy of the input PointSet. */
      result = (*parent_transform)( this_mapping, in, forward, out );

/* We will now extend the parent astTransform method by performing the
   calculations needed to generate the output coordinate values. */

/* First use the encapsulated FrameSet to transform the supplied positions
   from the current Frame in the encapsulated FrameSet (the Frame
   represented by the Region), to the base Frame (the Frame in which the
   Region is defined). This call also returns a pointer to the base Frame
   of the encapsulated FrameSet. Note, the returned pointer may be a
   clone of the "in" pointer, and so we must be carefull not to modify the
   contents of the returned PointSet. */
      pset_tmp = astRegTransform( reg, in, 0, NULL, NULL );

/* Determine the numbers of points and coordinates per point from the base
   Frame PointSet and obtain pointers for accessing the base Frame and output 
   coordinate values. */
      npoint = astGetNpoint( pset_tmp );
      ncoord_tmp = astGetNcoord( pset_tmp );
      ptr_tmp = astGetPoints( pset_tmp );      
      ncoord_out = astGetNcoord( result );
      ptr_out = astGetPoints( result );

/* Get a pointer to the array of axis limits */
      ptr_lims = astGetPoints( reg->points );

/* See if the Region is negated. */
      neg = astGetNegated( reg );

/* Indicate we have not yet got the bounding box of the uncertainty
   Region. */
      lbnd_unc = NULL;
      ubnd_unc = NULL;
      unc = NULL;

/* Perform coordinate arithmetic. */
      if ( astOK ) {

/* First deal with closed unnegated Intervals. */
/* ------------------------------------------- */
         if( astGetClosed( reg ) ) {
            if( !neg ) {

/* Loop round each point. */
               for ( point = 0; point < npoint; point++ ) {

/* Assume this point is inside the Region. We change this flag when we find 
   the first axis for which the point does not pass the axis test. */
                  setbad = 0;

/* Loop round each base Frame axis */
                  for ( coord = 0; coord < ncoord_tmp; coord++ ) {
                     p = ptr_tmp[ coord ][ point ];
                     lb = (this->lbnd)[ coord ];
                     ub = (this->ubnd)[ coord ];

/* If the limits are equal separate them slightly to give some tolerance. */
                     if( lb == ub ) {

/* If not yet done so, get the bounding box of the uncertainty Region in the 
   base Frame of the Interval */
                        if( !unc ) {
                           unc = astGetUnc( reg, AST__BASE );
                           lbnd_unc = astMalloc( sizeof( double)*(size_t) ncoord_tmp );
                           ubnd_unc = astMalloc( sizeof( double)*(size_t) ncoord_tmp );
                           astRegCurBox( unc, lbnd_unc, ubnd_unc );
                        }

/* Set the gap between the limits to be equal to the uincertainty on this
   axis. */
                        if( astOK ) {
                           wid = 0.5*( ubnd_unc[ coord ] - lbnd_unc[ coord ] );
                           lb -= wid;
                           ub += wid;
                        }
                     }

/* Bad input points should always be bad in the output. */
                     if( p == AST__BAD ) {
                        setbad = 1;
                        break;

/* Does the current axis value pass the limits test for this axis? */
                     } else if( lb <= ub ) {
                        pass = ( lb <= p && p <= ub );
                     } else {
                        pass = ( p <= ub || lb <= p );
                     }

/* If this point does not pass the test for this axis, then indicate that
   we should set the resulting output point bad and break since we now have 
   a definite value for the inside/outside flag. */
                     if( !pass ) {
                        setbad = 1;
                        break;
                     }
                  }

/* Set the axis values bad for this output point if required. */
                  if( setbad ) {
                     for ( coord = 0; coord < ncoord_out; coord++ ) {
                        ptr_out[ coord ][ point ] = AST__BAD;
                     }
                  }
               }

/* Now deal with closed negated Intervals. */
/* --------------------------------------- */
            } else  {

/* Loop round each point. */
               for ( point = 0; point < npoint; point++ ) {

/* Assume this point is outside the negated Region (i.e. inside the
   unnegated Region). We change this flag when we find the first axis for
   which the point passes the axis test. */
                  setbad = 1;

/* Loop round each base Frame axis */
                  for ( coord = 0; coord < ncoord_tmp; coord++ ) {
                     p = ptr_tmp[ coord ][ point ];
                     lb = (this->lbnd)[ coord ];
                     ub = (this->ubnd)[ coord ];

/* Bad input points should always be bad in the output. */
                     if( p == AST__BAD ) {
                        setbad = 1;
                        break;

/* Does the current axis value pass the limits test for this axis? */
                     } else if( lb <= ub ) {
                        pass = ( p <= lb || ub <= p );
                     } else {
                        pass = ( ub <= p && p <= lb );
                     }

/* If this point passes the test for this axis, then indicate that we should 
   not set the resulting output point bad and break since we now have a 
   definite value for the inside/outside flag. */
                     if( pass ) {
                        setbad = 0;
                        break;
                     }
                  }

/* Set the axis values bad for this output point if required. */
                  if( setbad ) {
                     for ( coord = 0; coord < ncoord_out; coord++ ) {
                        ptr_out[ coord ][ point ] = AST__BAD;
                     }
                  }
               }
            }

/* Now deal with open unnegated Intervals. */
/* --------------------------------------- */
         } else {
            if( !neg ) {

/* Loop round each point. */
               for ( point = 0; point < npoint; point++ ) {

/* Assume this point is inside the Region. We change this flag when we find 
   the first axis for which the point does not pass the axis test. */
                  setbad = 0;

/* Loop round each base Frame axis */
                  for ( coord = 0; coord < ncoord_tmp; coord++ ) {
                     p = ptr_tmp[ coord ][ point ];
                     lb = (this->lbnd)[ coord ];
                     ub = (this->ubnd)[ coord ];

/* Bad input points should always be bad in the output. */
                     if( p == AST__BAD ) {
                        setbad = 1;
                        break;

/* Does the current axis value pass the limits test for this axis? */
                     } else if( lb <= ub ) {
                        pass = ( lb < p && p < ub );
                     } else {
                        pass = ( p < ub || lb < p );
                     }

/* If this point does not pass the test for this axis, then indicate that
   we should set the resulting output point bad and break since we now have 
   a definite value for the inside/outside flag. */
                     if( !pass ) {
                        setbad = 1;
                        break;
                     }
                  }

/* Set the axis values bad for this output point if required. */
                  if( setbad ) {
                     for ( coord = 0; coord < ncoord_out; coord++ ) {
                        ptr_out[ coord ][ point ] = AST__BAD;
                     }
                  }
               }

/* Now deal with open negated Intervals. */
/* ------------------------------------- */
            } else  {

/* Loop round each point. */
               for ( point = 0; point < npoint; point++ ) {

/* Assume this point is outside the negated Region (i.e. inside the
   unnegated Region). We change this flag when we find the first axis for
   which the point passes the axis test. */
                  setbad = 1;

/* Loop round each base Frame axis */
                  for ( coord = 0; coord < ncoord_tmp; coord++ ) {
                     p = ptr_tmp[ coord ][ point ];
                     lb = (this->lbnd)[ coord ];
                     ub = (this->ubnd)[ coord ];

/* If the limits are equal separate them slightly to give some tolerance. */
                     if( lb == ub ) {

/* If not yet done so, get the bounding box of the uncertainty Region in the 
   base Frame of the Interval */
                        if( !unc ) {
                           unc = astGetUnc( reg, AST__BASE );
                           lbnd_unc = astMalloc( sizeof( double)*(size_t) ncoord_tmp );
                           ubnd_unc = astMalloc( sizeof( double)*(size_t) ncoord_tmp );
                           astRegCurBox( unc, lbnd_unc, ubnd_unc );
                        }

/* Set the gap between the limits to be equal to the uincertainty on this
   axis. */
                        if( astOK ) {
                           wid = 0.5*( ubnd_unc[ coord ] - lbnd_unc[ coord ] );
                           lb -= wid;
                           ub += wid;
                        }
                     }

/* Bad input points should always be bad in the output. */
                     if( p == AST__BAD ) {
                        setbad = 1;
                        break;

/* Does the current axis value pass the limits test for this axis? */
                     } else if( lb <= ub ) {
                        pass = ( p < lb || ub < p );
                     } else {
                        pass = ( ub < p && p < lb );
                     }

/* If this point passes the test for this axis, then indicate that we should 
   not set the resulting output point bad and break since we now have a 
   definite value for the inside/outside flag. */
                     if( pass ) {
                        setbad = 0;
                        break;
                     }
                  }

/* Set the axis values bad for this output point if required. */
                  if( setbad ) {
                     for ( coord = 0; coord < ncoord_out; coord++ ) {
                        ptr_out[ coord ][ point ] = AST__BAD;
                     }
                  }
               }
            }
         }
      }

/* Free resources */
      pset_tmp = astAnnul( pset_tmp );
      if( lbnd_unc ) lbnd_unc = astFree( lbnd_unc );
      if( ubnd_unc ) ubnd_unc = astFree( ubnd_unc );
      if( unc ) unc = astAnnul( unc );
   }

/* Annul the result if an error has occurred. */
   if( !astOK ) result = astAnnul( result );

/* Return a pointer to the output PointSet. */
   return result;
}

/* Functions which access class attributes. */
/* ---------------------------------------- */
/* Implement member functions to access the attributes associated with
   this class using the macros defined for this purpose in the
   "object.h" file. For a description of each attribute, see the class
   interface (in the associated .h file). */

/* Copy constructor. */
/* ----------------- */
static void Copy( const AstObject *objin, AstObject *objout ) {
/*
*  Name:
*     Copy

*  Purpose:
*     Copy constructor for Interval objects.

*  Type:
*     Private function.

*  Synopsis:
*     void Copy( const AstObject *objin, AstObject *objout )

*  Description:
*     This function implements the copy constructor for Region objects.

*  Parameters:
*     objin
*        Pointer to the object to be copied.
*     objout
*        Pointer to the object being constructed.

*  Notes:
*     -  This constructor makes a deep copy.
*/

/* Local Variables: */
   AstInterval *in;             /* Pointer to input Interval */
   AstInterval *out;            /* Pointer to output Interval */
   size_t nb;                   /* Number of bytes in limits array */

/* Check the global error status. */
   if ( !astOK ) return;

/* Obtain pointers to the input and output Intervals. */
   in = (AstInterval *) objin;
   out = (AstInterval *) objout;

/* For safety, first clear any references to the input memory from
   the output Interval. */
   out->box = NULL;
   out->lbnd = NULL;
   out->ubnd = NULL;

/* Note the number of bytes in each limits array */
   nb = sizeof( double )*(size_t) astGetNin( ((AstRegion *) in)->frameset );

/* Copy dynamic memory contents */
   if( in->box ) out->box = astCopy( in->box );
   out->lbnd = astStore( NULL, in->lbnd, nb );
   out->ubnd = astStore( NULL, in->ubnd, nb );
}


/* Destructor. */
/* ----------- */
static void Delete( AstObject *obj ) {
/*
*  Name:
*     Delete

*  Purpose:
*     Destructor for Interval objects.

*  Type:
*     Private function.

*  Synopsis:
*     void Delete( AstObject *obj )

*  Description:
*     This function implements the destructor for Interval objects.

*  Parameters:
*     obj
*        Pointer to the object to be deleted.

*  Notes:
*     This function attempts to execute even if the global error status is
*     set.
*/

/* Local Variables: */
   AstInterval *this;                 /* Pointer to Interval */

/* Obtain a pointer to the Interval structure. */
   this = (AstInterval *) obj;

/* Annul all resources. */
   if( this->box ) this->box = astAnnul( this->box );
   this->lbnd = astFree( this->lbnd );
   this->ubnd = astFree( this->ubnd );
}

/* Dump function. */
/* -------------- */
static void Dump( AstObject *this_object, AstChannel *channel ) {
/*
*  Name:
*     Dump

*  Purpose:
*     Dump function for Interval objects.

*  Type:
*     Private function.

*  Synopsis:
*     void Dump( AstObject *this, AstChannel *channel )

*  Description:
*     This function implements the Dump function which writes out data
*     for the Interval class to an output Channel.

*  Parameters:
*     this
*        Pointer to the Interval whose data are being written.
*     channel
*        Pointer to the Channel to which the data are being written.
*/

/* Local Variables: */
   AstInterval *this;                 /* Pointer to the Interval structure */

/* Check the global error status. */
   if ( !astOK ) return;

/* Obtain a pointer to the Interval structure. */
   this = (AstInterval *) this_object;

/* Write out values representing the instance variables for the
   Interval class.  Accompany these with appropriate comment strings,
   possibly depending on the values being written.*/

/* In the case of attributes, we first use the appropriate (private)
   Test...  member function to see if they are set. If so, we then use
   the (private) Get... function to obtain the value to be written
   out.

   For attributes which are not set, we use the astGet... method to
   obtain the value instead. This will supply a default value
   (possibly provided by a derived class which over-rides this method)
   which is more useful to a human reader as it corresponds to the
   actual default attribute value.  Since "set" will be zero, these
   values are for information only and will not be read back. */

/* There are no values to write, so return without further action. */
}

/* Standard class functions. */
/* ========================= */
/* Implement the astIsAInterval and astCheckInterval functions using the macros
   defined for this purpose in the "object.h" header file. */
astMAKE_ISA(Interval,Region,check,&class_init)
astMAKE_CHECK(Interval)

AstInterval *astInterval_( void *frame_void, const double lbnd[], 
                           const double ubnd[], AstRegion *unc, 
                           const char *options, ... ) {
/*
*++
*  Name:
c     astInterval
f     AST_INTERVAL

*  Purpose:
*     Create a Interval.

*  Type:
*     Public function.

*  Synopsis:
c     #include "interval.h"
c     AstInterval *astInterval( AstFrame *frame, const double lbnd[], 
c                               const double ubnd[], AstRegion *unc, 
c                               const char *options, ... ) 
f     RESULT = AST_INTERVAL( FRAME, LBND, UBND, UNC, OPTIONS, STATUS )

*  Class Membership:
*     Interval constructor.

*  Description:
*     This function creates a new Interval and optionally initialises its
*     attributes.
*
*     A Interval is a Region which represents upper and/or lower limits on 
*     one or more axes of a Frame. For a point to be within the region 
*     represented by the Interval, the point must satisfy all the 
*     restrictions placed on all the axes. The point is outside the region 
*     if it fails to satisfy any one of the restrictions. Each axis may have 
*     either an upper limit, a lower limit, both or neither. If both limits 
*     are supplied but are in reverse order (so that the lower limit is 
*     greater than the upper limit), then the interval is an excluded 
*     interval, rather than an included interval.
*
*     At least one axis limit must be supplied.

*  Parameters:
c     frame
f     FRAME = INTEGER (Given)
*        A pointer to the Frame in which the region is defined. A deep
*        copy is taken of the supplied Frame. This means that any
*        subsequent changes made to the Frame using the supplied pointer
*        will have no effect the Region.
c     lbnd
f     LBND( * ) = DOUBLE PRECISION (Given)
c        An array of double, with one element for each Frame axis
f        An array with one element for each Frame axis
*        (Naxes attribute) containing the lower limits on each axis.
*        Set a value to AST__BAD to indicate that the axis has no lower 
*        limit.
c     ubnd
f     UBND( * ) = DOUBLE PRECISION (Given)
c        An array of double, with one element for each Frame axis
f        An array with one element for each Frame axis
*        (Naxes attribute) containing the upper limits on each axis.
*        Set a value to AST__BAD to indicate that the axis has no upper
*        limit.
c     unc
f     UNC = INTEGER (Given)
*        An optional pointer to an existing Region which specifies the uncertainties 
*        associated with the boundary of the Interval being created. The 
*        uncertainty in any point on the boundary of the Interval is found by 
*        shifting the supplied "uncertainty" Region so that it is centred at 
*        the boundary point being considered. The area covered by the
*        shifted uncertainty Region then represents the uncertainty in the
*        boundary position. The uncertainty is assumed to be the same for
*        all points.
*
*        If supplied, the uncertainty Region must be either a Box, a Interval 
*        or an Interval. A deep copy of the supplied Region will be taken,
*        so subsequent changes to the uncertainty Region using the supplied 
*        pointer will have no effect on the created Interval. Alternatively, 
f        a null Object pointer (AST__NULL) 
c        a NULL Object pointer 
*        may be supplied, in which case a default uncertainty is used 
*        equivalent to a Box which has zero width on any axes for which
*        either the upper or lower limit has not been specifed, and which
*        has a width of 1.0E-6 of the axis interval on any axis for which
*        both limits have been supplied.
c     options
f     OPTIONS = CHARACTER * ( * ) (Given)
c        Pointer to a null-terminated string containing an optional
c        comma-separated list of attribute assignments to be used for
c        initialising the new Interval. The syntax used is identical to
c        that for the astSet function and may include "printf" format
c        specifiers identified by "%" symbols in the normal way.
f        A character string containing an optional comma-separated
f        list of attribute assignments to be used for initialising the
f        new Interval. The syntax used is identical to that for the
f        AST_SET routine.
c     ...
c        If the "options" string contains "%" format specifiers, then
c        an optional list of additional arguments may follow it in
c        order to supply values to be substituted for these
c        specifiers. The rules for supplying these are identical to
c        those for the astSet function (and for the C "printf"
c        function).
f     STATUS = INTEGER (Given and Returned)
f        The global status.

*  Returned Value:
c     astInterval()
f     AST_INTERVAL = INTEGER
*        A pointer to the new Interval.

*  Notes:
*     - A null Object pointer (AST__NULL) will be returned if this
c     function is invoked with the AST error status set, or if it
f     function is invoked with STATUS set to an error value, or if it
*     should fail for any reason.
*--
*/

/* Local Variables: */
   AstFrame *frame;              /* Pointer to Frame structure */
   AstInterval *new;             /* Pointer to new Interval */
   va_list args;                 /* Variable argument list */

/* Check the global status. */
   if ( !astOK ) return NULL;

/* Obtain and validate a pointer to the supplied Frame structure. */
   frame = astCheckFrame( frame_void );

/* Initialise the Interval, allocating memory and initialising the
   virtual function table as well if necessary. */
   new = astInitInterval( NULL, sizeof( AstInterval ), !class_init, 
                          &class_vtab, "Interval", frame, lbnd, ubnd, unc );

/* If successful, note that the virtual function table has been
   initialised. */
   if ( astOK ) {
      class_init = 1;

/* Obtain the variable argument list and pass it along with the options string
   to the astVSet method to initialise the new Interval's attributes. */
      va_start( args, options );
      astVSet( new, options, args );
      va_end( args );

/* If an error occurred, clean up by deleting the new object. */
      if ( !astOK ) new = astDelete( new );
   }

/* Return a pointer to the new Interval. */
   return new;
}

AstInterval *astIntervalId_( void *frame_void, const double lbnd[], 
                             const double ubnd[], void *unc_void,
                             const char *options, ... ) {
/*
*  Name:
*     astIntervalId_

*  Purpose:
*     Create a Interval.

*  Type:
*     Private function.

*  Synopsis:
*     #include "interval.h"
*     AstInterval *astIntervalId_( AstFrame *frame, const double lbnd[], 
*                                  const double ubnd[], AstRegion *unc,
*                                  const char *options, ... )

*  Class Membership:
*     Interval constructor.

*  Description:
*     This function implements the external (public) interface to the
*     astInterval constructor function. It returns an ID value (instead
*     of a true C pointer) to external users, and must be provided
*     because astInterval_ has a variable argument list which cannot be
*     encapsulated in a macro (where this conversion would otherwise
*     occur).
*
*     The variable argument list also prevents this function from
*     invoking astInterval_ directly, so it must be a re-implementation
*     of it in all respects, except for the final conversion of the
*     result to an ID value.

*  Parameters:
*     As for astInterval_.

*  Returned Value:
*     The ID value associated with the new Interval.
*/

/* Local Variables: */
   AstFrame *frame;              /* Pointer to Frame structure */
   AstInterval *new;             /* Pointer to new Interval */
   AstRegion *unc;               /* Pointer to Region structure */
   va_list args;                 /* Variable argument list */

/* Check the global status. */
   if ( !astOK ) return NULL;

/* Obtain a Frame pointer from the supplied ID and validate the
   pointer to ensure it identifies a valid Frame. */
   frame = astCheckFrame( astMakePointer( frame_void ) );

/* Obtain a Region pointer from the supplied "unc" ID and validate the
   pointer to ensure it identifies a valid Region . */
   unc = unc_void ? astCheckRegion( astMakePointer( unc_void ) ) : NULL;

/* Initialise the Interval, allocating memory and initialising the
   virtual function table as well if necessary. */
   new = astInitInterval( NULL, sizeof( AstInterval ), !class_init, &class_vtab,
                        "Interval", frame, lbnd, ubnd, unc );

/* If successful, note that the virtual function table has been
   initialised. */
   if ( astOK ) {
      class_init = 1;

/* Obtain the variable argument list and pass it along with the options string
   to the astVSet method to initialise the new Interval's attributes. */
      va_start( args, options );
      astVSet( new, options, args );
      va_end( args );

/* If an error occurred, clean up by deleting the new object. */
      if ( !astOK ) new = astDelete( new );
   }

/* Return an ID value for the new Interval. */
   return astMakeId( new );
}

AstInterval *astInitInterval_( void *mem, size_t size, int init, AstIntervalVtab *vtab, 
                               const char *name, AstFrame *frame, 
                               const double lbnd[], const double ubnd[],
                               AstRegion *unc ) {
/*
*+
*  Name:
*     astInitInterval

*  Purpose:
*     Initialise a Interval.

*  Type:
*     Protected function.

*  Synopsis:
*     #include "interval.h"
*     AstInterval *astInitInterval_( void *mem, size_t size, int init, AstIntervalVtab *vtab, 
*                                    const char *name, AstFrame *frame, 
*                                    const double lbnd[], const double ubnd[],
*                                    AstRegion *unc )

*  Class Membership:
*     Interval initialiser.

*  Description:
*     This function is provided for use by class implementations to initialise
*     a new Interval object. It allocates memory (if necessary) to accommodate
*     the Interval plus any additional data associated with the derived class.
*     It then initialises a Interval structure at the start of this memory. If
*     the "init" flag is set, it also initialises the contents of a virtual
*     function table for a Interval at the start of the memory passed via the
*     "vtab" parameter.

*  Parameters:
*     mem
*        A pointer to the memory in which the Interval is to be initialised.
*        This must be of sufficient size to accommodate the Interval data
*        (sizeof(Interval)) plus any data used by the derived class. If a value
*        of NULL is given, this function will allocate the memory itself using
*        the "size" parameter to determine its size.
*     size
*        The amount of memory used by the Interval (plus derived class data).
*        This will be used to allocate memory if a value of NULL is given for
*        the "mem" parameter. This value is also stored in the Interval
*        structure, so a valid value must be supplied even if not required for
*        allocating memory.
*     init
*        A logical flag indicating if the Interval's virtual function table is
*        to be initialised. If this value is non-zero, the virtual function
*        table will be initialised by this function.
*     vtab
*        Pointer to the start of the virtual function table to be associated
*        with the new Interval.
*     name
*        Pointer to a constant null-terminated character string which contains
*        the name of the class to which the new object belongs (it is this
*        pointer value that will subsequently be returned by the astGetClass
*        method).
*     frame
*        A pointer to the Frame in which the region is defined.
*     lbnd
*        An array of double, with one element for each Frame axis
*        (Naxes attribute) containing the lower limits on each axis.
*        Set a value to AST__BAD to indicate that the axis has no lower 
*        limit. Upper and ower limits can be reversed to create an
*        excluded interval rather than an included interval.
*     ubnd
*        An array of double, with one element for each Frame axis
*        (Naxes attribute) containing the upper limits on each axis.
*        Set a value to AST__BAD to indicate that the axis has no upper
*        limit.
*     unc
*        A pointer to a Region which specifies the uncertainty in the
*        supplied positions (all points on the boundary of the new Interval
*        being initialised are assumed to have the same uncertainty). A NULL 
*        pointer can be supplied, in which case default uncertainties equal to 
*        1.0E-6 of the dimensions of the new Interval's bounding box are used. 
*        If an uncertainty Region is supplied, it must be either a Box, a 
*        Circle or an Ellipse, and its encapsulated Frame must be related
*        to the Frame supplied for parameter "frame" (i.e. astConvert
*        should be able to find a Mapping between them). Two positions 
*        the "frame" Frame are considered to be co-incident if their 
*        uncertainty Regions overlap. The centre of the supplied
*        uncertainty Region is immaterial since it will be re-centred on the 
*        point being tested before use. A deep copy is taken of the supplied 
*        Region.

*  Returned Value:
*     A pointer to the new Interval.

*  Notes:
*     -  A null pointer will be returned if this function is invoked with the
*     global error status set, or if it should fail for any reason.
*-
*/

/* Local Variables: */
   AstInterval *new;         /* Pointer to new Interval */
   AstPointSet *pset;        /* PointSet to pass to Region initialiser */
   double **ptr;             /* Pointer to coords data in pset */
   int isNull;               /* Were no axis limits supplied? */
   int i;                    /* Axis index */
   int nc;                   /* No. of axes */

/* Check the global status. */
   if ( !astOK ) return NULL;

/* If necessary, initialise the virtual function table. */
   if ( init ) astInitIntervalVtab( vtab, name );

/* Initialise. */
   new = NULL;

/* Get the number of axis values required for each position. */
   nc = astGetNaxes( frame );

/* Create a PointSet to hold the upper and lower bounds, and get pointers to 
   the data arrays. */
   pset = astPointSet( 2, nc, "" );
   ptr = astGetPoints( pset );
   if( astOK ) {

/* Check we have at least one axis limit and copy the limits into the
   PointSet.  */
      isNull = 1;
      for( i = 0; i < nc; i++ ) {
         if( lbnd[ i ] != AST__BAD || ubnd[ i ] != AST__BAD ) isNull = 0;
         ptr[ i ][ 0 ] = lbnd[ i ];
         ptr[ i ][ 1 ] = ubnd[ i ];
      }

/* Report an error if no limits were supplied. */
      if( isNull ) astError( AST__BADIN, "astInitInterval(%s): No axis "
                             "limits were supplied for the new Interval "
                             "region.", name );

/* Initialise a Region structure (the parent class) as the first component
   within the Interval structure, allocating memory if necessary. */
      new = (AstInterval *) astInitRegion( mem, size, 0, (AstRegionVtab *) vtab, 
                                           name, frame, pset, unc );

      if ( astOK ) {

/* Initialise the Interval data. */
/* ----------------------------- */
         new->lbnd = NULL;
         new->ubnd = NULL;

         Cache( new );

/* If an error occurred, clean up by deleting the new Interval. */
         if ( !astOK ) new = astDelete( new );
      }
   }

/* Free resources. */
   pset = astAnnul( pset );

/* Return a pointer to the new Interval. */
   return new;
}

AstInterval *astLoadInterval_( void *mem, size_t size, AstIntervalVtab *vtab, 
                               const char *name, AstChannel *channel ) {
/*
*+
*  Name:
*     astLoadInterval

*  Purpose:
*     Load a Interval.

*  Type:
*     Protected function.

*  Synopsis:
*     #include "interval.h"
*     AstInterval *astLoadInterval( void *mem, size_t size, AstIntervalVtab *vtab, 
*                                   const char *name, AstChannel *channel )

*  Class Membership:
*     Interval loader.

*  Description:
*     This function is provided to load a new Interval using data read
*     from a Channel. It first loads the data used by the parent class
*     (which allocates memory if necessary) and then initialises a
*     Interval structure in this memory, using data read from the input
*     Channel.
*
*     If the "init" flag is set, it also initialises the contents of a
*     virtual function table for a Interval at the start of the memory
*     passed via the "vtab" parameter.

*  Parameters:
*     mem
*        A pointer to the memory into which the Interval is to be
*        loaded.  This must be of sufficient size to accommodate the
*        Interval data (sizeof(Interval)) plus any data used by derived
*        classes. If a value of NULL is given, this function will
*        allocate the memory itself using the "size" parameter to
*        determine its size.
*     size
*        The amount of memory used by the Interval (plus derived class
*        data).  This will be used to allocate memory if a value of
*        NULL is given for the "mem" parameter. This value is also
*        stored in the Interval structure, so a valid value must be
*        supplied even if not required for allocating memory.
*
*        If the "vtab" parameter is NULL, the "size" value is ignored
*        and sizeof(AstInterval) is used instead.
*     vtab
*        Pointer to the start of the virtual function table to be
*        associated with the new Interval. If this is NULL, a pointer
*        to the (static) virtual function table for the Interval class
*        is used instead.
*     name
*        Pointer to a constant null-terminated character string which
*        contains the name of the class to which the new object
*        belongs (it is this pointer value that will subsequently be
*        returned by the astGetClass method).
*
*        If the "vtab" parameter is NULL, the "name" value is ignored
*        and a pointer to the string "Interval" is used instead.

*  Returned Value:
*     A pointer to the new Interval.

*  Notes:
*     - A null pointer will be returned if this function is invoked
*     with the global error status set, or if it should fail for any
*     reason.
*-
*/

/* Local Variables: */
   AstInterval *new;              /* Pointer to the new Interval */

/* Initialise. */
   new = NULL;

/* Check the global error status. */
   if ( !astOK ) return new;

/* If a NULL virtual function table has been supplied, then this is
   the first loader to be invoked for this Interval. In this case the
   Interval belongs to this class, so supply appropriate values to be
   passed to the parent class loader (and its parent, etc.). */
   if ( !vtab ) {
      size = sizeof( AstInterval );
      vtab = &class_vtab;
      name = "Interval";

/* If required, initialise the virtual function table for this class. */
      if ( !class_init ) {
         astInitIntervalVtab( vtab, name );
         class_init = 1;
      }
   }

/* Invoke the parent class loader to load data for all the ancestral
   classes of the current one, returning a pointer to the resulting
   partly-built Interval. */
   new = astLoadRegion( mem, size, (AstRegionVtab *) vtab, name,
                        channel );

   if ( astOK ) {

/* Read input data. */
/* ================ */
/* Request the input Channel to read all the input data appropriate to
   this class into the internal "values list". */
      astReadClassData( channel, "Interval" );

/* Now read each individual data item from this list and use it to
   initialise the appropriate instance variable(s) for this class. */

/* In the case of attributes, we first read the "raw" input value,
   supplying the "unset" value as the default. If a "set" value is
   obtained, we then use the appropriate (private) Set... member
   function to validate and set the value properly. */

/* There are no values to read. */
/* ---------------------------- */
      new->lbnd = NULL;
      new->ubnd = NULL;

/* Cache intermediate results in the Interval structure */
      Cache( new );

/* If an error occurred, clean up by deleting the new Interval. */
      if ( !astOK ) new = astDelete( new );
   }

/* Return the new Interval pointer. */
   return new;
}

/* Virtual function interfaces. */
/* ============================ */
/* These provide the external interface to the virtual functions defined by
   this class. Each simply checks the global error status and then locates and
   executes the appropriate member function, using the function pointer stored
   in the object's virtual function table (this pointer is located using the
   astMEMBER macro defined in "object.h").

   Note that the member function may not be the one defined here, as it may
   have been over-ridden by a derived class. However, it should still have the
   same interface. */

