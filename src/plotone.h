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
 * plotone.h
 */

#ifndef __PLOTONE_H_
#define __PLOTONE_H_

#include "defines.h"
#include "draw.h"

#define BAR_HORIZONTAL  0
#define BAR_VERTICAL    1

void drawgraph(void);
void do_hardcopy(void);

void plotone(int gno);

void xyplot(int gno);
void draw_smith_chart(int gno);
void draw_polar_graph(int gno);

void drawframe(int gno);
void fillframe(int gno);

void drawsetfill(int gno, int setno, plotarr *p,
                 int refn, double *refx, double *refy, double offset);
void drawsetline(int gno, int setno, plotarr *p,
                 int refn, double *refx, double *refy, double offset);
void drawsetbars(int gno, int setno, plotarr *p,
                 int refn, double *refx, double *refy, double offset);
void drawsetsyms(int gno, int setno, plotarr *p,
                 int refn, double *refx, double *refy, double offset);
void drawsetavalues(int gno, int setno, plotarr *p,
                 int refn, double *refx, double *refy, double offset);
void drawseterrbars(int gno, int setno, plotarr *p,
                 int refn, double *refx, double *refy, double offset);
void drawsethilo(plotarr *p);
void drawcirclexy(plotarr *p);

int drawxysym(VPoint vp, int symtype, Pen sympen, Pen symfillpen, char s);
void drawerrorbar(WPoint wp, double offset, double ebarlen, int orient);

void draw_region(int r);

void draw_objects(int gno);
void draw_string(int gno, int i);
void draw_box(int gno, int i);
void draw_ellipse(int gno, int i);
void draw_line(int gno, int i);

void draw_arrowhead(VPoint vp1, VPoint vp2, const Arrow *arrowp);

void dolegend(int gno);
void putlegends(int gno, VPoint vp, double ldist, double sdist, double yskip);

void draw_titles(int gno);

void draw_ref_point(int gno);

void draw_timestamp(void);

void draw_regions(int gno);

#endif /* __PLOTONE_H_ */
