/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 * 
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 * 
 * Copyright (c) 1991-95 Paul J Turner, Portland, OR
 * Copyright (c) 1996-99 Grace Development Team
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
 * utilities for graphs
 *
 */

#include <config.h>
#include <cmath.h>

#include <stdio.h>

#include "globals.h"
#include "utils.h"
#include "draw.h"
#include "device.h"
#include "graphs.h"
#include "graphutils.h"
#include "protos.h"

extern char print_file[];

static void auto_ticks(int gno, int axis);

char *get_format_types(int f)
{
    static char s[128];

    strcpy(s, "decimal");
    switch (f) {
    case FORMAT_DECIMAL:
	strcpy(s, "decimal");
	break;
    case FORMAT_EXPONENTIAL:
	strcpy(s, "exponential");
	break;
    case FORMAT_GENERAL:
	strcpy(s, "general");
	break;
    case FORMAT_POWER:
	strcpy(s, "power");
	break;
    case FORMAT_SCIENTIFIC:
	strcpy(s, "scientific");
	break;
    case FORMAT_ENGINEERING:
	strcpy(s, "engineering");
	break;
    case FORMAT_DDMMYY:
	strcpy(s, "ddmmyy");
	break;
    case FORMAT_MMDDYY:
	strcpy(s, "mmddyy");
	break;
    case FORMAT_YYMMDD:
	strcpy(s, "yymmdd");
	break;
    case FORMAT_MMYY:
	strcpy(s, "mmyy");
	break;
    case FORMAT_MMDD:
	strcpy(s, "mmdd");
	break;
    case FORMAT_MONTHDAY:
	strcpy(s, "monthday");
	break;
    case FORMAT_DAYMONTH:
	strcpy(s, "daymonth");
	break;
    case FORMAT_MONTHS:
	strcpy(s, "months");
	break;
    case FORMAT_MONTHSY:
	strcpy(s, "monthsy");
	break;
    case FORMAT_MONTHL:
	strcpy(s, "monthl");
	break;
    case FORMAT_DAYOFWEEKS:
	strcpy(s, "dayofweeks");
	break;
    case FORMAT_DAYOFWEEKL:
	strcpy(s, "dayofweekl");
	break;
    case FORMAT_DAYOFYEAR:
	strcpy(s, "dayofyear");
	break;
    case FORMAT_HMS:
	strcpy(s, "hms");
	break;
    case FORMAT_MMDDHMS:
	strcpy(s, "mmddhms");
	break;
    case FORMAT_MMDDYYHMS:
	strcpy(s, "mmddyyhms");
	break;
    case FORMAT_YYMMDDHMS:
	strcpy(s, "yymmddhms");
	break;
    case FORMAT_DEGREESLON:
	strcpy(s, "degreeslon");
	break;
    case FORMAT_DEGREESMMLON:
	strcpy(s, "degreesmmlon");
	break;
    case FORMAT_DEGREESMMSSLON:
	strcpy(s, "degreesmmsslon");
	break;
    case FORMAT_MMSSLON:
	strcpy(s, "mmsslon");
	break;
    case FORMAT_DEGREESLAT:
	strcpy(s, "degreeslat");
	break;
    case FORMAT_DEGREESMMLAT:
	strcpy(s, "degreesmmlat");
	break;
    case FORMAT_DEGREESMMSSLAT:
	strcpy(s, "degreesmmsslat");
	break;
    case FORMAT_MMSSLAT:
	strcpy(s, "mmsslat");
	break;
    }
    return s;
}


int wipeout(void)
{
    if (!noask && is_dirtystate()) {
        if (!yesno("Abandon unsaved changes?", NULL, NULL, NULL)) {
            return 1;
        }
    }
    kill_all_graphs();
    do_clear_lines();
    do_clear_boxes();
    do_clear_ellipses();
    do_clear_text();
    reset_project_version();
    map_fonts(FONT_MAP_DEFAULT);
    set_docname(NULL);
    set_project_description(NULL);
    print_file[0] = '\0';
    /* a hack! the global "curtype" (as well as all others) should be removed */
    curtype = SET_XY;
    clear_dirtystate();
    return 0;
}


/* The following routines determine default axis range and tickmarks */

static void autorange_byset(int gno, int setno, int autos_type);
static double nicenum(double x, int nrange, int round);

#define NICE_FLOOR   0
#define NICE_CEIL    1
#define NICE_ROUND   2

void autotick_axis(int gno, int axis)
{
    switch (axis) {
    case ALL_AXES:
        auto_ticks(gno, X_AXIS);
        auto_ticks(gno, ZX_AXIS);
        auto_ticks(gno, Y_AXIS);
        auto_ticks(gno, ZY_AXIS);
        break;
    case ALL_X_AXES:
        auto_ticks(gno, X_AXIS);
        auto_ticks(gno, ZX_AXIS);
        break;
    case ALL_Y_AXES:
        auto_ticks(gno, Y_AXIS);
        auto_ticks(gno, ZY_AXIS);
        break;
    default:
        auto_ticks(gno, axis);
        break;
    }
}

void autoscale_byset(int gno, int setno, int autos_type)
{
    if ((setno == ALL_SETS && is_valid_gno(gno)) || is_set_active(gno, setno)) {
	autorange_byset(gno, setno, autos_type);
	switch (autos_type) {
        case AUTOSCALE_X:
            autotick_axis(gno, ALL_X_AXES);
            break;
        case AUTOSCALE_Y:
            autotick_axis(gno, ALL_Y_AXES);
            break;
        case AUTOSCALE_XY:
            autotick_axis(gno, ALL_AXES);
            break;
        }
    }
}

int autoscale_graph(int gno, int autos_type)
{
    if (number_of_active_sets(gno) > 0) {
        autoscale_byset(gno, ALL_SETS, autos_type);
        return RETURN_SUCCESS;
    } else {
        return RETURN_FAILURE;
    }
}

static void round_axis_limits(double *amin, double *amax, int scale)
{
    double smin, smax;
    int nrange;
    
    if (*amin == *amax) {
        switch (sign(*amin)) {
        case 0:
            *amin = -1.0;
            *amax = +1.0;
            break;
        case 1:
            *amin /= 2.0;
            *amax *= 2.0;
            break;
        case -1:
            *amin *= 2.0;
            *amax /= 2.0;
            break;
        }
    } 
    
    if (scale == SCALE_LOG) {
        if (*amax <= 0.0) {
            errmsg("Can't autoscale a log axis by non-positive data");
            *amax = 10.0;
            *amin = 1.0;
            return;
        } else if (*amin <= 0.0) {
            errmsg("Data have non-positive values");
            *amin = *amax/1.0e3;
        }
        smin = log10(*amin);
        smax = log10(*amax);
    } else {
        smin = *amin;
        smax = *amax;
    }

    if (sign(smin) == sign(smax)) {
        nrange = -rint(log10(fabs(2*(smax - smin)/(smax + smin))));
        nrange = MAX2(0, nrange);
    } else {
        nrange = 0;
    }
    smin = nicenum(smin, nrange, NICE_FLOOR);
    smax = nicenum(smax, nrange, NICE_CEIL);
    if (sign(smin) == sign(smax)) {
        if (smax/smin > 5.0) {
            smin = 0.0;
        } else if (smin/smax > 5.0) {
            smax = 0.0;
        }
    }
    
    if (scale == SCALE_LOG) {
        *amin = pow(10.0, smin);
        *amax = pow(10.0, smax);
    } else {
        *amin = smin;
        *amax = smax;
    }
}

static void autorange_byset(int gno, int setno, int autos_type)
{
    world w;
    double xmax, xmin, ymax, ymin;
    int scale;

    if (autos_type == AUTOSCALE_NONE) {
        return;
    }
    
    get_graph_world(gno, &w);
    
    if (get_graph_type(gno) == GRAPH_SMITH) {
        if (autos_type == AUTOSCALE_X || autos_type == AUTOSCALE_XY) {
            w.xg1 = -1.0;
            w.yg1 = -1.0;
        }
        if (autos_type == AUTOSCALE_Y || autos_type == AUTOSCALE_XY) {
            w.xg2 = 1.0;
            w.yg2 = 1.0;
	}
        set_graph_world(gno, w);
        return;
    }

    xmin=w.xg1;
    xmax=w.xg2;
    ymin=w.yg1;
    ymax=w.yg2;
    if (autos_type == AUTOSCALE_XY) {
        getsetminmax(gno, setno, &xmin, &xmax, &ymin, &ymax);
    } else if (autos_type == AUTOSCALE_X) {
        getsetminmax_c(gno, setno, &xmin, &xmax, &ymin, &ymax, 2);
    } else if (autos_type == AUTOSCALE_Y) {
        getsetminmax_c(gno, setno, &xmin, &xmax, &ymin, &ymax, 1);
    }

    if (autos_type == AUTOSCALE_X || autos_type == AUTOSCALE_XY) {
        scale = get_graph_xscale(gno);
        round_axis_limits(&xmin, &xmax, scale);
        w.xg1 = xmin;
        w.xg2 = xmax;
    }

    if (autos_type == AUTOSCALE_Y || autos_type == AUTOSCALE_XY) {
        scale = get_graph_yscale(gno);
        round_axis_limits(&ymin, &ymax, scale);
        w.yg1 = ymin;
        w.yg2 = ymax;
    }

    set_graph_world(gno, w);
}

static void auto_ticks(int gno, int axis)
{
    tickmarks *t;
    world w;
    double range, d, tmpmax, tmpmin;
    int axis_scale;

    t = get_graph_tickmarks(gno, axis);
    if (t == NULL) {
        return;
    }
    get_graph_world(gno, &w);

    if (is_xaxis(axis)) {
        tmpmin = w.xg1;
        tmpmax = w.xg2;
        axis_scale = get_graph_xscale(gno);
    } else {
        tmpmin = w.yg1;
        tmpmax = w.yg2;
        axis_scale = get_graph_yscale(gno);
    }

    if (axis_scale == SCALE_LOG) {
	if (t->tmajor <= 1.0) {
            t->tmajor = 10.0;
        }
        tmpmax = log10(tmpmax)/log10(t->tmajor);
	tmpmin = log10(tmpmin)/log10(t->tmajor);
    } else if (t->tmajor <= 0.0) {
        t->tmajor = 1.0;
    }
    if (t->nminor < 0) {
	t->nminor = 1;
    }
    
    range = tmpmax - tmpmin;
    if (axis_scale != SCALE_LOG) {
        d = nicenum(range/(t->t_autonum - 1), 0, NICE_ROUND);
	t->tmajor = d;
        t->nminor = 1;
    } else {
        d = ceil(range/(t->t_autonum - 1));
	t->tmajor = pow(t->tmajor, d);
        t->nminor = 9;
    }
    
    set_dirtystate();
}

/*
 * nicenum: find a "nice" number approximately equal to x
 */

static double nicenum(double x, int nrange, int round)
{
    int xsign;
    double f, y, fexp, rx, sx;
    
    if (x == 0.0) {
        return(0.0);
    }

    xsign = sign(x);
    x = fabs(x);

    fexp = floor(log10(x)) - nrange;
    sx = x/pow(10.0, fexp)/10.0;            /* scaled x */
    rx = floor(sx);                         /* rounded x */
    f = 10*(sx - rx);                       /* fraction between 0 and 10 */

    if ((round == NICE_FLOOR && xsign == +1) ||
        (round == NICE_CEIL  && xsign == -1)) {
        y = (int) floor(f);
    } else if ((round == NICE_FLOOR && xsign == -1) ||
               (round == NICE_CEIL  && xsign == +1)) {
	y = (int) ceil(f);
    } else {    /* round == NICE_ROUND */
	if (f < 1.5)
	    y = 1;
	else if (f < 3.)
	    y = 2;
	else if (f < 7.)
	    y = 5;
	else
	    y = 10;
    }
    
    sx = rx + (double) y/10.0;
    
    return (xsign*sx*10.0*pow(10.0, fexp));
}

/*
 * set scroll amount
 */
void scroll_proc(int value)
{
    scrollper = value / 100.0;
}

void scrollinout_proc(int value)
{
    shexper = value / 100.0;
}

/*
 * pan through world coordinates
 */
int graph_scroll(int type)
{
    world w;
    double dwc = 0.0;
    int gstart, gstop, i;

    if (scrolling_islinked) {
        gstart = 0;
        gstop = number_of_graphs() - 1;
    } else {
        gstart = get_cg();
        gstop = gstart;
    }
    
    for (i = gstart; i <= gstop; i++) {
        if (get_graph_world(i, &w) == RETURN_SUCCESS) {
            switch (type) {
            case GSCROLL_LEFT:    
            case GSCROLL_RIGHT:    
                if (islogx(i) == TRUE) {
                    errmsg("Scrolling of LOG axes is not implemented");
                    return RETURN_FAILURE;
                }
                dwc = scrollper * (w.xg2 - w.xg1);
                break;
            case GSCROLL_DOWN:    
            case GSCROLL_UP:    
                if (islogy(i) == TRUE) {
                    errmsg("Scrolling of LOG axes is not implemented");
                    return RETURN_FAILURE;
                }
                dwc = scrollper * (w.yg2 - w.yg1);
                break;
            }
            
            switch (type) {
            case GSCROLL_LEFT:    
                w.xg1 -= dwc;
                w.xg2 -= dwc;
                break;
            case GSCROLL_RIGHT:    
                w.xg1 += dwc;
                w.xg2 += dwc;
                break;
            case GSCROLL_DOWN:    
                w.yg1 -= dwc;
                w.yg2 -= dwc;
                break;
            case GSCROLL_UP:    
                w.yg1 += dwc;
                w.yg2 += dwc;
                break;
            }
            set_graph_world(i, w);
        }
    }
    
    return RETURN_SUCCESS;
}

int graph_zoom(int type)
{
    double dx, dy;
    world w;
    int gstart, gstop, gno;

    if (scrolling_islinked) {
        gstart = 0;
        gstop = number_of_graphs() - 1;
    } else {
        gstart = get_cg();
        gstop = gstart;
    }
    
    for (gno = gstart; gno <= gstop; gno++) {
        if (!islogx(gno) && !islogy(gno)) {
            if (get_graph_world(gno, &w) == RETURN_SUCCESS) {
                dx = shexper * (w.xg2 - w.xg1);
                dy = shexper * (w.yg2 - w.yg1);
                if (type == GZOOM_SHRINK) {
                    dx *= -1;
                    dy *= -1;
                }
 
                w.xg1 -= dx;
                w.xg2 += dx;
                w.yg1 -= dy;
                w.yg2 += dy;
 
                set_graph_world(gno, w);
            }
        } else {
            errmsg("Zooming is not implemented for LOG plots");
            return RETURN_FAILURE;
        }
    }
    
    return RETURN_SUCCESS;
}

/*
 *  Arrange procedures
 */
void arrange_graphs(int grows, int gcols)
{
    double hgap, vgap; /* inter-graph gaps*/
    double sx, sy; /* offsets */
    double wx, wy;
    double vx, vy;

    if (gcols < 1 ||  grows < 1) {
        return;
    }
    
    get_page_viewport(&vx, &vy);
    sx = 0.1;
    sy = 0.1;
    hgap = 0.07;
    vgap = 0.07;
    wx = ((vx - 2*sx) - (gcols - 1)*hgap)/gcols;
    wy = ((vy - 2*sy) - (grows - 1)*vgap)/grows;
    
    arrange_graphs2(grows, gcols, hgap, vgap, sx, sy, wx, wy);
}

int arrange_graphs2(int grows, int gcols, double vgap, double hgap,
		    double sx, double sy, double wx, double wy)
{
    int i, j;
    int gtmp = 0;
    view v;

    if (gcols < 1 || grows < 1) {
        return RETURN_FAILURE;
    }
    
    for (i = 0; i < gcols; i++) {
        for (j = 0; j < grows; j++) {
            if (!is_graph_active(gtmp)) {
                set_graph_active(gtmp, TRUE);
            }
            v.xv1 = sx + i*(hgap + wx);
            v.xv2 = v.xv1 + wx;
            v.yv1 = sy + j*(vgap + wy);
            v.yv2 = v.yv1 + wy;
            set_graph_viewport(gtmp, v);
            gtmp++;
        }
    }
    return RETURN_SUCCESS;
}

void define_arrange(int nrows, int ncols, int pack,
       double vgap, double hgap, double sx, double sy, double wx, double wy)
{
    int i, j, k, gno;

    if (arrange_graphs2(nrows, ncols, vgap, hgap, sx, sy, wx, wy) != RETURN_SUCCESS) {
	return;
    }
    
    switch (pack) {
    case 0:
	for (j = 0; j < ncols; j++) {
	    for (i = 0; i < nrows; i++) {
		gno = i + j * nrows;
		for (k = 0; k < MAXAXES; k++) {
		    activate_tick_labels(gno, k, TRUE);
		}
	    }
	}
	break;
    case 1:
	hgap = 0.0;
	for (j = 1; j < ncols; j++) {
	    for (i = 0; i < nrows; i++) {
		gno = i + j * nrows;
		for (k = 0; k < MAXAXES; k++) {
		    if (is_yaxis(k) == TRUE) {
                        activate_tick_labels(gno, k, FALSE);
                    }
		}
	    }
	}
	break;
    case 2:
	vgap = 0.0;
	for (j = 0; j < ncols; j++) {
	    for (i = 1; i < nrows; i++) {
		gno = i + j * nrows;
		for (k = 0; k < MAXAXES; k++) {
		    if (is_xaxis(k) == TRUE) {
		        activate_tick_labels(gno, k, FALSE);
                    }
		}
	    }
	}
	break;
    case 3:
	hgap = 0.0;
	vgap = 0.0;
	for (j = 1; j < ncols; j++) {
	    for (i = 0; i < nrows; i++) {
		gno = i + j * nrows;
		for (k = 0; k < MAXAXES; k++) {
		    if (is_yaxis(k) == TRUE) {
		        activate_tick_labels(gno, k, FALSE);
                    }
		}
	    }
	}
	for (j = 0; j < ncols; j++) {
	    for (i = 1; i < nrows; i++) {
		gno = i + j * nrows;
		for (k = 0; k < MAXAXES; k++) {
		    if (is_xaxis(k) == TRUE) {
		        activate_tick_labels(gno, k, FALSE);
                    }
		}
	    }
	}
	break;
    }
    set_dirtystate();
}

void move_legend(int gno, VVector shift)
{
    double xtmp, ytmp;
    legend l;

    if (is_valid_gno(gno)) {
        get_graph_legend(gno, &l);
        if (l.loctype == COORD_VIEW) {
            l.legx += shift.x;
            l.legy += shift.y;
        } else {
            world2view(l.legx, l.legy, &xtmp, &ytmp);
            xtmp += shift.x;
            ytmp += shift.y;
            view2world(xtmp, ytmp, &l.legx, &l.legy);
        }
        set_graph_legend(gno, &l);
        set_dirtystate();
    }
}

void move_timestamp(VVector shift)
{
    timestamp.x += shift.x;
    timestamp.y += shift.y;
    set_dirtystate();
}

void rescale_viewport(double ext_x, double ext_y)
{
    int i, gno;
    view v;
    legend leg;
    linetype l;
    boxtype b;
    ellipsetype e;
    plotstr s;
    
    for (gno = 0; gno < number_of_graphs(); gno++) {
        get_graph_viewport(gno, &v);
        v.xv1 *= ext_x;
        v.xv2 *= ext_x;
        v.yv1 *= ext_y;
        v.yv2 *= ext_y;
        set_graph_viewport(gno, v);
        
        get_graph_legend(gno, &leg);
        if (leg.loctype == COORD_VIEW) {
            leg.legx *= ext_x;
            leg.legy *= ext_y;
            set_graph_legend(gno, &leg);
        }
        
        /* TODO: tickmark offsets */
    }

    for (i = 0; i < number_of_lines(); i++) {
        get_graph_line(i, &l);
        if (l.loctype == COORD_VIEW) {
            l.x1 *= ext_x;
            l.x2 *= ext_x;
            l.y1 *= ext_y;
            l.y2 *= ext_y;
            set_graph_line(i, &l);
        }
    }
    for (i = 0; i < number_of_boxes(); i++) {
        get_graph_box(i, &b);
        if (b.loctype == COORD_VIEW) {
            b.x1 *= ext_x;
            b.x2 *= ext_x;
            b.y1 *= ext_y;
            b.y2 *= ext_y;
            set_graph_box(i, &b);
        }
    }
    for (i = 0; i < number_of_ellipses(); i++) {
        get_graph_ellipse(i, &e);
        if (e.loctype == COORD_VIEW) {
            e.x1 *= ext_x;
            e.x2 *= ext_x;
            e.y1 *= ext_y;
            e.y2 *= ext_y;
            set_graph_ellipse(i, &e);
        }
    }
    for (i = 0; i < number_of_strings(); i++) {
        get_graph_string(i, &s);
        if (s.loctype == COORD_VIEW) {
            s.x *= ext_x;
            s.y *= ext_y;
            set_graph_string(i, &s);
        }
    }
}

int overlay_graphs(int gsec, int gpri, int type)
{
    int i;
    tickmarks *tsec, *tpri;
    world wpri, wsec;
    view v;
    
    if (gsec == gpri) {
        return RETURN_FAILURE;
    }
    if (is_valid_gno(gpri) == FALSE || is_valid_gno(gsec) == FALSE) {
        return RETURN_FAILURE;
    }
    
    get_graph_viewport(gpri, &v);
    get_graph_world(gpri, &wpri);
    get_graph_world(gsec, &wsec);

    switch (type) {
    case GOVERLAY_SMART_AXES_XY:
        wsec = wpri;
	for (i = 0; i < MAXAXES; i++) {
	    tpri = get_graph_tickmarks(gpri, i);
	    tsec = get_graph_tickmarks(gsec, i);
            switch(i) {
            case X_AXIS:
            case Y_AXIS:
                tpri->active = TRUE;
	        tpri->label_op = PLACEMENT_NORMAL;
	        tpri->t_op = PLACEMENT_BOTH;
	        tpri->tl_op = PLACEMENT_NORMAL;

	        tsec->active = FALSE;
                break;
            default:
                /* don't touch alternative axes */
                break;
            }
	}
	break;
    case GOVERLAY_SMART_AXES_X:
        wsec.xg1 = wpri.xg1;
        wsec.xg2 = wpri.xg2;
	for (i = 0; i < MAXAXES; i++) {
	    tpri = get_graph_tickmarks(gpri, i);
	    tsec = get_graph_tickmarks(gsec, i);
	    switch(i) {
            case X_AXIS:
                tpri->active = TRUE;
	        tpri->label_op = PLACEMENT_NORMAL;
	        tpri->t_op = PLACEMENT_BOTH;
	        tpri->tl_op = PLACEMENT_NORMAL;

	        tsec->active = FALSE;
                break;
            case Y_AXIS:
	        tpri->active = TRUE;
	        tpri->label_op = PLACEMENT_NORMAL;
	        tpri->t_op = PLACEMENT_NORMAL;
	        tpri->tl_op = PLACEMENT_NORMAL;

                tsec->active = TRUE;
	        tsec->label_op = PLACEMENT_OPPOSITE;
	        tsec->t_op = PLACEMENT_OPPOSITE;
	        tsec->tl_op = PLACEMENT_OPPOSITE;
                break;
            default:
                /* don't touch alternative axes */
                break;
            }
	}
	break;
    case GOVERLAY_SMART_AXES_Y:
        wsec.yg1 = wpri.yg1;
        wsec.yg2 = wpri.yg2;
	for (i = 0; i < MAXAXES; i++) {
	    tpri = get_graph_tickmarks(gpri, i);
	    tsec = get_graph_tickmarks(gsec, i);
	    switch(i) {
            case X_AXIS:
	        tpri->active = TRUE;
	        tpri->label_op = PLACEMENT_NORMAL;
	        tpri->t_op = PLACEMENT_NORMAL;
	        tpri->tl_op = PLACEMENT_NORMAL;

                tsec->active = TRUE;
	        tsec->label_op = PLACEMENT_OPPOSITE;
	        tsec->t_op = PLACEMENT_OPPOSITE;
	        tsec->tl_op = PLACEMENT_OPPOSITE;
                break;
            case Y_AXIS:
                tpri->active = TRUE;
	        tpri->label_op = PLACEMENT_NORMAL;
	        tpri->t_op = PLACEMENT_BOTH;
	        tpri->tl_op = PLACEMENT_NORMAL;

	        tsec->active = FALSE;
                break;
            default:
                /* don't touch alternative axes */
                break;
            }
	}
	break;
    case GOVERLAY_SMART_AXES_NONE:
	for (i = 0; i < MAXAXES; i++) {
	    tpri = get_graph_tickmarks(gpri, i);
	    tsec = get_graph_tickmarks(gsec, i);
	    switch(i) {
            case X_AXIS:
            case Y_AXIS:
	        tpri->active = TRUE;
	        tpri->label_op = PLACEMENT_NORMAL;
	        tpri->t_op = PLACEMENT_NORMAL;
	        tpri->tl_op = PLACEMENT_NORMAL;

                tsec->active = TRUE;
	        tsec->label_op = PLACEMENT_OPPOSITE;
	        tsec->t_op = PLACEMENT_OPPOSITE;
	        tsec->tl_op = PLACEMENT_OPPOSITE;
                break;
            default:
                /* don't touch alternative axes */
                break;
            }
	}
	break;
    default:
        break;
    }
    
    /* set identical viewports */
    set_graph_viewport(gsec, v);
    
    /* update world coords */
    set_graph_world(gsec, wsec);

    return RETURN_SUCCESS;
}
