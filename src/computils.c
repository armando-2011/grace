/*
 * Grace - Graphics for Exploratory Data Analysis
 * 
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 * 
 * Copyright (c) 1991-95 Paul J Turner, Portland, OR
 * Copyright (c) 1996-98 GRACE Development Team
 * 
 * Maintained by Evgeny Stambulchik <fnevgeny@plasma-gate.weizmann.ac.il>
 * 
 * 
 *                           All Rights Reserved
 * 
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 * 
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 * 
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 *
 * procedures for performing transformations from the command
 * line interpreter and the GUI.
 *
 */

#include <config.h>
#include <cmath.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "globals.h"
#include "utils.h"
#include "graphs.h"
#include "protos.h"

static void forwarddiff(double *x, double *y, double *resx, double *resy, int n);
static void backwarddiff(double *x, double *y, double *resx, double *resy, int n);
static void centereddiff(double *x, double *y, double *resx, double *resy, int n);
int get_points_inregion(int rno, int invr, int len, double *x, double *y, int *cnt, double **xt, double **yt);

static char buf[256];

extern double result;

void do_running_command(int type, int setno, int rlen)
{
    switch (type) {
    case RUN_AVG:
	type = 0;
	break;
    case RUN_MED:
	type = 1;
	break;
    case RUN_MIN:
	type = 2;
	break;
    case RUN_MAX:
	type = 3;
	break;
    case RUN_STD:
	type = 4;
	break;
    }
    do_runavg(setno, rlen, type, -1, 0);
}

void do_fourier_command(int ftype, int setno, int ltype)
{
    switch (ftype) {
    case FFT_DFT:
	do_fourier(0, setno, 0, ltype, 0, 0, 0);
	break;
    case FFT_INVDFT:
	do_fourier(0, setno, 0, ltype, 1, 0, 0);
	break;
    case FFT_FFT:
	do_fourier(1, setno, 0, ltype, 0, 0, 0);
	break;
    case FFT_INVFFT:
	do_fourier(1, setno, 0, ltype, 1, 0, 0);
	break;
    }
}

/*
 * evaluate a formula
 */
int do_compute(int setno, int loadto, int graphto, char *fstr)
{
    if (graphto < 0) {
	graphto = get_cg();
    }
    if (strlen(fstr) == 0) {
	errmsg("Define formula first");
	return -1;
    }
    if (is_set_active(get_cg(), setno)) {
	/* both loadto and setno do double duty here */
	if (loadto) {
	    loadto = nextset(graphto);
	    if (loadto != -1) {
		do_copyset(get_cg(), setno, graphto, loadto);
		setno = loadto;
	    } else {
		return -1;
	    }
	} else if (graphto != get_cg()) {
	    loadto = setno;
	    if (is_set_active(graphto, loadto)) {
		killset(graphto, loadto);
	    }
	    do_copyset(get_cg(), setno, graphto, loadto);
	    setno = loadto;
	}
	if (formula(graphto, setno, fstr)) {
	    if (graphto != get_cg() || loadto != setno) {
		killset(graphto, loadto);
	    }
	    return -1;
	}
	if (!is_graph_active(graphto)) {
	    set_graph_active(graphto, TRUE);
	}
	return loadto;
    }
    return -1;
}

/*
 * load a set
 */
void do_load(int setno, int toval, double start, double step )
{
    int i, idraw = 0;

    if (setno == SET_SELECT_ALL) {
	for (i = 0; i < number_of_sets(get_cg()); i++) {
	    if (is_set_active(get_cg(), i)) {
		loadset(get_cg(), i, toval, start, step);
		idraw = 1;
	    }
	}
    } else if (is_set_active(get_cg(), setno)) {
	loadset(get_cg(), setno, toval, start, step);
	idraw = 1;
    }
    if (!idraw) {
	errmsg("Set(s) not active");
    }
}

/*
 * evaluate a formula loading the next set
 */
void do_compute2(int gno, char *fstrx, char *fstry, char *startstr, char *stopstr, int npts, int toval)
{
    int setno, ier;
    double start, stop, step, x, y, a, b, c, d;
    char comment[256];

    if (npts < 2) {
	errmsg("Number of points < 2");
	return;
    }
    /*
     * if npts is > maxarr, then increase length of scratch arrays
     */
    if (npts > maxarr) {
	if (init_scratch_arrays(npts)) {
	    return;
	}
    }
    setno = nextset(gno);
    if (setno < 0) {
	return;
    }
    activateset(gno, setno);
    setlength(gno, setno, npts);
    if (strlen(fstrx) == 0) {
	errmsg("Undefined expression for X");
	return;
    }
    if (strlen(fstry) == 0) {
	errmsg("Undefined expression for Y");
	return;
    }
    if (strlen(startstr) == 0) {
	errmsg("Start item undefined");
	return;
    }
    scanner(startstr, &x, &y, 1, &a, &b, &c, &d, 1, 0, 0, &ier);
    if (ier)
	return;
    start = result;

    if (strlen(stopstr) == 0) {
	errmsg("Stop item undefined");
	return;
    }
    scanner(stopstr, &x, &y, 1, &a, &b, &c, &d, 1, 0, 0, &ier);
    if (ier) {
	return;
    }
    stop = result;

    step = (stop - start) / (npts - 1);
    loadset(gno, setno, toval, start, step);
    strcpy(buf, "X=");
    strcat(buf, fstrx);
    strcat( strcpy( comment, buf ), ", " );
    formula(gno, setno, buf);
    strcpy(buf, "Y=");
    strcat(buf, fstry);
    formula(gno, setno, buf);
    strncat( comment, buf, 256-strlen(comment) );
    setcomment(gno, setno, comment );
}

/*
 * forward, backward and centered differences
 */
static void forwarddiff(double *x, double *y, double *resx, double *resy, int n)
{
    int i, eflag = 0;
    double h;

    for (i = 1; i < n; i++) {
	resx[i - 1] = x[i - 1];
	h = x[i - 1] - x[i];
	if (h == 0.0) {
	    resy[i - 1] = - MAXNUM;
	    eflag = 1;
	} else {
	    resy[i - 1] = (y[i - 1] - y[i]) / h;
	}
    }
    if (eflag) {
	errmsg("Warning: infinite slope, check set status before proceeding");
    }
}

static void backwarddiff(double *x, double *y, double *resx, double *resy, int n)
{
    int i, eflag = 0;
    double h;

    for (i = 0; i < n - 1; i++) {
	resx[i] = x[i];
	h = x[i + 1] - x[i];
	if (h == 0.0) {
	    resy[i] = - MAXNUM;
	    eflag = 1;
	} else {
	    resy[i] = (y[i + 1] - y[i]) / h;
	}
    }
    if (eflag) {
	errmsg("Warning: infinite slope, check set status before proceeding");
    }
}

static void centereddiff(double *x, double *y, double *resx, double *resy, int n)
{
    int i, eflag = 0;
    double h1, h2;

    for (i = 1; i < n - 1; i++) {
	resx[i - 1] = x[i];
	h1 = x[i] - x[i - 1];
	h2 = x[i + 1] - x[i];
	if (h1 + h2 == 0.0) {
	    resy[i - 1] = - MAXNUM;
	    eflag = 1;
	} else {
	    resy[i - 1] = (y[i + 1] - y[i - 1]) / (h1 + h2);
	}
    }
    if (eflag) {
	errmsg("Warning: infinite slope, check set status before proceeding");
    }
}

static void seasonaldiff(double *x, double *y,
			 double *resx, double *resy, int n, int period)
{
    int i;

    for (i = 0; i < n - period; i++) {
	resx[i] = x[i];
	resy[i] = y[i] - y[i + period];
    }
}

/*
 * trapezoidal rule
 */
double trapint(double *x, double *y, double *resx, double *resy, int n)
{
    int i;
    double sum = 0.0;
    double h;

    if (n < 2) {
        return 0.0;
    }
    
    if (resx != NULL) {
        resx[0] = x[0];
    }
    if (resy != NULL) {
        resy[0] = 0.0;
    }
    for (i = 1; i < n; i++) {
	h = (x[i] - x[i - 1]);
	if (resx != NULL) {
	    resx[i] = x[i];
	}
	sum = sum + h * (y[i - 1] + y[i]) * 0.5;
	if (resy != NULL) {
	    resy[i] = sum;
	}
    }
    return sum;
}

/*
 * apply a digital filter
 */
void do_digfilter(int set1, int set2)
{
    int digfiltset;

    if (!(is_set_active(get_cg(), set1) && is_set_active(get_cg(), set2))) {
	errmsg("Set not active");
	return;
    }
    if ((getsetlength(get_cg(), set1) < 3) || (getsetlength(get_cg(), set2) < 3)) {
	errmsg("Set length < 3");
	return;
    }
    digfiltset = nextset(get_cg());
    if (digfiltset != (-1)) {
	activateset(get_cg(), digfiltset);
	setlength(get_cg(), digfiltset, getsetlength(get_cg(), set1) - getsetlength(get_cg(), set2) + 1);
	sprintf(buf, "Digital filter from set %d applied to set %d", set2, set1);
	filterser(getsetlength(get_cg(), set1),
		  getx(get_cg(), set1),
		  gety(get_cg(), set1),
		  getx(get_cg(), digfiltset),
		  gety(get_cg(), digfiltset),
		  gety(get_cg(), set2),
		  getsetlength(get_cg(), set2));
	setcomment(get_cg(), digfiltset, buf);
	log_results(buf);
#ifndef NONE_GUI
	update_set_status(get_cg(), digfiltset);
#endif
    }
}

/*
 * linear convolution
 */
void do_linearc(int set1, int set2)
{
    int linearcset, i, itmp;
    double *xtmp;

    if (!(is_set_active(get_cg(), set1) && is_set_active(get_cg(), set2))) {
	errmsg("Set not active");
	return;
    }
    if ((getsetlength(get_cg(), set1) < 3) || (getsetlength(get_cg(), set2) < 3)) {
	errmsg("Set length < 3");
	return;
    }
    linearcset = nextset(get_cg());
    if (linearcset != (-1)) {
	activateset(get_cg(), linearcset);
	setlength(get_cg(), linearcset, (itmp = getsetlength(get_cg(), set1) + getsetlength(get_cg(), set2) - 1));
	linearconv(gety(get_cg(), set2), gety(get_cg(), set1), gety(get_cg(), linearcset), getsetlength(get_cg(), set2), getsetlength(get_cg(), set1));
	xtmp = getx(get_cg(), linearcset);
	for (i = 0; i < itmp; i++) {
	    xtmp[i] = i;
	}
	sprintf(buf, "Linear convolution of set %d with set %d", set1, set2);
	setcomment(get_cg(), linearcset, buf);
	log_results(buf);
#ifndef NONE_GUI
	update_set_status(get_cg(), linearcset);
#endif
    }
}

/*
 * cross correlation
 */
void do_xcor(int set1, int set2, int maxlag)
{
    int xcorset, i, ierr;
    double *xtmp;

    if (!(is_set_active(get_cg(), set1) && is_set_active(get_cg(), set2))) {
	errmsg("Set not active");
	return;
    }
    if (maxlag < 0 || maxlag + 2 > getsetlength(get_cg(), set1)) {
	errmsg("Lag incorrectly specified");
	return;
    }
    if ((getsetlength(get_cg(), set1) < 3) || (getsetlength(get_cg(), set2) < 3)) {
	errmsg("Set length < 3");
	return;
    }
    xcorset = nextset(get_cg());
    if (xcorset != (-1)) {
	activateset(get_cg(), xcorset);
	setlength(get_cg(), xcorset, maxlag + 1);
	if (set1 != set2) {
	    sprintf(buf, "X-correlation of set %d and %d at maximum lag %d",
                    set1, set2, maxlag);
	} else {
	    sprintf(buf, "Autocorrelation of set %d at maximum lag %d",
                    set1, maxlag);
	}
	ierr = crosscorr(gety(get_cg(), set1), gety(get_cg(), set2), getsetlength(get_cg(), set1),
                         maxlag, getx(get_cg(), xcorset), gety(get_cg(), xcorset));
	xtmp = getx(get_cg(), xcorset);
	for (i = 0; i <= maxlag; i++) {
	    xtmp[i] = i;
	}
	setcomment(get_cg(), xcorset, buf);
	log_results(buf);
#ifndef NONE_GUI
	update_set_status(get_cg(), xcorset);
#endif
    }
}

/*
 * splines
 */
void do_spline(int set, double start, double stop, int n, int type)
{
    int i, splineset, len;
    double delx, *x, *y, *b, *c, *d, *xtmp, *ytmp;

    if (!is_set_active(get_cg(), set)) {
	errmsg("Set not active");
	return;
    }
    if ((len = getsetlength(get_cg(), set)) < 3) {
	errmsg("Improper set length");
	return;
    }
    if (n <= 1) {
	errmsg("Number of steps must be > 1");
	return;
    }
    delx = (stop - start) / (n - 1);
    splineset = nextset(get_cg());
    if (splineset != -1) {
	activateset(get_cg(), splineset);
	setlength(get_cg(), splineset, n);
	x = getx(get_cg(), set);
	y = gety(get_cg(), set);
	b = calloc(len, SIZEOF_DOUBLE);
	c = calloc(len, SIZEOF_DOUBLE);
	d = calloc(len, SIZEOF_DOUBLE);
	if (b == NULL || c == NULL || d == NULL) {
	    errmsg("Not enough memory for splines");
	    cxfree(b);
	    cxfree(c);
	    cxfree(d);
	    killset(get_cg(), splineset);
	    return;
	}
	if (type == SPLINE_AKIMA) {
	    aspline(len, x, y, b, c, d);
	} else {
	    spline (len, x, y, b, c, d);
	}
	xtmp = getx(get_cg(), splineset);
	ytmp = gety(get_cg(), splineset);

	for (i = 0; i < n; i++) {
	    xtmp[i] = start + i * delx;
	    if (type == SPLINE_AKIMA) {
	    	ytmp[i] = seval(len, xtmp[i], x, y, b, c, d);
	        sprintf(buf, "Akima spline fit from set %d", set);
	    } else {
	    	ytmp[i] = seval (len, xtmp[i], x, y, b, c, d);
	        sprintf(buf, "Cubic spline fit from set %d", set);
	    }
	}
	setcomment(get_cg(), splineset, buf);
	log_results(buf);

	cxfree(b);
	cxfree(c);
	cxfree(d);

#ifndef NONE_GUI
	update_set_status(get_cg(), splineset);
#endif
    }
}


/*
 * numerical integration
 */
double do_int(int setno, int itype)
{
    int intset;
    double sum = 0;

    if (!is_set_active(get_cg(), setno)) {
	errmsg("Set not active");
	return 0.0;
    }
    if (getsetlength(get_cg(), setno) < 3) {
	errmsg("Set length < 3");
	return 0.0;
    }
    if (itype == 0) {
	intset = nextset(get_cg());
	if (intset != (-1)) {
	    activateset(get_cg(), intset);
	    setlength(get_cg(), intset, getsetlength(get_cg(), setno));
	    sprintf(buf, "Cumulative sum of set %d", setno);
	    sum = trapint(getx(get_cg(), setno), gety(get_cg(), setno), getx(get_cg(), intset), gety(get_cg(), intset), getsetlength(get_cg(), setno));
	    setcomment(get_cg(), intset, buf);
	    log_results(buf);
#ifndef NONE_GUI
	    update_set_status(get_cg(), intset);
#endif
	}
    } else {
	sum = trapint(getx(get_cg(), setno), gety(get_cg(), setno), NULL, NULL, getsetlength(get_cg(), setno));
    }
    return sum;
}

/*
 * difference a set
 * itype means
 *  0 - forward
 *  1 - backward
 *  2 - centered difference
 */
void do_differ(int setno, int itype)
{
    int diffset;

    if (!is_set_active(get_cg(), setno)) {
	errmsg("Set not active");
	return;
    }
    if (getsetlength(get_cg(), setno) < 3) {
	errmsg("Set length < 3");
	return;
    }
    diffset = nextset(get_cg());
    if (diffset != (-1)) {
	activateset(get_cg(), diffset);
	switch (itype) {
	case 0:
	    sprintf(buf, "Forward difference of set %d", setno);
	    setlength(get_cg(), diffset, getsetlength(get_cg(), setno) - 1);
	    forwarddiff(getx(get_cg(), setno), gety(get_cg(), setno), getx(get_cg(), diffset), gety(get_cg(), diffset), getsetlength(get_cg(), setno));
	    break;
	case 1:
	    sprintf(buf, "Backward difference of set %d", setno);
	    setlength(get_cg(), diffset, getsetlength(get_cg(), setno) - 1);
	    backwarddiff(getx(get_cg(), setno), gety(get_cg(), setno), getx(get_cg(), diffset), gety(get_cg(), diffset), getsetlength(get_cg(), setno));
	    break;
	case 2:
	    sprintf(buf, "Centered difference of set %d", setno);
	    setlength(get_cg(), diffset, getsetlength(get_cg(), setno) - 2);
	    centereddiff(getx(get_cg(), setno), gety(get_cg(), setno), getx(get_cg(), diffset), gety(get_cg(), diffset), getsetlength(get_cg(), setno));
	    break;
	}
	setcomment(get_cg(), diffset, buf);
	log_results(buf);
#ifndef NONE_GUI
	update_set_status(get_cg(), diffset);
#endif
    }
}

/*
 * seasonally difference a set
 */
void do_seasonal_diff(int setno, int period)
{
    int diffset;

    if (!is_set_active(get_cg(), setno)) {
	errmsg("Set not active");
	return;
    }
    if (getsetlength(get_cg(), setno) < 2) {
	errmsg("Set length < 2");
	return;
    }
    diffset = nextset(get_cg());
    if (diffset != (-1)) {
	activateset(get_cg(), diffset);
	setlength(get_cg(), diffset, getsetlength(get_cg(), setno) - period);
	seasonaldiff(getx(get_cg(), setno), gety(get_cg(), setno),
		     getx(get_cg(), diffset), gety(get_cg(), diffset),
		     getsetlength(get_cg(), setno), period);
	sprintf(buf, "Seasonal difference of set %d, period %d", setno, period);
	setcomment(get_cg(), diffset, buf);
	log_results(buf);
#ifndef NONE_GUI
	update_set_status(get_cg(), diffset);
#endif
    }
}

/*
 * regression with restrictions to region rno if rno >= 0
 */
void do_regress(int setno, int ideg, int iresid, int rno, int invr, int fitset)
	/* 
	 * setno  - set to perform fit on
	 * ideg   - degree of fit
	 * irisid - 0 -> whole set, 1-> subset of setno
	 * rno    - region number of subset
	 * invr   - 1->invert region, 0 -> do nothing
	 * fitset - set to which fitted function will be loaded
	 * 			Y values are computed at the x values in the set
	 *          if -1 is specified, a set with the same x values as the
	 *          one being fitted will be created and used
	 */
{
    int len, i, sdeg = ideg;
    int cnt = 0, fitlen = 0;
    double *x, *y, *xt = NULL, *yt = NULL, *xr = NULL, *yr = NULL;
    char buf[256];
    double c[20];   /* coefficients of fitted polynomial */

    if (!is_set_active(get_cg(), setno)) {
		errmsg("Set not active");
		return;
    }
    len = getsetlength(get_cg(), setno);
    x = getx(get_cg(), setno);
    y = gety(get_cg(), setno);
    if (rno == -1) {
		xt = x;
		yt = y;
    } else if (isactive_region(rno)) {
		if (!get_points_inregion(rno, invr, len, x, y, &cnt, &xt, &yt)) {
			if (cnt == 0) {
			errmsg("No points found in region, operation cancelled");
			} else {
			errmsg("Memory allocation failed for points in region");
			}
			return;
		}
		len = cnt;
    } else {
		errmsg("Selected region is not active");
		return;
    }
    /*
     * first part for polynomials, second part for linear fits to transformed
     * data
     */
    if ((len < ideg && ideg <= 10) || (len < 2 && ideg > 10)) {
		errmsg("Too few points in set, operation cancelled");
		return;
    }
	/* determine is set provided or use abscissa from fitted set */
    if( fitset == -1 ) {		        /* create set */
    	if( (fitset = nextset(get_cg())) != -1 ) {
			activateset(get_cg(), fitset);
			setlength(get_cg(), fitset, len);
			fitlen = len;
			xr = getx(get_cg(), fitset);
			for (i = 0; i < len; i++) {
	    		xr[i] = xt[i];
			}	
			yr = gety(get_cg(), fitset);
		}
    } else {									/* set has been provided */
		fitlen = getsetlength( get_cg(), fitset );
		xr = getx(get_cg(), fitset);
		yr = gety(get_cg(), fitset);
    }

	/* transform data so that system has the form y' = A + B * x' */
    if (fitset != -1) {
	if (ideg == 12) {	/* y=A*x^B -> ln(y) = ln(A) + B * ln(x) */
	    ideg = 1;
	    for (i = 0; i < len; i++) {
			if (xt[i] <= 0.0) {
				errmsg("One of X[i] <= 0.0");
				return;
			}
			if (yt[i] <= 0.0) {
				errmsg("One of Y[i] <= 0.0");
				return;
			}
	    }
	    for (i = 0; i < len; i++) {
			xt[i] = log(xt[i]);
			yt[i] = log(yt[i]);
	    }
		for( i=0; i<fitlen; i++ )
			if( xr[i] <= 0.0 ) {
				errmsg("One of X[i] <= 0.0");
				return;
			} 
		for( i=0; i<fitlen; i++ )
			xr[i] = log( xr[i] );
	} else if (ideg == 13) {   /*y=Aexp(B*x) -> ln(y) = ln(A) + B * x */
	    ideg = 1;
	    for (i = 0; i < len; i++) {
			if (yt[i] <= 0.0) {
				errmsg("One of Y[i] <= 0.0");
				return;
		 	}
	    }
	    for (i = 0; i < len; i++) {
			yt[i] = log(yt[i]);
	    }
	} else if (ideg == 14) {	/* y = A + B * ln(x) */
	    ideg = 1;
	    for (i = 0; i < len; i++) {
			if (xt[i] <= 0.0) {
				errmsg("One of X[i] <= 0.0");
				return;
			}
	    }
		for (i = 0; i < len; i++) {
			xt[i] = log(xt[i]);	
		}
		for( i=0; i<fitlen; i++ )
			if( xr[i] <= 0.0 ) {
				errmsg("One of X[i] <= 0.0");
				return;
			} 
		for( i=0; i<fitlen; i++ ){
			xr[i] = log( xr[i] );
		}	  
	} else if (ideg == 15) {	/* y = 1/( A + B*x ) -> 1/y = a + B*x */
	    ideg = 1;
	    for (i = 0; i < len; i++) {
			if (yt[i] == 0.0) {
			    errmsg("One of Y[i] = 0.0");
			    return;
			}
	    }
	    for (i = 0; i < len; i++) {
			yt[i] = 1.0 / yt[i];
	    }
	}

	if (fitcurve(xt, yt, len, ideg, c)) {
	    killset(get_cg(), fitset);
	    goto bustout;
	}

	/* compute function at requested x ordinates */
	for( i=0; i<fitlen; i++ )
		yr[i] = leasev( c, ideg, xr[i] );

	sprintf(buf, "\nN.B. Statistics refer to the transformed model\n");
	/* apply inverse transform, output fitted function in human readable form */
	if( sdeg<11 ) {
		sprintf(buf, "\ny = %.5g %c %.5g x", c[0], c[1]>=0?'+':'-', fabs(c[1]));
		for( i=2; i<=ideg; i++ )
			sprintf( buf+strlen(buf), " %c %.5g x^%d", c[i]>=0?'+':'-', 
															fabs(c[i]), i );
		strcat( buf, "\n" );
	} else if (sdeg == 12) {	/* ln(y) = ln(A) + b * ln(x) */
		sprintf( buf, "\ny = %.5g x^%.5g\n", exp(c[0]), c[1] );
	    for (i = 0; i < len; i++) {
			xt[i] = exp(xt[i]);
			yt[i] = exp(yt[i]);
	    }
	    for (i = 0; i < fitlen; i++){
			yr[i] = exp(yr[i]);
			xr[i] = exp(xr[i]);
		}
	} else if (sdeg == 13) { 	/* ln(y) = ln(A) + B * x */
		sprintf( buf, "\ny = %.5g exp( %.5g x )\n", exp(c[0]), c[1] );
	    for (i = 0; i < len; i++) {
			yt[i] = exp(yt[i]);
	    }
	    for (i = 0; i < fitlen; i++)
			yr[i] = exp(yr[i]);
	} else if (sdeg == 14) {	/* y = A + B * ln(x) */
		sprintf(buf, "\ny = %.5g %c %.5g ln(x)\n", c[0], c[1]>=0?'+':'-',
											fabs(c[1]) );
	    for (i = 0; i < len; i++) {
			xt[i] = exp(xt[i]);
	    }
	    for (i = 0; i < fitlen; i++)
			xr[i] = exp(xr[i]);
	} else if (sdeg == 15) {	/* y = 1/( A + B*x ) */
		sprintf( buf, "\ny = 1/(%.5g %c %.5g x)\n", c[0], c[1]>=0?'+':'-',
													fabs(c[1]) );
	    for (i = 0; i < len; i++) {
			yt[i] = 1.0 / yt[i];
	    }
	    for (i = 0; i < fitlen; i++)
			yr[i] = 1.0 / yr[i];
	}
	stufftext(buf, STUFF_STOP);
	sprintf(buf, "\nRegression of set %d results to set %d\n", setno, fitset);
	stufftext(buf, STUFF_STOP);
	
	switch (iresid) {
	case 1:
		/* determine residual */
	    for (i = 0; i < len; i++) {
			yr[i] = yt[i] - yr[i];
	    }
	    break;
	case 2:
	    break;
	}
	sprintf(buf, "%d deg fit of set %d", ideg, setno);
	setcomment(get_cg(), fitset, buf);
	log_results(buf);
#ifndef NONE_GUI
	update_set_status(get_cg(), fitset);
#endif
    }
    bustout:;
    if (rno >= 0 && cnt != 0) {	/* had a region and allocated memory there */
	free(xt);
	free(yt);
    }
}

/*
 * running averages, medians, min, max, std. deviation
 */
void do_runavg(int setno, int runlen, int runtype, int rno, int invr)
{
    int runset;
    int len, cnt = 0;
    double *x, *y, *xt = NULL, *yt = NULL, *xr, *yr;

    if (!is_set_active(get_cg(), setno)) {
	errmsg("Set not active");
	return;
    }
    if (runlen < 2) {
	errmsg("Length of running average < 2");
	return;
    }
    len = getsetlength(get_cg(), setno);
    x = getx(get_cg(), setno);
    y = gety(get_cg(), setno);
    if (rno == -1) {
	xt = x;
	yt = y;
    } else if (isactive_region(rno)) {
	if (!get_points_inregion(rno, invr, len, x, y, &cnt, &xt, &yt)) {
	    if (cnt == 0) {
		errmsg("No points found in region, operation cancelled");
	    } else {
		errmsg("Memory allocation failed for points in region");
	    }
	    return;
	}
	len = cnt;
    } else {
	errmsg("Selected region is not active");
	return;
    }
    if (runlen >= len) {
	errmsg("Length of running average > set length");
	goto bustout;
    }
    runset = nextset(get_cg());
    if (runset != (-1)) {
	activateset(get_cg(), runset);
	setlength(get_cg(), runset, len - runlen + 1);
	xr = getx(get_cg(), runset);
	yr = gety(get_cg(), runset);
	switch (runtype) {
	case 0:
	    runavg(xt, yt, xr, yr, len, runlen);
	    sprintf(buf, "%d-pt. avg. on set %d ", runlen, setno);
	    break;
	case 1:
	    runmedian(xt, yt, xr, yr, len, runlen);
	    sprintf(buf, "%d-pt. median on set %d ", runlen, setno);
	    break;
	case 2:
	    runminmax(xt, yt, xr, yr, len, runlen, 0);
	    sprintf(buf, "%d-pt. min on set %d ", runlen, setno);
	    break;
	case 3:
	    runminmax(xt, yt, xr, yr, len, runlen, 1);
	    sprintf(buf, "%d-pt. max on set %d ", runlen, setno);
	    break;
	case 4:
	    runstddev(xt, yt, xr, yr, len, runlen);
	    sprintf(buf, "%d-pt. std dev., set %d ", runlen, setno);
	    break;
	}
	setcomment(get_cg(), runset, buf);
	log_results(buf);
#ifndef NONE_GUI
	update_set_status(get_cg(), runset);
#endif
    }
  bustout:;
    if (rno >= 0 && cnt != 0) {	/* had a region and allocated memory there */
	free(xt);
	free(yt);
    }
}

/*
 * DFT by FFT or definition
 */
void do_fourier(int fftflag, int setno, int load, int loadx, int invflag, int type, int wind)
{
    int i, ilen;
    double *x, *y, *xx, *yy, delt, T;
    int i2 = 0, specset;

    if (!is_set_active(get_cg(), setno)) {
	errmsg("Set not active");
	return;
    }
    ilen = getsetlength(get_cg(), setno);
    if (ilen < 2) {
	errmsg("Set length < 2");
	return;
    }
    if (fftflag) {
	if ((i2 = ilog2(ilen)) <= 0) {
	    errmsg("Set length not a power of 2");
	    return;
	}
    }
    specset = nextset(get_cg());
    if (specset != -1) {
	activateset(get_cg(), specset);
	setlength(get_cg(), specset, ilen);
	xx = getx(get_cg(), specset);
	yy = gety(get_cg(), specset);
	x = getx(get_cg(), setno);
	y = gety(get_cg(), setno);
	copyx(get_cg(), setno, specset);
	copyy(get_cg(), setno, specset);
	if (wind != 0) {	/* apply data window if needed */
	    apply_window(xx, yy, ilen, type, wind);
	}
	if (type == 0) {	/* real data */
	    for (i = 0; i < ilen; i++) {
		xx[i] = yy[i];
		yy[i] = 0.0;
	    }
	}
	if (fftflag) {
	    fft(xx, yy, ilen, i2, invflag);
	} else {
	    dft(xx, yy, ilen, invflag);
	}
	switch (load) {
	case 0:
	    delt = (x[ilen-1] - x[0])/(ilen -1.0);
	    T = (x[ilen - 1] - x[0]);
	    xx = getx(get_cg(), specset);
	    yy = gety(get_cg(), specset);
	    for (i = 0; i < ilen / 2; i++) {
	      /* carefully get amplitude of complex xform: 
		 use abs(a[i]) + abs(a[-i]) except for zero term */
	      if(i) yy[i] = hypot(xx[i], yy[i])+hypot(xx[ilen-i], yy[ilen-i]);
	      else yy[i]=fabs(xx[i]);
		switch (loadx) {
		case 0:
		    xx[i] = i;
		    break;
		case 1:
		    /* xx[i] = 2.0 * M_PI * i / ilen; */
		    xx[i] = i / T;
		    break;
		case 2:
		    if (i == 0) {
			xx[i] = T + delt;	/* the mean */
		    } else {
			/* xx[i] = (double) ilen / (double) i; */
			xx[i] = T / i;
		    }
		    break;
		}
	    }
	    setlength(get_cg(), specset, ilen / 2);
	    break;
	case 1:
	    delt = (x[ilen-1] - x[0])/(ilen -1.0);
	    T = (x[ilen - 1] - x[0]);
	    setlength(get_cg(), specset, ilen / 2);
	    xx = getx(get_cg(), specset);
	    yy = gety(get_cg(), specset);
	    for (i = 0; i < ilen / 2; i++) {
		yy[i] = -atan2(yy[i], xx[i]);
		switch (loadx) {
		case 0:
		    xx[i] = i;
		    break;
		case 1:
		    /* xx[i] = 2.0 * M_PI * i / ilen; */
		    xx[i] = i / T;
		    break;
		case 2:
		    if (i == 0) {
			xx[i] = T + delt;
		    } else {
			/* xx[i] = (double) ilen / (double) i; */
			xx[i] = T / i;
		    }
		    break;
		}
	    }
	    break;
	}
	if (fftflag) {
	    sprintf(buf, "FFT of set %d", setno);
	} else {
	    sprintf(buf, "DFT of set %d", setno);
	}
	setcomment(get_cg(), specset, buf);
	log_results(buf);
#ifndef NONE_GUI
	update_set_status(get_cg(), specset);
#endif
    }
}

/*
 * Apply a window to a set, result goes to a new set.
 */
void do_window(int setno, int type, int wind)
{
    int ilen;
    double *xx, *yy;
    int specset;

    if (!is_set_active(get_cg(), setno)) {
	errmsg("Set not active");
	return;
    }
    ilen = getsetlength(get_cg(), setno);
    if (ilen < 2) {
	errmsg("Set length < 2");
	return;
    }
    specset = nextset(get_cg());
    if (specset != -1) {
	char *wtype[6];
	wtype[0] = "Triangular";
	wtype[1] = "Hanning";
	wtype[2] = "Welch";
	wtype[3] = "Hamming";
	wtype[4] = "Blackman";
	wtype[5] = "Parzen";

	activateset(get_cg(), specset);
	setlength(get_cg(), specset, ilen);
	xx = getx(get_cg(), specset);
	yy = gety(get_cg(), specset);
	copyx(get_cg(), setno, specset);
	copyy(get_cg(), setno, specset);
	if (wind != 0) {
	    apply_window(xx, yy, ilen, type, wind);
	    sprintf(buf, "%s windowed set %d", wtype[wind - 1], setno);
	} else {		/* shouldn't happen */
	}
	setcomment(get_cg(), specset, buf);
	log_results(buf);
#ifndef NONE_GUI
	update_set_status(get_cg(), specset);
#endif
    }
}

void apply_window(double *xx, double *yy, int ilen, int type, int wind)
{
    int i;

    for (i = 0; i < ilen; i++) {
	switch (wind) {
	case 1:		/* triangular */
	    if (type != 0) {
		xx[i] *= 1.0 - fabs((i - 0.5 * (ilen - 1.0)) / (0.5 * (ilen - 1.0)));
	    }
	    yy[i] *= 1.0 - fabs((i - 0.5 * (ilen - 1.0)) / (0.5 * (ilen - 1.0)));

	    break;
	case 2:		/* Hanning */
	    if (type != 0) {
		xx[i] = xx[i] * (0.5 - 0.5 * cos(2.0 * M_PI * i / (ilen - 1.0)));
	    }
	    yy[i] = yy[i] * (0.5 - 0.5 * cos(2.0 * M_PI * i / (ilen - 1.0)));
	    break;
	case 3:		/* Welch (from Numerical Recipes) */
	    if (type != 0) {
		xx[i] *= 1.0 - pow((i - 0.5 * (ilen - 1.0)) / (0.5 * (ilen + 1.0)), 2.0);
	    }
	    yy[i] *= 1.0 - pow((i - 0.5 * (ilen - 1.0)) / (0.5 * (ilen + 1.0)), 2.0);
	    break;
	case 4:		/* Hamming */
	    if (type != 0) {
		xx[i] = xx[i] * (0.54 - 0.46 * cos(2.0 * M_PI * i / (ilen - 1.0)));
	    }
	    yy[i] = yy[i] * (0.54 - 0.46 * cos(2.0 * M_PI * i / (ilen - 1.0)));
	    break;
	case 5:		/* Blackman */
	    if (type != 0) {
		xx[i] = xx[i] * (0.42 - 0.5 * cos(2.0 * M_PI * i / (ilen - 1.0)) + 0.08 * cos(4.0 * M_PI * i / (ilen - 1.0)));
	    }
	    yy[i] = yy[i] * (0.42 - 0.5 * cos(2.0 * M_PI * i / (ilen - 1.0)) + 0.08 * cos(4.0 * M_PI * i / (ilen - 1.0)));
	    break;
	case 6:		/* Parzen (from Numerical Recipes) */
	    if (type != 0) {
		xx[i] *= 1.0 - fabs((i - 0.5 * (ilen - 1)) / (0.5 * (ilen + 1)));
	    }
	    yy[i] *= 1.0 - fabs((i - 0.5 * (ilen - 1)) / (0.5 * (ilen + 1)));
	    break;
	}
    }
}


/*
 * histograms
 */
void do_histo(int fromgraph, int fromset, int tograph, int toset,
	      double binw, double xmin, double xmax, int hist_type)
{
    int i, ndata, nbins;
    int bsign;
    int *hist;
    double *x, *y, *data, *bins;
    plotarr p;
    
    if (!is_set_active(get_cg(), fromset)) {
	errmsg("Set not active");
	return;
    }
    if (getsetlength(get_cg(), fromset) <= 0) {
	errmsg("Set length = 0");
	return;
    }
    if (binw <= 0.0) {
	errmsg("Bin width <= 0");
	return;
    }
    if (tograph == -1) {
	tograph = get_cg();
    }
    if (is_graph_active(tograph) == FALSE) {
	set_graph_active(tograph, TRUE);
    }
    if (toset == SET_SELECT_NEXT) {
	toset = nextset(tograph);
	if (toset == -1) {
	    return;
	}
    }
    
    ndata = getsetlength(fromgraph, fromset);
    data = gety(fromgraph, fromset);
    
    nbins = (int) ceil (fabs((xmax - xmin)/binw));
    
    hist = (int *) malloc (nbins*sizeof(int));
    if (hist == NULL) {
        errmsg("malloc failed in do_histo()");
        return;
    }

    bins = (double *) malloc ((nbins + 1)*sizeof(double));
    if (bins == NULL) {
        errmsg("malloc failed in do_histo()");
        free(hist);
        return;
    }
    
    bsign = sign(xmax - xmin);
    for (i = 0; i < nbins + 1; i++) {
        bins[i] = xmin + bsign*i*binw;
    }
    if (bsign*bins[nbins] > bsign*xmax) {
        bins[nbins] = xmax;
    }
    
    if (histogram(ndata, data, nbins, bins, hist) == GRACE_EXIT_FAILURE){
        free(hist);
        free(bins);
        return;
    }
    
    activateset(tograph, toset);
    setlength(tograph, toset, nbins + 1);
    x = getx(tograph, toset);
    y = gety(tograph, toset);
    
    x[0] = bins[0];
    y[0] = 0.0;
    for (i = 1; i < nbins + 1; i++) {
        x[i] = bins[i];
        y[i] = (hist_type == HISTOGRAM_TYPE_CUMULATIVE)*y[i - 1] + hist[i - 1];
    }
    
    free(hist);
    free(bins);

    get_graph_plotarr(tograph, toset, &p);
    p.sym = SYM_NONE;
    p.linet = LINE_TYPE_LEFTSTAIR;
    p.dropline = TRUE;
    p.baseline = FALSE;
    p.baseline_type = BASELINE_TYPE_0;
    p.lines = 1;
    p.symlines = 1;
    sprintf(p.comments, "Histogram from G%d.S%d", fromgraph, fromset);
    set_graph_plotarr(tograph, toset, &p);

    log_results(buf);
}

int histogram(int ndata, double *data, int nbins, double *bins, int *hist)
{
    int i, j, bsign;
    double minval, maxval;
    
    if (nbins < 1) {
        errmsg("Number of bins < 1");
        return GRACE_EXIT_FAILURE;
    }
    
    bsign = sign(bins[1] - bins[0]);
    
    if (nbins > 1) {
        if (bsign == 0) {
            errmsg("Zero-width bin");
            return GRACE_EXIT_FAILURE;
        }
        for (i = 1; i < nbins; i++) {
            if (bsign != sign(bins[i + 1] - bins[i])) {
                errmsg("Non-monothonic bins");
                return GRACE_EXIT_FAILURE;
            }
        }
    }
    
    for (i = 0; i < nbins; i++) {
        hist[i] = 0;
    }
    
    /* TODO: binary search */
    for (i = 0; i < ndata; i++) {
        for (j = 0; j < nbins; j++) {
            if (bsign > 0) {
                minval = bins[j];
                maxval = bins[j + 1];
            } else {
                minval = bins[j + 1];
                maxval = bins[j];
            }
            if (data[i] >= minval && data[i] <= maxval) {
                hist[j] += 1;
                break;
            }
        }
    }
    return GRACE_EXIT_SUCCESS;
}


/*
 * sample a set, by start/step or logical expression
 */
void do_sample(int setno, int typeno, char *exprstr, int startno, int stepno)
{
    int len, npts = 0, i, resset, ier;
    double *x, *y, tmpx, tmpy;
    double a, b, c, d;

    if (!is_set_active(get_cg(), setno)) {
	errmsg("Set not active");
	return;
    }
    len = getsetlength(get_cg(), setno);
    resset = nextset(get_cg());
    if (resset < 0) {
	return;
    }
    if (typeno == 0) {
	if (len <= 2) {
	    errmsg("Set has <= 2 points");
	    return;
	}
	if (startno < 1) {
	    errmsg("Start point < 1 (locations in sets are numbered starting from 1)");
	    return;
	}
	if (stepno < 1) {
	    errmsg("Step < 1");
	    return;
	}
	x = getx(get_cg(), setno);
	y = gety(get_cg(), setno);
	for (i = startno - 1; i < len; i += stepno) {
	    add_point(get_cg(), resset, x[i], y[i], 0.0, 0.0, SET_XY);
	    npts++;
	}
	sprintf(buf, "Sample, %d, %d set #%d", startno, stepno, setno);
    } else {
	if (!strlen(exprstr)) {
	    errmsg("Enter logical expression first");
	    return;
	}
	x = getx(get_cg(), setno);
	y = gety(get_cg(), setno);
	npts = 0;
	sprintf(buf, "Sample from %d, using '%s'", setno, exprstr);
	tmpx = x[0];
	tmpy = y[0];
	for (i = 0; i < len; i++) {
	    x[0] = x[i];
	    y[0] = y[i];
	    scanner(exprstr, &x[i], &y[i], 1, &a, &b, &c, &d, 1, i, setno, &ier);
	    if (ier) {
		killset(get_cg(), resset);
		x[0] = tmpx;
		y[0] = tmpy;
		return;
	    }
	    if ((int) result) {
		add_point(get_cg(), resset, x[i], y[i], 0.0, 0.0, SET_XY);
		npts++;
	    }
	}
	x[0] = tmpx;
	y[0] = tmpy;
    }
    if (npts > 0) {
	setcomment(get_cg(), resset, buf);
	log_results(buf);
#ifndef NONE_GUI
	update_set_status(get_cg(), resset);
#endif
    }
}

#define prune_xconv(res,x,xtype)	\
    switch (deltatypeno) {		\
    case PRUNE_VIEWPORT:		\
	res = xy_xconv(x);			\
	break;				\
    case PRUNE_WORLD:			\
	switch (xtype) {		\
	case PRUNE_LIN:			\
	    res = x;			\
	    break;			\
	case PRUNE_LOG:			\
	    res = log(x);		\
	    break;			\
	}				\
    }

#define prune_yconv(res,y,ytype)	\
    switch (deltatypeno) {		\
    case PRUNE_VIEWPORT:		\
	res = xy_yconv(y);			\
	break;				\
    case PRUNE_WORLD:			\
	switch (ytype) {		\
	case PRUNE_LIN:			\
	    res = y;			\
	    break;			\
	case PRUNE_LOG:			\
	    res = log(y);		\
	    break;			\
	}				\
    }

/*
 * Prune data
 */
void do_prune(int setno, int typeno, int deltatypeno, float deltax, float deltay, int dxtype, int dytype)
{
    int len, npts = 0, d, i, j, k, drop, resset;
    double *x, *y, *resx, *resy, xtmp, ytmp, ddx = 0.0, ddy = 0.0;
    double xj = 0.0, xjm = 0.0, xjp = 0.0, yj = 0.0, yjm = 0.0, yjp = 0.0;

    if (!is_set_active(get_cg(), setno)) {
        errmsg("Set not active");
        return;
    }
    len = getsetlength(get_cg(), setno);
    if (len <= 2) {
	errmsg("Set has <= 2 points");
	return;
    }
    x = getx(get_cg(), setno);
    y = gety(get_cg(), setno);
    switch (typeno) {
    case PRUNE_CIRCLE:
    case PRUNE_ELLIPSE:
    case PRUNE_RECTANGLE:
	deltax = fabs(deltax);
	if (deltax == 0)
	    return;
	break;
    }
    switch (typeno) {
    case PRUNE_CIRCLE:
	deltay = deltax;
	break;
    case PRUNE_ELLIPSE:
    case PRUNE_RECTANGLE:
    case PRUNE_INTERPOLATION:
	deltay = fabs(deltay);
	if (deltay == 0)
	    return;
	break;
    }
    if (deltatypeno == PRUNE_WORLD) {
	if (dxtype == PRUNE_LOG && deltax < 1.0) {
	    deltax = 1.0 / deltax;
	}
	if (dytype == PRUNE_LOG && deltay < 1.0) {
	    deltay = 1.0 / deltay;
	}
    }
    resset = nextset(get_cg());
    if (resset < 0) {
        return;
    }
    add_point(get_cg(), resset, x[0], y[0], 0.0, 0.0, SET_XY);
    npts++;
    resx = getx(get_cg(), resset);
    resy = gety(get_cg(), resset);
    switch (typeno) {
    case PRUNE_CIRCLE:
    case PRUNE_ELLIPSE:
	for (i = 1; i < len; i++) {
	    xtmp = x[i];
	    ytmp = y[i];
	    drop = FALSE;
	    for (j = npts - 1; j >= 0 && drop == FALSE; j--) {
		switch (deltatypeno) {
		case PRUNE_VIEWPORT:
		    ddx = (xy_xconv(xtmp) - xy_xconv(resx[j])) / deltax;
		    if (fabs(ddx) < 1.0) {
			ddy = (xy_yconv(ytmp) - xy_yconv(resy[j])) / deltay;
			if (ddx * ddx + ddy * ddy < 1.0) {
			    drop = TRUE;
			}
		    }
		    break;
		case PRUNE_WORLD:
		    switch (dxtype) {
		    case PRUNE_LIN:
			ddx = (xtmp - resx[j]) / deltax;
			break;
		    case PRUNE_LOG:
			ddx = (xtmp / resx[j]);
			if (ddx < 1.0) {
			    ddx = 1.0 / ddx;
			}
			ddx /= deltax;
			break;
		    }
		    if (fabs(ddx) < 1.0) {
			switch (dytype) {
			case PRUNE_LIN:
			    ddy = (ytmp - resy[j]) / deltay;
			    break;
			case PRUNE_LOG:
			    ddy = (ytmp / resy[j]);
			    if (ddy < 1.0) {
				ddy = 1.0 / ddy;
			    }
			    ddy /= deltay;
			    break;
			}
			if (ddx * ddx + ddy * ddy < 1.0) {
			    drop = TRUE;
			}
		    }
		    break;
		}
	    }
	    if (drop == FALSE) {
		add_point(get_cg(), resset, xtmp, ytmp, 0.0, 0.0, SET_XY);
		npts++;
		resx = getx(get_cg(), resset);
		resy = gety(get_cg(), resset);
	    }
	}
	sprintf(buf, "Prune from %d, %s dx = %g dy = %g", setno, 
	    (typeno == 0) ? "Circle" : "Ellipse", deltax, deltay);
	break;
    case PRUNE_RECTANGLE:
	for (i = 1; i < len; i++) {
	    xtmp = x[i];
	    ytmp = y[i];
	    drop = FALSE;
	    for (j = npts - 1; j >= 0 && drop == FALSE; j--) {
		switch (deltatypeno) {
		case PRUNE_VIEWPORT:
		    ddx = fabs(xy_xconv(xtmp) - xy_xconv(resx[j]));
		    if (ddx < deltax) {
			ddy = fabs(xy_yconv(ytmp) - xy_yconv(resy[j]));
			if (ddy < deltay) {
			    drop = TRUE;
			}
		    }
		    break;
		case PRUNE_WORLD:
		    switch (dxtype) {
		    case PRUNE_LIN:
			ddx = fabs(xtmp - resx[j]);
			break;
		    case PRUNE_LOG:
			ddx = (xtmp / resx[j]);
			if (ddx < 1.0) {
			    ddx = 1.0 / ddx;
			}
			break;
		    }
		    if (ddx < deltax) {
			switch (dytype) {
			case PRUNE_LIN:
			    ddy = fabs(ytmp - resy[j]);
			    break;
			case PRUNE_LOG:
			    ddy = (ytmp / resy[j]);
			    if (ddy < 1.0) {
				ddy = 1.0 / ddy;
			    }
			    break;
			}
			if (ddy < deltay) {
			    drop = TRUE;
			}
		    }
		    break;
		}
	    }
	    if (drop == FALSE) {
		add_point(get_cg(), resset, xtmp, ytmp, 0.0, 0.0, SET_XY);
		npts++;
		resx = getx(get_cg(), resset);
		resy = gety(get_cg(), resset);
	    }
	}
	sprintf(buf, "Prune from %d, %s dx = %g dy = %g", setno, 
	    "Rectangle", deltax, deltay);
	break;
    case PRUNE_INTERPOLATION:
	k = 0;
	prune_xconv(xjm, x[0], dxtype);
	prune_yconv(yjm, y[0], dytype);
	while (k < len - 2) {
	    d = 1;
	    i = k + d + 1;
	    drop = TRUE;
	    while (TRUE) {
		prune_xconv(xjp, x[i], dxtype);
		prune_yconv(yjp, y[i], dytype);
		for (j = k + 1; j < i && drop == TRUE; j++) {
		    prune_xconv(xj, x[j], dxtype);
		    prune_yconv(yj, y[j], dytype);
		    if (xjp == xjm) {
			ytmp = 0.5 * (yjp + yjm);
		    } else {
			ytmp = (yjp*(xj-xjm)+yjm*(xjp-xj))/(xjp-xjm);
		    }
		    switch (deltatypeno) {
		    case PRUNE_VIEWPORT:
			ddy = fabs(ytmp - yj);
			break;
		    case PRUNE_WORLD:
			switch (dytype) {
			case PRUNE_LIN:
			    ddy = fabs(ytmp - yj);
			    break;
			case PRUNE_LOG:
			    ddy = exp(fabs(ytmp - yj));
			    break;
			}
		    }
		    if (ddy > deltay) {
			drop = FALSE;
		    }
		}
		if (drop == FALSE || i == len - 1) {
		    break;
		}
		d *= 2;
		i = k + d + 1;
		if (i >= len) {
		    i = len - 1;
		}
	    }
	    if (drop == FALSE) {
		i = k + 1;
		drop = TRUE;
		while (d > 1) {
		    d /= 2;
		    i += d;
		    prune_xconv(xjp, x[i], dxtype);
		    prune_yconv(yjp, y[i], dytype);
		    drop = TRUE;
		    for (j = k + 1; j < i && drop == TRUE; j++) {
			prune_xconv(xj, x[j], dxtype);
			prune_yconv(yj, y[j], dytype);
			ytmp = (yjp*(xj-xjm)+yjm*(xjp-xj))/(xjp-xjm);
			switch (deltatypeno) {
			case PRUNE_VIEWPORT:
			    ddy = fabs(ytmp - yj);
			    break;
			case PRUNE_WORLD:
			    switch (dytype) {
			    case PRUNE_LIN:
				ddy = fabs(ytmp - yj);
				break;
			    case PRUNE_LOG:
				ddy = exp(fabs(ytmp - yj));
				break;
			    }
			}
			if (ddy > deltay) {
			    drop = FALSE;
			}
		    }
		    if (drop == FALSE) {
			i -= d;
		    }
		}
	    }
	    k = i;
	    prune_xconv(xjm, x[k], dxtype);
	    prune_yconv(yjm, y[k], dytype);
	    add_point(get_cg(), resset, x[k], y[k], 0.0, 0.0, SET_XY);
	    npts++;
	    resx = getx(get_cg(), resset);
	    resy = gety(get_cg(), resset);
	}
	if (k == len - 2) {
	    add_point(get_cg(), resset, x[len-1], y[len-1], 0.0, 0.0, SET_XY);
	    npts++;
	}
	sprintf(buf, "Prune from %d, %s dy = %g", setno, 
	    "Interpolation", deltay);
	break;
    }
    setcomment(get_cg(), resset, buf);
    log_results(buf);
#ifndef NONE_GUI
    update_set_status(get_cg(), resset);
#endif
}

int get_points_inregion(int rno, int invr, int len, double *x, double *y, int *cnt, double **xt, double **yt)
{
    int i, clen = 0;
    double *xtmp, *ytmp;
    *cnt = 0;
    if (isactive_region(rno)) {
	for (i = 0; i < len; i++) {
	    if (invr) {
		if (!inregion(rno, x[i], y[i])) {
		    clen++;
		}
	    } else {
		if (inregion(rno, x[i], y[i])) {
		    clen++;
		}
	    }
	}
	if (clen == 0) {
	    return 0;
	}
	xtmp = (double *) calloc(clen, sizeof(double));
	if (xtmp == NULL) {
	    return 0;
	}
	ytmp = (double *) calloc(clen, sizeof(double));
	if (ytmp == NULL) {
	    free(xtmp);
	    return 0;
	}
	clen = 0;
	for (i = 0; i < len; i++) {
	    if (invr) {
		if (!inregion(rno, x[i], y[i])) {
		    xtmp[clen] = x[i];
		    ytmp[clen] = y[i];
		    clen++;
		}
	    } else {
		if (inregion(rno, x[i], y[i])) {
		    xtmp[clen] = x[i];
		    ytmp[clen] = y[i];
		    clen++;
		}
	    }
	}
    } else {
	return 0;
    }
    *cnt = clen;
    *xt = xtmp;
    *yt = ytmp;
    return 1;
}

void do_interp( int yset, int xset, int method )
/* interpolate a set at abscissas from another set
 *  yset - set to interpolate
 *  xset - set supplying abscissas
 *  method - spline or linear interpolation
 *
 * Added by Ed Vigmond 10/2/97
 */
{
    int i, j, iset, xsetlength, ysetlength, isetlength;
    double *x1, *x2, *newx, *y, *b, *c, *d, newy;
    double xmin, xmax, ymin, ymax;
	
    if (!is_set_active(get_cg(), yset)) {
	errmsg("Interpolating set not active");
	return;
    }
    if (!is_set_active(get_cg(), xset)) {
        errmsg("Sampling set not active");
        return;
    }
    iset = nextset(get_cg());
    /* make sure set was really killed */
/*
 *     if (getsetlength(get_cg(), iset) > 0) {                
 *         set_default_plotarr(&g[get_cg()].p[iset]);
 *     }
 */
    activateset( get_cg(), iset );
    /* ensure bounds of new set */
    x1=getx( get_cg(), yset );
    y=gety( get_cg(), yset );
    x2=getx( get_cg(), xset );
    newx = calloc( getsetlength( get_cg(), xset ), sizeof( double ) );
    xsetlength = getsetlength( get_cg(), xset );
    ysetlength = getsetlength( get_cg(), yset );
    for(i = 0, j = 0; i < xsetlength; i++) {		/* get intersection of sets */
    	getsetminmax(get_cg(), yset, &xmin, &xmax, &ymin, &ymax);
        if(x2[i] >= xmin && x2[i] <= xmax) {
    	    newx[j++] = x2[i];
        }
    }
    isetlength = j;

    if( method ) {					/* spline */
		b = calloc(ysetlength, sizeof(double));
		c = calloc(ysetlength, sizeof(double));
		d = calloc(ysetlength, sizeof(double));
		if (b == NULL || c == NULL || d == NULL) {
	    	errmsg("Not enough memory for splines");
	    	cxfree(b);
	    	cxfree(c);
	    	cxfree(d);
	    	killset(get_cg(), iset);
	    	return;
		}
		if (method == SPLINE_AKIMA){      /* Akima spline */
		    aspline(ysetlength, x1, y, b, c, d);
		} else {                          /* Plain cubic spline */
		    spline(ysetlength, x1, y, b, c, d);
		}
		for (i = 0; i < j; i++) {
		    add_point(get_cg(), iset, newx[i], seval(ysetlength, newx[i], x1, y,
		               b, c, d), 0.0, 0.0, SET_XY);
		}
		
    }else {				          /* linear interpolation */
    	for( j=0; j<isetlength; j++ ) {
    		i=0;
    		while( (x1[i]>newx[j] || x1[i+1]<=newx[j]) && i<ysetlength )
    			i++;
    		if( i>= ysetlength)
    			newy = y[ysetlength-1];
    		else 
    			newy =
    			(newx[j]-x1[i])*(y[i+1]-y[i])/(x1[i+1]-x1[i])+y[i];
    		add_point(get_cg(), iset, newx[j], newy, 0.0, 0.0, SET_XY);
    	}
    }
    /* activate new set and update sets */
    sprintf( buf, "Interpolated from Set %d at points from Set %d", yset, xset );
    cxfree( newx );
    setcomment(get_cg(), iset, buf);
#ifndef NONE_GUI
    update_set_status(get_cg(), iset);
#endif
}
