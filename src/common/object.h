/* Copyright (c) 1999 Silicon Graphics, Inc. */

/* SCCSid "$SunId$ SGI" */

/*
 *  object.h - header file for routines using objects and object sets.
 *
 *     7/28/85
 */

/*
 *	Object definitions require general specifications
 *	which may include a number of different argument types.
 *	The following structure aids in the storage of such
 *	argument lists.
 */

typedef struct {
	short  nsargs;			/* # of string arguments */
	short  nfargs;			/* # of real arguments */
	char  **sarg;			/* string arguments */
	FLOAT  *farg;			/* real arguments */
#ifdef  IARGS
	short  niargs;			/* # of integer arguments */
	long  *iarg;			/* integer arguments */
#endif
}  FUNARGS;

#define  MAXSTR		128		/* maximum string length */

/*
 *	An object is defined as an index into an array of
 *	structures containing the object type and specification
 *	and the modifier index.
 */

#ifndef  OBJECT
#ifdef  BIGMEM
#define  OBJECT		int4		/* index to object array */
#else
#define  OBJECT		int2		/* index to object array */
#endif
#endif

typedef struct {
	OBJECT  omod;			/* modifier number */
	short  otype;			/* object type number */
	char  *oname;			/* object name */
	FUNARGS  oargs;			/* object specification */
	char  *os;			/* object structure */
}  OBJREC;

#ifndef  MAXOBJBLK
#ifdef  BIGMEM
#define  MAXOBJBLK	65535		/* maximum number of object blocks */
#else
#define  MAXOBJBLK	63		/* maximum number of object blocks */
#endif
#endif

extern OBJREC  *objblock[MAXOBJBLK];	/* the object blocks */
extern OBJECT  nobjects;		/* # of objects */

#define  OBJBLKSHFT	9
#define  OBJBLKSIZ	(1<<OBJBLKSHFT)	/* object block size */
#define  objptr(obj)	(objblock[(obj)>>OBJBLKSHFT]+((obj)&(OBJBLKSIZ-1)))

#define  OVOID		(-1)		/* void object */
#define  VOIDID		"void"		/* void identifier */

/*
 *     Object sets begin with the number of objects and proceed with
 *  the objects in ascending order.
 */

#define  MAXSET		127		/* maximum object set size */

OBJECT	*setsave();

#define setfree(os)	free((char *)(os))
