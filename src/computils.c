/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 * 
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 * 
 * Copyright (c) 1991-1995 Paul J Turner, Portland, OR
 * Copyright (c) 1996-2000 Grace Development Team
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

#include "cephes/cephes.h"

#include "globals.h"
#include "utils.h"
#include "draw.h"
#include "ssdata.h"
#include "graphs.h"
#include "parser.h"
#include "protos.h"

static char buf[256];

/*
 * evaluate a formula
 */
int do_compute(int gno, int setno, int graphto, int loadto, char *rarray, char *fstr)
{
    if (is_set_active(gno, setno)) {
	if (gno != graphto || setno != loadto) {
	    if (copysetdata(gno, setno, graphto, loadto) != RETURN_SUCCESS) {
	        return RETURN_FAILURE;
            }
        }
	filter_set(graphto, loadto, rarray);
        set_parser_setno(graphto, loadto);
        if (scanner(fstr) != RETURN_SUCCESS) {
	    if (graphto != gno || loadto != setno) {
		killset(graphto, loadto);
	    }
	    return RETURN_FAILURE;
	} else {
	    set_dirtystate();
            return RETURN_SUCCESS;
        }
    } else {
        return RETURN_FAILURE;
    }
}

/*
 * difference a set
 */
int do_differ(int gsrc, int setfrom, int gdest, int setto,
    int derivative, int xplace, int period)
{
    int i, ncols, nc, len, newlen;
    double *x1, *x2;
    char *stype, pbuf[32];
    
    if (!is_set_active(gsrc, setfrom)) {
	errmsg("Set not active");
	return RETURN_FAILURE;
    }
    
    if (period < 1) {
	errmsg("Non-positive period");
	return RETURN_FAILURE;
    }
    
    len = getsetlength(gsrc, setfrom);
    newlen = len - period;
    if (newlen <= 0) {
	errmsg("Source set length <= differentiation period");
	return RETURN_FAILURE;
    }
    
    x1 = getcol(gsrc, setfrom, DATA_X);
    if (derivative) {
        for (i = 0; i < newlen; i++) {
            if (x1[i + period] - x1[i] == 0.0) {
	        sprintf(buf, "Can't evaluate derivative, x1[%d] = x1[%d]",
                    i, i + period);
                errmsg(buf);
                return RETURN_FAILURE;
            }
        }
    }
    
    activateset(gdest, setto);
    if (setlength(gdest, setto, newlen) != RETURN_SUCCESS) {
	return RETURN_FAILURE;
    }
    
    ncols = dataset_cols(gsrc, setfrom);
    if (dataset_cols(gdest, setto) != ncols) {
        set_dataset_type(gdest, setto, dataset_type(gsrc, setfrom));
    }
    
    for (nc = 1; nc < ncols; nc++) {
        double h, *d1, *d2;
        d1 = getcol(gsrc, setfrom, nc);
        d2 = getcol(gdest, setto, nc);
        for (i = 0; i < newlen; i++) {
            d2[i] = d1[i + period] - d1[i];
            if (derivative) {
                h = x1[i + period] - x1[i];
                d2[i] /= h;
            }
        }
    }
    
    x2 = getcol(gdest, setto, DATA_X);
    for (i = 0; i < newlen; i++) {
        switch (xplace) {
        case DIFF_XPLACE_LEFT:
            x2[i] = x1[i];
	    break;
        case DIFF_XPLACE_RIGHT:
            x2[i] = x1[i + period];
	    break;
        case DIFF_XPLACE_CENTER:
            x2[i] = (x1[i + period] + x1[i])/2;
	    break;
        }
    }
    
    /* Prepare set comments */
    switch (xplace) {
    case DIFF_XPLACE_LEFT:
	stype = "Left";
	break;
    case DIFF_XPLACE_RIGHT:
	stype = "Right";
	break;
    case DIFF_XPLACE_CENTER:
	stype = "Centered";
	break;
    default:
	errmsg("Wrong parameters passed to do_differ()");
	return RETURN_FAILURE;
        break;
    }
    
    if (period != 1) {
        sprintf(pbuf, " (period = %d)", period);
    } else {
        pbuf[0] = '\0';
    }
    sprintf(buf, "%s %s%s of set G%d.S%d", stype, pbuf,
        derivative ? "derivative":"difference", gsrc, setfrom);
    
    setcomment(gdest, setto, buf);
    
    return RETURN_SUCCESS;
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
 * linear convolution
 */
int do_linearc(int gsrc, int setfrom, int gdest, int setto,
    int gconv, int setconv)
{
    int srclen, convlen, destlen, i, ncols, nc;
    double xspace1, xspace2, *xsrc, *xconv, *xdest, *yconv;

    if (!is_set_active(gsrc, setfrom) ||
        !is_set_active(gconv, setconv)) {
	errmsg("Set not active");
	return RETURN_FAILURE;
    }
    
    srclen  = getsetlength(gsrc, setfrom);
    convlen = getsetlength(gconv, setconv);
    if (srclen < 2 || convlen < 2) {
	errmsg("Set length < 2");
	return RETURN_FAILURE;
    }
    
    destlen = srclen + convlen - 1;

    xsrc  = getcol(gsrc, setfrom, DATA_X);
    if (monospaced(xsrc, srclen, &xspace1) != TRUE) {
        errmsg("Abscissas of the set are not monospaced");
        return RETURN_FAILURE;
    } else {
        if (xspace1 == 0.0) {
            errmsg("The set spacing is 0");
            return RETURN_FAILURE;
        }
    }

    xconv = getcol(gconv, setconv, DATA_X);
    if (monospaced(xconv, convlen, &xspace2) != TRUE) {
        errmsg("Abscissas of the set are not monospaced");
        return RETURN_FAILURE;
    } else {
        if (fabs(xspace2/xspace1 - 1) > 0.01/(srclen + convlen)) {
            errmsg("The source and convoluting functions are not equally sampled");
            return RETURN_FAILURE;
        }
    }
    
    activateset(gdest, setto);
    if (setlength(gdest, setto, destlen) != RETURN_SUCCESS) {
	return RETURN_FAILURE;
    }

    ncols = dataset_cols(gsrc, setfrom);
    if (dataset_cols(gdest, setto) != ncols) {
        set_dataset_type(gdest, setto, dataset_type(gsrc, setfrom));
    }
    
    yconv = getcol(gconv, setconv, DATA_Y);
    
    for (nc = 1; nc < ncols; nc++) {
        double *d1, *d2;
        
        d1 = getcol(gsrc, setfrom, nc);
        d2 = getcol(gdest, setto, nc);
        
        linearconv(d1, srclen, yconv, convlen, d2);
        for (i = 0; i < destlen; i++) {
            d2[i] *= xspace1;
        }
    }

    xdest = getcol(gdest, setto, DATA_X);
    for (i = 0; i < destlen; i++) {
	xdest[i] = (xsrc[0] + xconv[0]) + i*xspace1;
    }
    
    sprintf(buf, "Linear convolution of set G%d.S%d with set G%d.S%d",
        gsrc, setfrom, gconv, setconv);
    setcomment(gdest, setto, buf);
    
    return RETURN_SUCCESS;
}

/*
 * cross correlation
 */
int do_xcor(int gsrc, int setfrom, int gdest, int setto,
    int gcor, int setcor, int maxlag)
{
    int autocor;
    int len, i, ncols1, ncols2, ncols, nc;
    double xspace1, xspace2, *xsrc, *xcor, *xdest, xoffset;

    if (maxlag < 0) {
	errmsg("Negative max lag");
	return RETURN_FAILURE;
    }

    if (!is_set_active(gsrc, setfrom)) {
	errmsg("Set not active");
	return RETURN_FAILURE;
    }
    
    len = getsetlength(gsrc, setfrom);
    if (len < 2) {
	errmsg("Set length < 2");
	return RETURN_FAILURE;
    }

    xsrc = getcol(gsrc, setfrom, DATA_X);
    if (monospaced(xsrc, len, &xspace1) != TRUE) {
        errmsg("Abscissas of the source set are not monospaced");
        return RETURN_FAILURE;
    } else {
        if (xspace1 == 0.0) {
            errmsg("The source set spacing is 0");
            return RETURN_FAILURE;
        }
    }

    if (gsrc == gcor && setfrom == setcor) {
        autocor = TRUE;
    } else {
        autocor = FALSE;
    }

    if (!autocor) {
        if (!is_set_active(gcor, setcor)) {
	    errmsg("Set not active");
	    return RETURN_FAILURE;
        }
        
        if (getsetlength(gcor, setcor) != len) {
	    errmsg("The correlating sets are of different length");
	    return RETURN_FAILURE;
        }

        xcor = getcol(gcor, setcor, DATA_X);
        if (monospaced(xcor, len, &xspace2) != TRUE) {
            errmsg("Abscissas of the set are not monospaced");
            return RETURN_FAILURE;
        } else {
            if (fabs(xspace2/xspace1 - 1) > 0.01/(2*len)) {
                errmsg("The source and correlation functions are not equally sampled");
                return RETURN_FAILURE;
            }
        }
        
        xoffset = xcor[0] - xsrc[0];
    } else {
        xoffset = 0.0;
    }

    activateset(gdest, setto);
    if (setlength(gdest, setto, maxlag) != RETURN_SUCCESS) {
	return RETURN_FAILURE;
    }

    ncols1 = dataset_cols(gsrc, setfrom);
    ncols2 = dataset_cols(gcor, setcor);
    ncols = MIN2(ncols1, ncols2);
    if (dataset_cols(gdest, setto) != ncols) {
        set_dataset_type(gdest, setto, dataset_type(gsrc, setfrom));
    }

    for (nc = 1; nc < ncols; nc++) {
        int buflen;
        double *d1_re, *d1_im, *d2_re, *d2_im, *dres;
        
        buflen = len + maxlag;
        
        /* reallocate input to pad with zeros */
        d1_re = copy_data_column(getcol(gsrc, setfrom, nc), len);
        d1_re = xrealloc(d1_re, SIZEOF_DOUBLE*buflen);
        d1_im = xcalloc(buflen, SIZEOF_DOUBLE);
        if (!d1_re || !d1_im) {
            xfree(d1_re);
            xfree(d1_im);
            return RETURN_FAILURE;
        }
        /* pad with zeros */
        for (i = len; i < buflen; i++) {
            d1_re[i] = 0.0;
        }
        
        /* perform FT */
        fourier(d1_re, d1_im, buflen, FALSE);
        
        /* do the same with the second input if not autocorrelating */
        if (!autocor) {
            d2_re = copy_data_column(getcol(gcor, setcor, nc), len);
            d2_re = xrealloc(d2_re, SIZEOF_DOUBLE*buflen);
            d2_im = xcalloc(buflen, SIZEOF_DOUBLE);
            if (!d1_im || !d2_im) {
                xfree(d1_re);
                xfree(d2_re);
                xfree(d1_im);
                xfree(d2_im);
                return RETURN_FAILURE;
            }
            for (i = len; i < buflen; i++) {
                d2_re[i] = 0.0;
            }
            
            fourier(d2_re, d2_im, buflen, FALSE);
        } else {
            d2_re = NULL;
            d2_im = NULL;
        }
        
        /* multiply fourier */
        for (i = 0; i < buflen; i++) {
            if (autocor) {
                d1_re[i] = d1_re[i]*d1_re[i] + d1_im[i]*d1_im[i];
                d1_im[i] = 0.0;
            } else {
                double bufc;
                bufc     = d1_re[i]*d2_re[i] + d1_im[i]*d2_im[i];
                d1_im[i] = d2_re[i]*d1_im[i] - d1_re[i]*d2_im[i];
                d1_re[i] = bufc;
            }
        }
        
        /* these are no longer used */
        xfree(d2_re);
        xfree(d2_im);

        /* perform backward FT */
        fourier(d1_re, d1_im, buflen, TRUE);
        
        /* the imaginary part must be zero */
        xfree(d1_im);
        
        dres = getcol(gdest, setto, nc);
        for (i = 0; i < maxlag; i++) {
            dres[i] = d1_re[i]/buflen*xspace1;
        }
        
        /* free the remaining buffer storage */
        xfree(d1_re);
    }

    xdest = getcol(gdest, setto, DATA_X);
    for (i = 0; i < maxlag; i++) {
	xdest[i] = xoffset + i*xspace1;
    }

    if (autocor) {
	sprintf(buf,
            "Auto-correlation of set G%d.S%d at maximum lag %d",
            gsrc, setfrom, maxlag);
    } else {
	sprintf(buf,
            "Cross-correlation of sets G%d.S%d and G%d.S%d at maximum lag %d",
            gsrc, setfrom, gcor, setcor, maxlag);
    }
    setcomment(gdest, setto, buf);
    
    return RETURN_SUCCESS;
}


/*
 * numerical integration
 */
int do_int(int gsrc, int setfrom, int gdest, int setto,
    int disponly, double *sum)
{
    int len;
    double *x, *y;
    
    *sum = 0.0;

    if (!is_set_active(gsrc, setfrom)) {
	errmsg("Set not active");
	return RETURN_FAILURE;
    }
    
    len = getsetlength(gsrc, setfrom);
    if (len < 3) {
	errmsg("Set length < 3");
	return RETURN_FAILURE;
    }
    
    x = getcol(gsrc, setfrom, DATA_X);
    y = getcol(gsrc, setfrom, DATA_Y);
    if (!disponly) {
	if (activateset(gdest, setto)    != RETURN_SUCCESS ||
            setlength(gdest, setto, len) != RETURN_SUCCESS) {
	    errmsg("Can't activate target set");
	    return RETURN_FAILURE;
        } else {
	    *sum = trapint(x, y, getx(gdest, setto), gety(gdest, setto), len);
	    sprintf(buf, "Integral of set G%d.S%d", gsrc, setfrom);
	    setcomment(gdest, setto, buf);
	}
    } else {
	*sum = trapint(x, y, NULL, NULL, len);
    }
    return RETURN_SUCCESS;
}

/*
 * running properties
 */
int do_runavg(int gsrc, int setfrom, int gdest, int setto,
    int runlen, char *formula, int xplace)
{
    int i, nc, ncols, len, newlen;
    double *x1, *x2;
    grarr *t;

    if (!is_valid_setno(gsrc, setfrom)) {
	errmsg("Source set not active");
	return RETURN_FAILURE;
    }
    
    if (runlen < 1) {
	errmsg("Length of running average < 1");
	return RETURN_FAILURE;
    }

    len = getsetlength(gsrc, setfrom);
    if (runlen > len) {
	errmsg("Length of running average > set length");
	return RETURN_FAILURE;
    }
    
    if (!formula || formula[0] == '\0') {
	errmsg("Empty formula");
	return RETURN_FAILURE;
    }

    newlen = len - runlen + 1;
    activateset(gdest, setto);
    if (setlength(gdest, setto, newlen) != RETURN_SUCCESS) {
	return RETURN_FAILURE;
    }
    
    ncols = dataset_cols(gsrc, setfrom);
    if (dataset_cols(gdest, setto) != ncols) {
        set_dataset_type(gdest, setto, dataset_type(gsrc, setfrom));
    }
    
    t = get_parser_arr_by_name("$t");
    if (t == NULL) {
        t = define_parser_arr("$t");
        if (t == NULL) {
            errmsg("Internal error");
            return RETURN_FAILURE;
        }
    }
    
    if (t->length != 0) {
        XCFREE(t->data);
    }
    t->length = runlen;

    set_parser_setno(gsrc, setfrom);
    for (nc = 1; nc < ncols; nc++) {
        double *d1, *d2;
        d1 = getcol(gsrc, setfrom, nc);
        d2 = getcol(gdest, setto, nc);
        for (i = 0; i < newlen; i++) {
            t->data = &(d1[i]);
            if (s_scanner(formula, &(d2[i])) != RETURN_SUCCESS) {
                t->length = 0;
                t->data = NULL;
                return RETURN_FAILURE;
            }
        }
    }
    
    /* undefine the virtual array */
    t->length = 0;
    t->data = NULL;

    x1 = getcol(gsrc, setfrom, DATA_X);
    x2 = getcol(gdest, setto, DATA_X);
    for (i = 0; i < newlen; i++) {
        double dummy;
        switch (xplace) {
        case RUN_XPLACE_LEFT:
            x2[i] = x1[i];
            break;
        case RUN_XPLACE_RIGHT:
            x2[i] = x1[i + runlen - 1];
            break;
        default:
            stasum(&(x1[i]), runlen, &(x2[i]), &dummy);
            break;
        }
    }
    
    sprintf(buf, "%d-pt. running %s on G%d.S%d", runlen, formula, gsrc, setfrom);
    setcomment(gdest, setto, buf);
    
    return RETURN_SUCCESS;
}

int dump_dc(double *v, int len)
{
    int i;
    double avg, dummy;
    
    if (len < 1 || !v) {
        return RETURN_FAILURE;
    }
    
    stasum(v, len, &avg, &dummy);
    for (i = 0; i < len; i++) {
        v[i] -= avg;
    }
    
    return RETURN_SUCCESS;
}

int apply_window(double *v, int len, int window, double beta)
{
    int i;

    if (len < 2 || !v) {
        return RETURN_FAILURE;
    }
    
    if (window == FFT_WINDOW_NONE) {
        return RETURN_SUCCESS;
    }
    
    for (i = 0; i < len; i++) {
	double c, w, tmp;
        w = 2*M_PI*i/(len - 1);
        switch (window) {
	case FFT_WINDOW_TRIANGULAR:
	    c = 1.0 - fabs((i - 0.5*(len - 1))/(0.5*(len - 1)));
	    break;
	case FFT_WINDOW_PARZEN:
	    c = 1.0 - fabs((i - 0.5*(len - 1))/(0.5*(len + 1)));
	    break;
	case FFT_WINDOW_WELCH:
	    tmp = (i - 0.5*(len - 1))/(0.5*(len + 1));
            c = 1.0 - tmp*tmp;
	    break;
	case FFT_WINDOW_HANNING:
	    c = 0.5 - 0.5*cos(w);
	    break;
	case FFT_WINDOW_HAMMING:
	    c = 0.54 - 0.46*cos(w);
	    break;
	case FFT_WINDOW_BLACKMAN:
	    c = 0.42 - 0.5*cos(w) + 0.08*cos(2*w);
	    break;
	case FFT_WINDOW_FLATTOP:
	    c = 0.2810639 - 0.5208972*cos(w) + 0.1980399*cos(2*w);
	    break;
	case FFT_WINDOW_KAISER:
	    tmp = (i - 0.5*(len - 1))/(0.5*(len - 1));
            c = i0(beta*sqrt(1.0 - tmp*tmp))/i0(beta);
	    break;
	default:	/* should never happen */
            c = 0.0;
            return RETURN_FAILURE;
	    break;
        }
    
        v[i] *= c;
    }
    
    return RETURN_SUCCESS;
}

/*
 * Fourier transform
 */
int do_fourier(int gfrom, int setfrom, int gto, int setto,
    int invflag, int xscale, int norm, int complexin, int dcdump,
    double oversampling, int round2n, int window, double beta, int halflen,
    int output)
{
    int i, inlen, buflen, outlen, ncols, settype;
    double *in_x, *in_re, *in_im, *buf_re, *buf_im, *out_x, *out_y, *out_y1;
    double xspace, amp_correction;

    inlen = getsetlength(gfrom, setfrom);
    if (inlen < 2) {
	errmsg("Set length < 2");
	return RETURN_FAILURE;
    }

    if (activateset(gto, setto) != RETURN_SUCCESS) {
	errmsg("Can't activate target set");
        return RETURN_FAILURE;
    }
    
    /* get input */
    in_re = getcol(gfrom, setfrom, DATA_Y);
    if (!in_re) {
        /* should never happen */
        return RETURN_FAILURE;
    }
    if (!complexin) {
        in_im = NULL;
    } else {
        in_im = getcol(gfrom, setfrom, DATA_Y1);
    }
    
    in_x = getcol(gfrom, setfrom, DATA_X);
    if (monospaced(in_x, inlen, &xspace) != TRUE) {
        errmsg("Abscissas of the set are not monospaced, can't use for sampling");
        return RETURN_FAILURE;
    } else {
        if (xspace == 0.0) {
            errmsg("The set spacing is 0, can't continue");
            return RETURN_FAILURE;
        }
    }
    
    /* copy input data to buffer storage which will be used then to hold out */
    
    buf_re = copy_data_column(in_re, inlen);
    if (in_im) {
        buf_im = copy_data_column(in_im, inlen);
    } else {
        buf_im = xcalloc(inlen, SIZEOF_DOUBLE);
    }
    if (!buf_re || !buf_im) {
        xfree(buf_re);
        xfree(buf_im);
        return RETURN_FAILURE;
    }
    
    /* dump the DC component */
    if (dcdump) {
        dump_dc(buf_re, inlen);
    }
    
    /* apply data window */
    apply_window(buf_re, inlen, window, beta);
    if (in_im) {
        apply_window(buf_im, inlen, window, beta);
    }
    
    /* a safety measure */
    oversampling = MAX2(1.0, oversampling);
    
    buflen = (int) rint(inlen*oversampling);
    if (round2n) {
        /* round to the closest 2^N, but NOT throw away any data */
        int i2 = (int) rint(log2((double) buflen));
        buflen = (int) pow(2.0, (double) i2);
        if (buflen < inlen) {
            buflen *= 2;
        }
    }
    
    if (buflen != inlen) {
        buf_re = xrealloc(buf_re, SIZEOF_DOUBLE*buflen);
        buf_im = xrealloc(buf_im, SIZEOF_DOUBLE*buflen);
        
        if (!buf_re || !buf_im) {
            xfree(buf_re);
            xfree(buf_im);
            return RETURN_FAILURE;
        }
        
        /* stuff the added data with zeros */
        for (i = inlen; i < buflen; i++) {
            buf_re[i] = 0.0;
            buf_im[i] = 0.0;
        }
    }
    
    if (fourier(buf_re, buf_im, buflen, invflag) != RETURN_SUCCESS) {
        xfree(buf_re);
        xfree(buf_im);
        return RETURN_FAILURE;
    }
    
    /* amplitude correction due to the zero padding etc */
    amp_correction = (double) buflen/inlen;
    if ((invflag  && norm == FFT_NORM_BACKWARD) ||
        (!invflag && norm == FFT_NORM_FORWARD)) {
        amp_correction /= buflen;
    } else if (norm == FFT_NORM_SYMMETRIC) {
        amp_correction /= sqrt((double) buflen);
    }

    if (halflen && !complexin) {
	outlen = buflen/2 + 1;
        /* DC component */
        buf_re[0] = buf_re[0];
        buf_im[0] = 0.0;
        for (i = 1; i < outlen; i++) {
            /* carefully get amplitude of complex form: 
               use abs(a[i]) + abs(a[-i]) except for zero term */
            buf_re[i] += buf_re[buflen - i];
            buf_im[i] -= buf_im[buflen - i];
        }
    } else {
	outlen = buflen;
    }
    
    switch (output) {
    case FFT_OUTPUT_REIM:
    case FFT_OUTPUT_APHI:
        ncols = 3;
        settype = SET_XYZ;
        break;
    default:
        ncols = 2;
        settype = SET_XY;
        break;
    }
    
    if (dataset_cols(gto, setto) != ncols) {
        if (set_dataset_type(gto, setto, settype) != RETURN_SUCCESS) {
            xfree(buf_re);
            xfree(buf_im);
            return RETURN_FAILURE;
        } 
    }
    if (setlength(gto, setto, outlen) != RETURN_SUCCESS) {
        xfree(buf_re);
        xfree(buf_im);
        return RETURN_FAILURE;
    }
    
    out_y  = getcol(gto, setto, DATA_Y);
    out_y1 = getcol(gto, setto, DATA_Y1);
    
    for (i = 0; i < outlen; i++) {
        switch (output) {
        case FFT_OUTPUT_MAGNITUDE:
            out_y[i]  = amp_correction*hypot(buf_re[i], buf_im[i]);
            break;
        case FFT_OUTPUT_RE:
            out_y[i]  = amp_correction*buf_re[i];
            break;
        case FFT_OUTPUT_IM:
            out_y[i]  = amp_correction*buf_im[i];
            break;
        case FFT_OUTPUT_PHASE:
            out_y[i]  = atan2(buf_im[i], buf_re[i]);
            break;
        case FFT_OUTPUT_REIM:
            out_y[i]  = amp_correction*buf_re[i];
            out_y1[i] = amp_correction*buf_im[i];
            break;
        case FFT_OUTPUT_APHI:
            out_y[i]  = amp_correction*hypot(buf_re[i], buf_im[i]);
            out_y1[i] = atan2(buf_im[i], buf_re[i]);
            break;
        }
    }
    
    out_x  = getcol(gto, setto, DATA_X);
    for (i = 0; i < outlen; i++) {
        switch (xscale) {
	case FFT_XSCALE_NU:
	    out_x[i] = (double) i/(xspace*buflen);
	    break;
	case FFT_XSCALE_OMEGA:
	    out_x[i] = 2*M_PI*i/(xspace*buflen);
	    break;
	default:
	    out_x[i] = (double) i;
	    break;
	}
    }
    
    xfree(buf_re);
    xfree(buf_im);
    
    sprintf(buf, "FFT of set G%d.S%d", gfrom, setfrom);
    setcomment(gto, setto, buf);
    
    return RETURN_SUCCESS;
}


/*
 * histograms
 */
int do_histo(int fromgraph, int fromset, int tograph, int toset,
	      double *bins, int nbins, int cumulative, int normalize)
{
    int i, ndata;
    int *hist;
    double *x, *y, *data;
    set *p;
    char buf[64];
    
    if (!is_set_active(fromgraph, fromset)) {
	errmsg("Set not active");
	return RETURN_FAILURE;
    }
    if (nbins <= 0) {
	errmsg("Number of bins <= 0");
	return RETURN_FAILURE;
    }
    if (toset == SET_SELECT_NEXT) {
	toset = nextset(tograph);
    }
    if (!is_valid_setno(tograph, toset)) {
	errmsg("Can't activate destination set");
        return RETURN_FAILURE;
    }
    
    ndata = getsetlength(fromgraph, fromset);
    data = gety(fromgraph, fromset);
    
    hist = xmalloc(nbins*SIZEOF_INT);
    if (hist == NULL) {
        errmsg("xmalloc failed in do_histo()");
        return RETURN_FAILURE;
    }

    if (histogram(ndata, data, nbins, bins, hist) == RETURN_FAILURE){
        xfree(hist);
        return RETURN_FAILURE;
    }
    
    activateset(tograph, toset);
    setlength(tograph, toset, nbins + 1);
    x = getx(tograph, toset);
    y = gety(tograph, toset);
    
    x[0] = bins[0];
    y[0] = 0.0;
    for (i = 1; i < nbins + 1; i++) {
        x[i] = bins[i];
        y[i] = hist[i - 1];
        if (cumulative) {
            y[i] += y[i - 1];
        }
    }
    
    if (normalize) {
        for (i = 1; i < nbins + 1; i++) {
            double factor;
            if (cumulative) {
                factor = 1.0/ndata;
            } else {
                factor = 1.0/((bins[i] - bins[i - 1])*ndata);
            }
            y[i] *= factor;
        }
    }
    
    xfree(hist);

    p = set_get(tograph, toset);
    p->sym = SYM_NONE;
    p->linet = LINE_TYPE_LEFTSTAIR;
    p->dropline = TRUE;
    p->baseline = FALSE;
    p->baseline_type = BASELINE_TYPE_0;
    p->lines = 1;
    p->symlines = 1;
    sprintf(buf, "Histogram from G%d.S%d", fromgraph, fromset);
    p->comment = copy_string(p->comment, buf);

    return RETURN_SUCCESS;
}

int histogram(int ndata, double *data, int nbins, double *bins, int *hist)
{
    int i, j, bsign;
    double minval, maxval;
    
    if (nbins < 1) {
        errmsg("Number of bins < 1");
        return RETURN_FAILURE;
    }
    
    bsign = monotonicity(bins, nbins + 1, TRUE);
    if (bsign == 0) {
        errmsg("Non-monotonic bins");
        return RETURN_FAILURE;
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
    return RETURN_SUCCESS;
}


/*
 * sample a set, by start/step or logical expression
 */
void do_sample(int setno, int typeno, char *exprstr, int startno, int stepno)
{
    int len, npts = 0, i, resset;
    double *x, *y;
    int reslen;
    double *result;
    int gno = get_cg();

    if (!is_set_active(gno, setno)) {
	errmsg("Set not active");
	return;
    }

    len = getsetlength(gno, setno);

    resset = nextset(gno);
    if (resset < 0) {
	return;
    }

    x = getx(gno, setno);
    y = gety(gno, setno);

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
	for (i = startno - 1; i < len; i += stepno) {
	    add_point(gno, resset, x[i], y[i]);
	    npts++;
	}
	sprintf(buf, "Sample, %d, %d set #%d", startno, stepno, setno);
    } else {
        if (set_parser_setno(gno, setno) != RETURN_SUCCESS) {
	    errmsg("Bad set");
            killset(gno, resset);
	    return;
        }
        if (v_scanner(exprstr, &reslen, &result) != RETURN_SUCCESS) {
            killset(gno, resset);
	    return;
        }
        if (reslen != len) {
	    errmsg("Internal error");
            killset(gno, resset);
	    return;
        }

        npts = 0;
	sprintf(buf, "Sample from %d, using '%s'", setno, exprstr);
	for (i = 0; i < len; i++) {
	    if ((int) rint(result[i])) {
		add_point(gno, resset, x[i], y[i]);
		npts++;
	    }
	}
        xfree(result);
    }
    if (npts > 0) {
	setcomment(gno, resset, buf);
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
    add_point(get_cg(), resset, x[0], y[0]);
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
		add_point(get_cg(), resset, xtmp, ytmp);
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
		add_point(get_cg(), resset, xtmp, ytmp);
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
	    add_point(get_cg(), resset, x[k], y[k]);
	    npts++;
	    resx = getx(get_cg(), resset);
	    resy = gety(get_cg(), resset);
	}
	if (k == len - 2) {
	    add_point(get_cg(), resset, x[len-1], y[len-1]);
	    npts++;
	}
	sprintf(buf, "Prune from %d, %s dy = %g", setno, 
	    "Interpolation", deltay);
	break;
    }
    setcomment(get_cg(), resset, buf);
}


int interpolate(double *mesh, double *yint, int meshlen,
    double *x, double *y, int len, int method)
{
    double *b, *c, *d;
    double dx;
    int i, ifound;
    int m;

    /* For linear interpolation, non-strict monotonicity is fine */
    m = monotonicity(x, len, method == INTERP_LINEAR ? FALSE:TRUE);
    if (m == 0) {
        errmsg("Can't interpolate a set with non-monotonic abscissas");
        return RETURN_FAILURE;
    }

    switch (method) {
    case INTERP_SPLINE:
    case INTERP_ASPLINE:
        b = xcalloc(len, SIZEOF_DOUBLE);
        c = xcalloc(len, SIZEOF_DOUBLE);
        d = xcalloc(len, SIZEOF_DOUBLE);
        if (b == NULL || c == NULL || d == NULL) {
            xfree(b);
            xfree(c);
            xfree(d);
            return RETURN_FAILURE;
        }
        if (method == INTERP_ASPLINE){
            /* Akima spline */
            aspline(len, x, y, b, c, d);
        } else {
            /* Plain cubic spline */
            spline(len, x, y, b, c, d);
        }

	seval(mesh, yint, meshlen, x, y, b, c, d, len);

        xfree(b);
        xfree(c);
        xfree(d);
        break;
    default:
        /* linear interpolation */

        for (i = 0; i < meshlen; i++) {
            ifound = find_span_index(x, len, m, mesh[i]);
            if (ifound < 0) {
                ifound = 0;
            } else if (ifound > len - 2) {
                ifound = len - 2;
            }
            dx = x[ifound + 1] - x[ifound];
            if (dx) {
                yint[i] = y[ifound] + (mesh[i] - x[ifound])*
                    ((y[ifound + 1] - y[ifound])/dx);
            } else {
                yint[i] = (y[ifound] + y[ifound + 1])/2;
            }
        }
        break;
    }
    
    return RETURN_SUCCESS;
}

/* interpolate a set at abscissas from mesh
 * method - type of spline (or linear interpolation)
 * if strict is set, perform interpolation only within source set bounds
 * (i.e., no extrapolation)
 */
int do_interp(int gno_src, int setno_src, int gno_dest, int setno_dest,
    double *mesh, int meshlen, int method, int strict)
{
    int len, n, ncols;
    double *x, *xint;
    char *s;
	
    if (!is_valid_setno(gno_src, setno_src)) {
	errmsg("Interpolated set not active");
	return RETURN_FAILURE;
    }
    if (mesh == NULL || meshlen < 1) {
        errmsg("NULL sampling mesh");
        return RETURN_FAILURE;
    }
    
    len = getsetlength(gno_src, setno_src);
    ncols = dataset_cols(gno_src, setno_src);

    if (setno_dest == SET_SELECT_NEXT) {
	setno_dest = nextset(gno_dest);
    }
    if (!is_valid_setno(gno_dest, setno_dest)) {
	errmsg("Can't activate destination set");
	return RETURN_FAILURE;
    }

    if (dataset_cols(gno_dest, setno_dest) != ncols) {
        copyset(gno_src, setno_src, gno_dest, setno_dest);
    }
    
    setlength(gno_dest, setno_dest, meshlen);
    activateset(gno_dest, setno_dest);
    
    x = getcol(gno_src, setno_src, DATA_X);
    for (n = 1; n < ncols; n++) {
        int res;
        double *y, *yint;
        
        y    = getcol(gno_src, setno_src, n);
        yint = getcol(gno_dest, setno_dest, n);
        
        res = interpolate(mesh, yint, meshlen, x, y, len, method);
        if (res != RETURN_SUCCESS) {
            killset(gno_dest, setno_dest);
            return RETURN_FAILURE;
        }
    }

    xint = getcol(gno_dest, setno_dest, DATA_X);
    memcpy(xint, mesh, meshlen*SIZEOF_DOUBLE);

    if (strict) {
        double xmin, xmax;
        int i, imin, imax;
        minmax(x, len, &xmin, &xmax, &imin, &imax);
        for (i = meshlen - 1; i >= 0; i--) {
            if (xint[i] < xmin || xint[i] > xmax) {
                del_point(gno_dest, setno_dest, i);
            }
        }
    }
    
    switch (method) {
    case INTERP_SPLINE:
        s = "cubic spline";
        break;
    case INTERP_ASPLINE:
        s = "Akima spline";
        break;
    default:
        s = "linear interpolation";
        break;
    }
    sprintf(buf, "Interpolated from G%d.S%d using %s", gno_src, setno_src, s);
    setcomment(gno_dest, setno_dest, buf);
    
    return RETURN_SUCCESS;
}

int get_restriction_array(int gno, int setno,
    int rtype, int negate, char **rarray)
{
    int i, n, regno;
    double *x, *y;
    world w;
    WPoint wp;
    
    if (rtype == RESTRICT_NONE) {
        *rarray = NULL;
        return RETURN_SUCCESS;
    }
    
    n = getsetlength(gno, setno);
    if (n <= 0) {
        *rarray = NULL;
        return RETURN_FAILURE;
    }
    
    *rarray = xmalloc(n*SIZEOF_CHAR);
    if (*rarray == NULL) {
        return RETURN_FAILURE;
    }
    
    x = getcol(gno, setno, DATA_X);
    y = getcol(gno, setno, DATA_Y);
    
    switch (rtype) {
    case RESTRICT_REG0:
    case RESTRICT_REG1:
    case RESTRICT_REG2:
    case RESTRICT_REG3:
    case RESTRICT_REG4:
        regno = rtype - RESTRICT_REG0;
        for (i = 0; i < n; i++) {
            (*rarray)[i] = inregion(regno, x[i], y[i]) ? !negate : negate;
        }
        break;
    case RESTRICT_WORLD:
        get_graph_world(gno, &w);
        for (i = 0; i < n; i++) {
            wp.x = x[i];
            wp.y = y[i];
            (*rarray)[i] = is_wpoint_inside(&wp, &w) ? !negate : negate;
        }
        break;
    default:
        errmsg("Internal error in get_restriction_array()");
        XCFREE(*rarray);
        return RETURN_FAILURE;
        break;
    }
    return RETURN_SUCCESS;
}

int monotonicity(double *array, int len, int strict)
{
    int i;
    int s0, s1;
    
    if (len < 2) {
        errmsg("Monotonicity of an array of length < 2 is meaningless");
        return 0;
    }
    
    s0 = sign(array[1] - array[0]);
    for (i = 2; i < len; i++) {
        s1 = sign(array[i] - array[i - 1]);
        if (s1 != s0) {
            if (strict) {
                return 0;
            } else if (s0 == 0) {
                s0 = s1;
            } else if (s1 != 0) {
                return 0;
            }
        }
    }
    
    return s0;
}

int monospaced(double *array, int len, double *space)
{
    int i;
    double eps;
    
    if (len < 2) {
        errmsg("Monospacing of an array of length < 2 is meaningless");
        return FALSE;
    }
    
    *space = array[1] - array[0];
    eps = fabs((array[len - 1] - array[0]))*1.0e-6; /* FIXME */
    for (i = 2; i < len; i++) {
        if (fabs(array[i] - array[i - 1] - *space) > eps) {
            return FALSE;
        }
    }
    
    return TRUE;
}

int find_span_index(double *array, int len, int m, double x)
{
    int ind, low = 0, high = len - 1;
    
    if (len < 2 || m == 0) {
        errmsg("find_span_index() called with a non-monotonic array");
        return -2;
    } else if (m > 0) {
        /* ascending order */
        if (x < array[0]) {
            return -1;
        } else if (x > array[len - 1]) {
            return len - 1;
        } else {
            while (low <= high) {
	        ind = (low + high) / 2;
	        if (x < array[ind]) {
	            high = ind - 1;
	        } else if (x > array[ind + 1]) {
	            low = ind + 1;
	        } else {
	            return ind;
	        }
            }
        }
    } else {
        /* descending order */
        if (x > array[0]) {
            return -1;
        } else if (x < array[len - 1]) {
            return len - 1;
        } else {
            while (low <= high) {
	        ind = (low + high) / 2;
	        if (x > array[ind]) {
	            high = ind - 1;
	        } else if (x < array[ind + 1]) {
	            low = ind + 1;
	        } else {
	            return ind;
	        }
            }
        }
    }

    /* should never happen */
    errmsg("internal error in find_span_index()");
    return -2;
}

/* feature extraction */
int featext(int gfrom, int *sfrom, int nsets, int gto, int setto, char *formula)
{
    int i;
    char *tbuf;
    double *x, *y;

    if (!is_valid_gno(gfrom) || !is_valid_gno(gto)) {
	return RETURN_FAILURE;
    }
    
    if (activateset(gto, setto) != RETURN_SUCCESS) {
        return RETURN_FAILURE;
    }
    
    if (dataset_cols(gto, setto) != 2) {
        set_dataset_type(gto, setto, SET_XY);
    }
    
    if (setlength(gto, setto, nsets) != RETURN_SUCCESS) {
        return RETURN_FAILURE;
    }
    
    x = getcol(gto, setto, DATA_X);
    y = getcol(gto, setto, DATA_Y);
    for (i = 0; i < nsets; i++) {
        int setno = sfrom[i];
	set_parser_setno(gfrom, setno);
        x[i] = (double) i;
        if (s_scanner(formula, &y[i]) != RETURN_SUCCESS) {
            return RETURN_FAILURE;
        }
    }

    /* set comment */
    tbuf = copy_string(NULL, "Feature extraction by formula ");
    tbuf = concat_strings(tbuf, formula);
    setcomment(gto, setto, tbuf);
    xfree(tbuf);
    
    return RETURN_SUCCESS;
}
