/*
 * Grace - Graphics for Exploratory Data Analysis
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
 * Canvas events
 *
 */

#ifndef __EVENTS_H_
#define __EVENTS_H_

#include <config.h>

#include "grace.h"

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>

/* for canvas event proc */
typedef enum {
    DO_NOTHING,
    ZOOM_1ST,
    ZOOM_2ND,
    VIEW_1ST,
    VIEW_2ND,
    STR_LOC,
    PLACE_LEGEND_1ST,
    PLACE_LEGEND_2ND,
    DEL_POINT,
    MOVE_POINT1ST,
    MOVE_POINT2ND,
    ADD_POINT,
    DEL_OBJECT,
    MOVE_OBJECT_1ST,
    MOVE_OBJECT_2ND,
    MAKE_BOX_1ST,
    MAKE_BOX_2ND,
    MAKE_LINE_1ST,
    MAKE_LINE_2ND,
    MAKE_ELLIP_1ST,
    MAKE_ELLIP_2ND,
    SEL_POINT,
    COMP_AREA,
    COMP_PERIMETER,
    TRACKER,
    DEF_REGION,
    DEF_REGION1ST,
    DEF_REGION2ND,
    EDIT_OBJECT,
    COPY_OBJECT1ST,
    COPY_OBJECT2ND,
    AUTO_NEAREST,
    ZOOMX_1ST,
    ZOOMX_2ND,
    ZOOMY_1ST,
    ZOOMY_2ND,
    DISLINE1ST,
    DISLINE2ND
} CanvasAction;

/* add points at */
#define ADD_POINT_BEGINNING 0
#define ADD_POINT_END       1
#define ADD_POINT_NEAREST   2

/* move points */
#define MOVE_POINT_XY   0
#define MOVE_POINT_X    1
#define MOVE_POINT_Y    2

/*
 * double click detection interval (ms)
 */
#define CLICK_INT 400
#define CLICK_DIST 10

#define MAXPICKDIST 0.015      /* the maximum distance away from an object */
                              /* you may be when picking it (in viewport  */
                              /* coordinates)                             */  

void canvas_event_proc(Widget w, XtPointer data, XEvent *event, Boolean *cont);
void anchor_point(int curx, int cury, VPoint curvp);
void set_actioncb(Widget but, void *data);
void set_action(CanvasAction act);
void track_point(Quark *pset, int *loc, int shift);
void update_locator_lab(Quark *cg, VPoint *vpp);
void set_stack_message(void);
void do_select_area(void);
void do_select_peri(void);
void do_dist_proc(void);
void do_select_region(void);
Quark *next_graph_containing(Quark *cg, VPoint *vp);
int graph_clicked(Quark *gr, VPoint vp);
int focus_clicked(Quark *cg, VPoint vp, VPoint *avp);
int legend_clicked(Quark *gr, VPoint vp, view *bb);
int axis_clicked(Quark *gr, VPoint vp, int *axisno);
int title_clicked(Quark *gr, VPoint vp);
int find_insert_location(Quark *pset, VPoint vp);
int find_point(Quark *gr, VPoint vp, Quark **pset, int *loc);
void newworld(Quark *gr, int axes, VPoint vp1, VPoint vp2);
void push_and_zoom(void);

/* action routines */
void set_viewport_action( Widget, XEvent *, String *, Cardinal * );
void enable_zoom_action( Widget, XEvent *, String *, Cardinal * );
void autoscale_action( Widget, XEvent *, String *, Cardinal * );
void draw_line_action( Widget, XEvent *, String *, Cardinal * );
void draw_box_action( Widget, XEvent *, String *, Cardinal * );
void draw_ellipse_action( Widget, XEvent *, String *, Cardinal * );
void write_string_action( Widget, XEvent *, String *, Cardinal * );
void refresh_hotlink_action( Widget, XEvent *, String *, Cardinal * );

void update_point_locator(Quark *pset, int loc);
void get_tracking_props(int *setno, int *move_dir, int *add_at);

#endif /* __EVENTS_H_ */
