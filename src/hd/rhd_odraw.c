/* Copyright (c) 1998 Silicon Graphics, Inc. */

#ifndef lint
static char SCCSid[] = "$SunId$ SGI";
#endif

/*
 * Routines for drawing samples using depth buffer checks.
 */

#include "standard.h"

#include <sys/types.h>
#include <GL/glx.h>
#include <GL/glu.h>

#include "random.h"
#include "rhd_odraw.h"

#ifndef DEPTHEPS
#define DEPTHEPS	0.02		/* depth epsilon */
#endif
#ifndef SAMPSPERBLOCK
#define SAMPSPERBLOCK	1024		/* target samples per image block */
#endif
#ifndef SFREEFRAC
#define SFREEFRAC	0.2		/* fraction to free at a time */
#endif
#ifndef MAXFAN
#define MAXFAN		32		/* maximum arms in a triangle fan */
#endif
#ifndef MINFAN
#define MINFAN		4		/* minimum arms in a triangle fan */
#endif
#ifndef FANSIZE
#define FANSIZE		3.5		/* fan sizing factor */
#endif

#define NEWMAP		01		/* need to recompute mapping */
#define NEWRGB		02		/* need to remap RGB values */

struct ODview	*odView;	/* our view list */
int	odNViews;		/* number of views in our list */

struct ODsamp	odS;		/* sample values */

static int	needmapping;	/* what needs doing with tone map */


#define SAMP32	(32*(2*sizeof(short)+sizeof(union ODfunion)+sizeof(TMbright)+\
			6*sizeof(BYTE))+sizeof(int4))

int
odInit(n)				/* initialize drawing routines */
int	n;
{
	int	nbytes, i, j, k, nextsamp, count, blockdiv;
	int	res[2];

	if (odNViews) {			/* deallocate view structures */
		for (i = 0; i < odNViews; i++) {
			free((char *)odView[i].bmap);
			if (odView[i].emap != NULL)
				free((char *)odView[i].emap);
		}
		free((char *)odView);
		odView = NULL;
		odNViews = 0;
	}
	if (n && n != odS.nsamp) {
				/* round space up to nearest power of 2 */
		nbytes = (n+31)/32 * SAMP32;
		for (i = 1024; nbytes > i-8; i <<= 1)
			;
		n = (i-8)/SAMP32 * 32;
		needmapping = NEWMAP;
	}
	if (n != odS.nsamp) {	/* (re)allocate sample array */
		if (odS.nsamp)
			free(odS.base);
		odS.nsamp = 0;
		if (!n)
			return(0);
		nbytes = (n+31)/32 * SAMP32;
		odS.base = (char *)malloc(nbytes);
		if (odS.base == NULL)
			return(0);
				/* assign larger alignment types earlier */
		odS.f = (union ODfunion *)odS.base;
		odS.redraw = (int4 *)(odS.f + n);
		odS.ip = (short (*)[2])(odS.redraw + n/32);
		odS.brt = (TMbright *)(odS.ip + n);
		odS.chr = (BYTE (*)[3])(odS.brt + n);
		odS.rgb = (BYTE (*)[3])(odS.chr + n);
		odS.nsamp = n;
	}
	if (!n)
		return(0);
				/* allocate view information */
	count = 0;			/* count pixels */
	for (i = 0; dev_auxview(i, res) != NULL; i++)
		count += res[0]*res[1];
	odView = (struct ODview *)malloc(i*sizeof(struct ODview));
	if (odView == NULL)
		return(0);
	odNViews = i;
	blockdiv = sqrt(count/(n/SAMPSPERBLOCK)) + 0.5;
	if (blockdiv < 8) blockdiv = 8;
	nextsamp = 0; count /= blockdiv*blockdiv;	/* # blocks */
	while (i--) {			/* initialize each view */
		odView[i].emap = NULL;
		odView[i].dmap = NULL;
		dev_auxview(i, res);
		odView[i].hhi = res[0];
		odView[i].hlow = (res[0] + blockdiv/2) / blockdiv;
		if (odView[i].hlow < 1) odView[i].hlow = 1;
		odView[i].vhi = res[1];
		odView[i].vlow = (res[1] + blockdiv/2) / blockdiv;
		if (odView[i].vlow < 1) odView[i].vlow = 1;
		j = odView[i].hlow*odView[i].vlow;
		odView[i].bmap = (struct ODblock *)malloc(
				j * sizeof(struct ODblock));
		if (odView[i].bmap == NULL)
			return(0);
		DCHECK(count<=0 | nextsamp>=n,
				CONSISTENCY, "counter botch in odInit");
		if (!i) count = j;
		while (j--) {
			odView[i].bmap[j].first = k = nextsamp;
			nextsamp += odView[i].bmap[j].nsamp = 
				(n - nextsamp)/count--;
			odView[i].bmap[j].free = k;
			while (++k < nextsamp)
				odS.nextfree(k-1) = k;
			odS.nextfree(k-1) = ENDFREE;
			odView[i].bmap[j].nused = 0;
		}
	}
	CLR4ALL(odS.redraw, odS.nsamp);		/* clear redraw flags */
	for (i = odS.nsamp; i--; )		/* clear values */
		odS.ip[i][0] = odS.ip[i][1] = -1;
	return(odS.nsamp);			/* return number of samples */
}

#undef SAMP32


int
sampcmp(s0, s1)			/* sample order, descending proximity */
int	*s0, *s1;
{
	register double	diff = odS.closeness(*s1) - odS.closeness(*s0);

	return (diff > FTINY ? 1 : diff < -FTINY ? -1 : 0);
}


int
odAllocBlockSamp(vn, hl, vl)		/* allocate sample from block */
int	vn, hl, vl;
{
	int	si[SAMPSPERBLOCK+SAMPSPERBLOCK/4];
	VIEW	*vw;
	FVECT	ro, rd;
	int	res[2];
	register struct ODblock	*bp;
	register int	i;

	bp = odView[vn].bmap + vl*odView[vn].hlow + hl;
	if (bp->free != ENDFREE) {	/* check free list first */
		i = bp->free;
		bp->free = odS.nextfree(i);
		bp->nused++;
		return(i);
	}
	DCHECK(bp->nsamp<=0, CONSISTENCY,
			"no available samples in odAllocBlockSamp");
	DCHECK(bp->nsamp > sizeof(si)/sizeof(si[0]), CONSISTENCY,
			"too many samples in odAllocBlockSamp");
					/* free some samples */
	if ((vw = dev_auxview(vn, res)) == NULL)
		error(CONSISTENCY, "bad view number in odAllocBlockSamp");
	for (i = bp->nsamp; i--; )	/* figure out which are worse */
		si[i] = bp->first + i;
	qsort((char *)si, bp->nsamp, sizeof(int), sampcmp);
	i = bp->nsamp*SFREEFRAC + .5;	/* put them in a list */
	while (--i > 0) {
		odS.nextfree(si[i]) = bp->free;
		bp->free = si[i];
		bp->nused--;
	}
	return(si[0]);			/* return first free sample */
}


odSample(c, d, p)			/* add a sample value */
COLR	c;
FVECT	d, p;
{
	FVECT	disp;
	double	d0, d1, h, v;
	register VIEW	*vw;
	int	hl, vl, hh, vh;
	int	res[2];
	register int	i, id;

	DCHECK(odS.nsamp<=0, CONSISTENCY, "no samples allocated in odSample");
						/* add value to each view */
	for (i = 0; (vw = dev_auxview(i, res)) != NULL; i++) {
		DCHECK(i>=odNViews, CONSISTENCY, "too many views in odSample");
		CHECK(vw->type!=VT_PER, INTERNAL,
				"cannot handle non-perspective views");
		if (p != NULL) {		/* compute view position */
			VSUB(disp, p, vw->vp);
			d0 = DOT(disp, vw->vdir);
			if (d0 <= vw->vfore+FTINY)
				continue;		/* too close */
		} else {
			VCOPY(disp, d);
			d0 = DOT(disp, vw->vdir);
			if (d0 <= FTINY)		/* behind view */
				continue;
		}
		h = DOT(disp,vw->hvec)/(d0*vw->hn2) + 0.5 - vw->hoff;
		if (h < 0. || h >= 1.)
			continue;			/* left or right */
		v = DOT(disp,vw->vvec)/(d0*vw->vn2) + 0.5 - vw->voff;
		if (v < 0. || v >= 1.)
			continue;			/* above or below */
		hh = h * res[0];
		vh = v * res[1];
		if (odView[i].dmap != NULL) {		/* check depth */
			d1 = odView[i].dmap[vh*res[0] + hh];
			if (d1 < 0.99*FHUGE && (d0 > (1.+DEPTHEPS)*d1 ||
						(1.+DEPTHEPS)*d0 < d1))
			continue;			/* occlusion error */
		}
		hl = hh*odView[i].hlow/res[0];
		vl = vh*odView[i].vlow/res[1];
						/* may duplicate samples */
		id = odAllocBlockSamp(i, hl, vl);
		odS.ip[id][0] = hh;
		odS.ip[id][1] = vh;
		if (p != NULL) {		/* compute closeness (sin^2) */
			d1 = DOT(disp, d);
			odS.closeness(id) = 1. - d1*d1/DOT(disp,disp);
		} else
			odS.closeness(id) = 0.;
							/* convert color */
		tmCvColrs(&odS.brt[id], odS.chr[id], c, 1);
		if (imm_mode | needmapping)		/* if immediate mode */
			needmapping |= NEWRGB;		/* map it later */
		else					/* else map it now */
			tmMapPixels(odS.rgb[id], &odS.brt[id], odS.chr[id], 1);
		SET4(odS.redraw, id);			/* mark for redraw */
	}
}


odRemap()				/* recompute tone mapping */
{
	needmapping |= NEWMAP|NEWRGB;
}


odRedraw(vn, hmin, vmin, hmax, vmax)	/* redraw view region */
int	vn, hmin, vmin, hmax, vmax;
{
	int	i, j;
	register struct ODblock	*bp;
	register int	k;

	if (vn<0 | vn>=odNViews)
		return;
				/* check view limits */
	if (hmin < 0) hmin = 0;
	if (hmax >= odView[vn].hhi) hmax = odView[vn].hhi-1;
	if (vmin < 0) vmin = 0;
	if (vmax >= odView[vn].vhi) vmax = odView[vn].vhi-1;
	if (hmax <= hmin | vmax <= vmin)
		return;
				/* convert to low resolution */
	hmin = hmin * odView[vn].hlow / odView[vn].hhi;
	hmax = hmax * odView[vn].hlow / odView[vn].hhi;
	vmin = vmin * odView[vn].vlow / odView[vn].vhi;
	vmax = vmax * odView[vn].vlow / odView[vn].vhi;
				/* mark block samples for redraw, inclusive */
	for (i = hmin; i <= hmax; i++)
		for (j = vmin; j <= vmax; j++) {
			bp = odView[vn].bmap + j*odView[vn].hlow + i;
			for (k = bp->nsamp; k--; )
				if (odS.ip[bp->first+k][0] >= 0)
					SET4(odS.redraw, bp->first+k);
		}
}


odDepthMap(vn, dm)			/* assign depth map for view */
int	vn;
GLfloat	*dm;
{
	double	d0, d1;
	int	i, j, hmin, hmax, vmin, vmax;
	register int	k, l;

	DCHECK(vn<0 | vn>=odNViews, CONSISTENCY,
			"bad view number in odDepthMap");
	if (dm == NULL) {			/* free edge map */
		if (odView[vn].emap != NULL)
			free((char *)odView[vn].emap);
		odView[vn].emap = NULL;
		odView[vn].dmap = NULL;
		return;
	}
	odView[vn].dmap = dm;			/* initialize edge map */
	if (odView[vn].emap == NULL) {
		odView[vn].emap = (int4 *)malloc(
			FL4NELS(odView[vn].hlow*odView[vn].vlow)*sizeof(int4));
		if (odView[vn].emap == NULL)
			error(SYSTEM, "out of memory in odDepthMap");
	}
	CLR4ALL(odView[vn].emap, odView[vn].hlow*odView[vn].vlow);
						/* compute edge map */
	vmin = odView[vn].vhi;			/* enter loopsville */
	for (j = odView[vn].vlow; j--; ) {
		vmax = vmin;
		vmin = j*odView[vn].vhi/odView[vn].vlow;
		hmin = odView[vn].hhi;
		for (i = odView[vn].hlow; i--; ) {
			hmax = hmin;
			hmin = i*odView[vn].hhi/odView[vn].hlow;
			for (l = vmin; l < vmax; l++) {	/* vertical edges */
				d1 = dm[l*odView[vn].hhi+hmin];
				for (k = hmin+1; k < hmax; k++) {
					d0 = d1;
					d1 = dm[l*odView[vn].hhi+k];
					if (d0 > (1.+DEPTHEPS)*d1 ||
						(1.+DEPTHEPS)*d0 < d1) {
						SET4(odView[vn].emap,
							j*odView[vn].hlow + i);
						break;
					}
				}
				if (k < hmax)
					break;
			}
			if (l < vmax)
				continue;
			for (k = hmin; k < hmax; k++) {	/* horizontal edges */
				d1 = dm[vmin*odView[vn].hhi+k];
				for (l = vmin+1; l < vmax; l++) {
					d0 = d1;
					d1 = dm[l*odView[vn].hhi+k];
					if (d0 > (1.+DEPTHEPS)*d1 ||
						(1.+DEPTHEPS)*d0 < d1) {
						SET4(odView[vn].emap,
							j*odView[vn].hlow + i);
						break;
					}
				}
				if (l < vmax)
					break;
			}
		}
	}
}


odUpdate(vn)				/* update this view */
int	vn;
{
	int	i, j;
	register struct ODblock	*bp;
	register int	k;

	DCHECK(vn<0 | vn>=odNViews, CONSISTENCY,
			"bad view number in odUpdate");
					/* need to do some tone mapping? */
	if (needmapping & NEWRGB) {
		if (needmapping & NEWMAP) {
			tmClearHisto();
			if (tmAddHisto(odS.brt,odS.nsamp,1) != TM_E_OK)
				return;
			if (tmComputeMapping(0.,0.,0.) != TM_E_OK)
				return;
			for (k = odS.nsamp; k--; )	/* redraw all */
				if (odS.ip[k][0] >= 0)
					SET4(odS.redraw, k);
		}
		if (tmMapPixels(odS.rgb,odS.brt,odS.chr,odS.nsamp) != TM_E_OK)
			return;
		needmapping = 0;		/* reset flag */
	}
					/* draw each block in view */
	for (i = odView[vn].hlow; i--; )
		for (j = odView[vn].vlow; j--; ) {
					/* get block */
			bp = odView[vn].bmap + j*odView[vn].hlow + i;
					/* do quick, conservative flag check */
			for (k = (bp->first+bp->nsamp+31)>>5;
					k-- > bp->first>>5; )
				if (odS.redraw[k])
					break;		/* non-zero flag */
			if (k < bp->first>>5)
				continue;		/* no flags set */
			for (k = bp->nsamp; k--; )	/* sample by sample */
				if (CHK4(odS.redraw, bp->first+k)) {
					odDrawBlockSamp(vn, i, j, bp->first+k);
					CLR4(odS.redraw, bp->first+k);
				}
		}
}


#if 0
static
clip_end(p, o, vp)			/* clip line segment to view */
GLshort	p[3];
short	o[2];
register struct ODview	*vp;
{
	if (p[0] < 0) {
		p[1] = -o[0]*(p[1]-o[1])/(p[0]-o[0]) + o[1];
		p[2] = -o[0]*p[2]/(p[0]-o[0]);
		p[0] = 0;
	} else if (p[0] >= vp->hhi) {
		p[1] = (vp->hhi-1-o[0])*(p[1]-o[1])/(p[0]-o[0]) + o[1];
		p[2] = (vp->hhi-1-o[0])*p[2]/(p[0]-o[0]);
		p[0] = vp->hhi-1;
	}
	if (p[1] < 0) {
		p[0] = -o[1]*(p[0]-o[0])/(p[1]-o[1]) + o[0];
		p[2] = -o[1]*p[2]/(p[1]-o[1]);
		p[1] = 0;
	} else if (p[1] >= vp->vhi) {
		p[0] = (vp->vhi-1-o[1])*(p[0]-o[0])/(p[1]-o[1]) + o[0];
		p[2] = (vp->vhi-1-o[1])*p[2]/(p[1]-o[1]);
		p[1] = vp->vhi-1;
	}
}
#endif


static int
make_arms(ar, cp, vp, sz)		/* make arms for triangle fan */
GLshort	ar[MAXFAN][3];
short	cp[2];
register struct ODview	*vp;
double	sz;
{
	int	na, dv;
	double	hrad, vrad, phi0, phi;
	register int	i;

	DCHECK(sz > 1, CONSISTENCY, "super-unary size in make_arms");
	na = MAXFAN*sz*sz + 0.5;		/* keep area constant */
	if (na < MINFAN) na = MINFAN;
	hrad = FANSIZE*sz*vp->hhi/vp->hlow;
	vrad = FANSIZE*sz*vp->vhi/vp->vlow;
	if (hrad*vrad < 2.25)
		hrad = vrad = 1.5;
	phi0 = (2.*PI) * frandom();
	dv = OMAXDEPTH*sz + 0.5;
	for (i = 0; i < na; i++) {
		phi = phi0 + (2.*PI)*i/na;
		ar[i][0] = cp[0] + tcos(phi)*hrad + 0.5;
		ar[i][1] = cp[1] + tsin(phi)*vrad + 0.5;
		ar[i][2] = dv;
		/* clip_end(ar[i], cp, vp); */
	}
	return(na);
}


static int
depthchange(vp, x0, y0, x1, y1)		/* check depth discontinuity */
register struct ODview	*vp;
int	x0, y0, x1, y1;
{
	register double	d0, d1;

	DCHECK(x0<0 | x0>=vp->hhi | y0<0 | y0>=vp->vhi,
			CONSISTENCY, "coordinates off view in depthchange");

	if (x1<0 | x1>=vp->hhi | y1<0 | y1>=vp->vhi)
		return(1);

	d0 = vp->dmap[y0*vp->hhi + x0];
	d1 = vp->dmap[y1*vp->hhi + x1];

	return((1.+DEPTHEPS)*d0 < d1 || d0 > (1.+DEPTHEPS)*d1);
}


static
clip_edge(p, o, vp)			/* clip line segment to depth edge */
GLshort	p[3];
short	o[2];
register struct ODview	*vp;
{
	int	x, y, xstep, ystep, rise, rise2, run, run2, n;

	DCHECK(vp->dmap==NULL, CONSISTENCY,
			"clip_edge called with no depth map");
	x = o[0]; y = o[1];
	run = p[0] - x;
	xstep = run > 0 ? 1 : -1;
	run *= xstep;
	rise = p[1] - y;
	ystep = rise > 0 ? 1 : -1;
	rise *= ystep;
	rise2 = run2 = 0;
	if (rise > run) rise2 = 1;
	else run2 = 1;
	n = rise + run;
	while (n--)			/* run out arm, checking depth */
		if (run2 > rise2) {
			if (depthchange(vp, x, y, x+xstep, y))
				break;
			x += xstep;
			rise2 += rise;
		} else {
			if (depthchange(vp, x, y, x, y+ystep))
				break;
			y += ystep;
			run2 += run;
		}
	if (n < 0)			/* found something? */
		return;
	if (run > rise)
		p[2] = (x - o[0])*p[2]/(p[0] - o[0]);
	else
		p[2] = (y - o[1])*p[2]/(p[1] - o[1]);
	p[0] = x;
	p[1] = y;
}


static int
getblock(vp, h, v)			/* get block index */
register struct ODview	*vp;
register int	h, v;
{
	if (h<0 | h>=vp->hhi | v<0 | v>=vp->vhi)
		return(-1);
	return(h*vp->hlow/vp->hhi + v*vp->vlow/vp->vhi*vp->hlow);
}


odDrawBlockSamp(vn, h, v, id)		/* draw sample in view block */
int	vn, h, v;
register int	id;
{
	GLshort	arm[MAXFAN][3];
	int	narms, blockindex, bi1;
	register struct ODview	*vp;
	double	size;
	int	home_edges;
	register int	i;

	vp = odView + vn;
	blockindex = v*vp->hlow + h;
	DCHECK(odS.ip[id][0]*vp->hlow/vp->hhi != h |
			odS.ip[id][1]*vp->vlow/vp->vhi != v,
			CONSISTENCY, "bad sample position in odDrawBlockSamp");
	DCHECK(vp->bmap[blockindex].nused <= 0,
			CONSISTENCY, "bad in-use count in odDrawBlockSamp");
					/* create triangle fan */
	size = 1./sqrt((double)vp->bmap[blockindex].nused);
	narms = make_arms(arm, odS.ip[id], vp, size);
	if (vp->emap != NULL) {		/* check for edge collisions */
		home_edges = CHK4(vp->emap, blockindex);
		for (i = 0; i < narms; i++)
			/* the following test is flawed, because we could
			 * be passing through a block on a diagonal run */
			if (home_edges ||
				( (bi1 = getblock(vp, arm[i][0], arm[i][1]))
						!= blockindex &&
					(bi1 < 0 || CHK4(vp->emap, bi1)) ))
				clip_edge(arm[i], odS.ip[id], vp);
	}
					/* draw triangle fan */
	glColor3ub(odS.rgb[id][0], odS.rgb[id][1], odS.rgb[id][2]);
	glBegin(GL_TRIANGLE_FAN);
	glVertex3s((GLshort)odS.ip[id][0], (GLshort)odS.ip[id][1], (GLshort)0);
	for (i = 0; i < narms; i++)
		glVertex3sv(arm[i]);
	glVertex3sv(arm[0]);		/* connect last to first */
	glEnd();
}