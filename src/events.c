/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 * 
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 * 
 * Copyright (c) 1996-2004 Grace Development Team
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
 * event handler
 *
 */

#include <config.h>

#include <stdlib.h>

#include "utils.h"
#include "core_utils.h"
#include "events.h"

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>
#include <X11/keysym.h>
#include <Xm/ScrollBar.h>

#include "motifinc.h"
#include "protos.h"

extern Widget drawing_window;

/*
 * set format string for locator
 */
static char *typestr[6] = {"X, Y",
                           "DX, DY",
			   "DIST",
			   "Phi, Rho",
			   "VX, VY",
                           "SX, SY"};


int cursortype = 0;

static void scroll_bar_pix(Widget bar, int pix)
{
    int value, slider_size, maxvalue;

    XtVaGetValues(bar,
        XmNvalue,      &value,
        XmNmaximum,    &maxvalue,
        XmNsliderSize, &slider_size,
        NULL);
    value += pix;
    if (value < 0) {
        value = 0;
    } else
    if (value > maxvalue - slider_size) {
        value = maxvalue - slider_size;
    }
    XmScrollBarSetValues(bar, value, 0, 0, 0, True);
}

static void scroll_pix(Widget w, int dx, int dy)
{
    Widget bar;
    
    if (dx && (bar = XtNameToWidget(w, "HorScrollBar"))) {
        scroll_bar_pix(bar, dx);
    }
    if (dy && (bar = XtNameToWidget(w, "VertScrollBar"))) {
        scroll_bar_pix(bar, dy);
    }
}
static void scroll(Widget w, int up, int horiz)
{
    int value, slider_size, increment, page_increment, maxvalue;
    Widget vbar;
    
    if (horiz) {
        vbar = XtNameToWidget(w, "HorScrollBar");
    } else {
        vbar = XtNameToWidget(w, "VertScrollBar");
    }
    
    if (!vbar) {
        return;
    }
    
    XmScrollBarGetValues(vbar, &value, &slider_size,
        &increment, &page_increment);
    if (up) {
        value -= increment;
        if (value < 0) {
            value = 0;
        }
    } else {
        XtVaGetValues(vbar, XmNmaximum, &maxvalue, NULL);
        value += increment;
        if (value > maxvalue - slider_size) {
            value = maxvalue - slider_size;
        }
    }
    XmScrollBarSetValues(vbar, value, 0, 0, 0, True);
}

typedef struct {
    VPoint vp;
    Quark *q;
    int part;
    view bbox;
    int found;
} canvas_target;

static void target_consider(canvas_target *ct, Quark *q, int part,
    const view *v)
{
    if (is_vpoint_inside(v, &ct->vp, 0.0)) {
        ct->q = q;
        ct->part = part;
        ct->bbox = *v;
        ct->found = TRUE;
    }
}

static int target_hook(Quark *q, void *udata, QTraverseClosure *closure)
{
    canvas_target *ct = (canvas_target *) udata;
    view v;
    AText *at;
    DObject *o;
    double *x, *y;
    
    switch (q->fid) {
    case QFlavorFrame:
        if (frame_is_active(q)) {
            legend *l;

            frame_get_view(q, &v);
            target_consider(ct, q, 0, &v);

            if ((l = frame_get_legend(q)) && l->active) {
                target_consider(ct, q, 1, &l->bb);
            }
        }
        break;
    case QFlavorAText:
        at = atext_get_data(q);
        if (at->active) {
            target_consider(ct, q, 0, &at->bb);
        }
        break;
    case QFlavorDObject:
        o = object_get_data(q);
        if (o->active) {
            target_consider(ct, q, 0, &o->bb);
        }
        break;
    case QFlavorSet:
        x = set_get_col(q, DATA_X);
        y = set_get_col(q, DATA_Y);
        if (x && y) {
            int i;
            WPoint wp;
            VPoint vp;
            for (i = 0; i < set_get_length(q); i++) {
                wp.x = x[i];
                wp.y = y[i];
                Wpoint2Vpoint(q, &wp, &vp);
                v.xv1 = v.xv2 = vp.x;
                v.yv1 = v.yv2 = vp.y;
                view_extend(&v, 0.005);
                target_consider(ct, q, i, &v);
            }
        }
        break;
    }
    
    return TRUE;
}

static int find_target(Quark *pr, canvas_target *ct)
{
    ct->found = FALSE;
    quark_traverse(pr, target_hook, ct);

    return ct->found ? RETURN_SUCCESS:RETURN_FAILURE;
}

static void move_target(canvas_target *ct, const VPoint *vp)
{
    VVector vshift;
    
    vshift.x = vp->x - ct->vp.x;
    vshift.y = vp->y - ct->vp.y;
    
    switch (ct->q->fid) {
    case QFlavorFrame:
        switch (ct->part) {
        case 0:
            frame_shift(ct->q, &vshift);
            break;
        case 1:
            frame_legend_shift(ct->q, &vshift);
            break;
        }
        break;
    case QFlavorAText:
        atext_shift(ct->q, &vshift);
        break;
    case QFlavorDObject:
        object_shift(ct->q, &vshift);
        break;
    case QFlavorSet:
        set_point_shift(ct->q, ct->part, &vshift);
        break;
    }
}

void canvas_event_proc(Widget w, XtPointer data, XEvent *event, Boolean *cont)
{
    int x, y;                /* pointer coordinates */
    VPoint vp;
    KeySym keybuf;
    Grace *grace = (Grace *) data;
    Quark *cg = graph_get_current(grace->project);
    
    XMotionEvent *xme;
    XButtonEvent *xbe;
    XKeyEvent    *xke;
    
    static Time lastc_time = 0;  /* time of last mouse click */
    static int lastc_x, lastc_y; /* coords of last mouse click */
    static int last_b1down_x, last_b1down_y;   /* coords of last event */
    int dbl_click;

    int undo_point = FALSE;
    int abort_action = FALSE;
    int collect_points = FALSE;
    int npoints = 0, npoints_requested = 0;
    
    static canvas_target ct;
    
    x = event->xmotion.x;
    y = event->xmotion.y;
    
    switch (event->type) {
    case MotionNotify:
	xme = (XMotionEvent *) event;
	if (cursortype || xme->state & ShiftMask) {
            crosshair_motion(grace->gui, x, y);
        }

	if (xme->state & Button1Mask) {
            if (xme->state & ControlMask) {
                if (ct.found) {
                    slide_region(grace->gui, ct.bbox, x - last_b1down_x, y - last_b1down_y, TRUE);
                }
            } else {
                scroll_pix(drawing_window, last_b1down_x - x, last_b1down_y - y);
            }
        } else {
	    x11_dev2VPoint(x, y, &vp);

            if (grace->gui->focus_policy == FOCUS_FOLLOWS) {
                cg = next_graph_containing(cg, &vp);
            }
            update_locator_lab(cg, &vp);
        }
        
        break;
    case ButtonPress:
        xbe = (XButtonEvent *) event;
	x = event->xbutton.x;
	y = event->xbutton.y;
	x11_dev2VPoint(x, y, &vp);

	switch (event->xbutton.button) {
	case Button1:
            /* first, determine if it's a double click */
            if (xbe->time - lastc_time < CLICK_INT &&
                abs(x - lastc_x) < CLICK_DIST      &&
                abs(y - lastc_y) < CLICK_DIST) {
                dbl_click = TRUE;
            } else {
                dbl_click = FALSE;
            }
            lastc_time = xbe->time;
            lastc_x = x;
            lastc_y = y;

            if (!dbl_click) {
                if (xbe->state & ControlMask) {
                    ct.vp = vp;
                    if (find_target(grace->project, &ct) == RETURN_SUCCESS) {
                        slide_region(grace->gui, ct.bbox, 0, 0, FALSE);
                    }
                } else {
                    if (grace->gui->focus_policy == FOCUS_CLICK) {
                        cg = next_graph_containing(cg, &vp);
                    }
                    update_locator_lab(cg, &vp);
                }
            } else {
                ct.vp = vp;
                if (find_target(grace->project, &ct) == RETURN_SUCCESS) {
                    raise_explorer(grace->gui, ct.q);
                    ct.found = FALSE;
                }
            }
            
            if (collect_points) {
                /* add_point(x, y) */
                npoints++;
            }

            last_b1down_x = x;
            last_b1down_y = y;
            set_cursor(grace->gui, 5);
            
            break;
	case Button2:
            fprintf(stderr, "Button2\n");
            break;
	case Button3:
            if (collect_points) {
                undo_point = TRUE;
                if (npoints) {
                    npoints--;
                }
                if (npoints == 0) {
                    abort_action = TRUE;
                }
            } else {
                ct.vp = vp;
                if (find_target(grace->project, &ct) == RETURN_SUCCESS) {
                    /* TODO: context-sensitive menu */
                    fprintf(stderr, "%s(%d)\n", QIDSTR(ct.q), ct.part);
                    ct.found = FALSE;
                }
            }
            break;
	case Button4:
            scroll(drawing_window, TRUE, xbe->state & ControlMask);
            break;
	case Button5:
            scroll(drawing_window, FALSE, xbe->state & ControlMask);
            break;
	default:
            break;
        }
        break;
    case ButtonRelease:
        xbe = (XButtonEvent *) event;
	switch (event->xbutton.button) {
	case Button1:
            if (xbe->state & ControlMask && ct.found) {
                slide_region(grace->gui, ct.bbox, x - last_b1down_x, y - last_b1down_y, FALSE);

                x11_dev2VPoint(x, y, &vp);

                move_target(&ct, &vp);
                ct.found = FALSE;

                update_explorer(grace->gui->eui, TRUE);
                xdrawgraph(grace->project, TRUE);
            }
            set_cursor(grace->gui, -1);
            break;
        }
        break;
    case KeyPress:
	xke = (XKeyEvent *) event;
        keybuf = XLookupKeysym(xke, 0);
        switch (keybuf) {
        case XK_Escape: /* Esc */
            fprintf(stderr, "Esc\n");
            abort_action = TRUE;
            return;
            break;
        case XK_KP_Add: /* "Grey" plus */
            if (xke->state & ControlMask) {
                page_zoom_inout(grace, +1);
            }
            break;
        case XK_KP_Subtract: /* "Grey" minus */
            if (xke->state & ControlMask) {
                page_zoom_inout(grace, -1);
            }
            break;
        case XK_1:
            if (xke->state & ControlMask) {
                page_zoom_inout(grace, 0);
            }
            break;
        }
        break;
    case KeyRelease:
	xke = (XKeyEvent *) event;
        keybuf = XLookupKeysym(xke, 0);
        if (cursortype == 0 &&
            (keybuf == XK_Shift_L || keybuf == XK_Shift_R)) { /* Shift */
            reset_crosshair(grace->gui, TRUE);
        }
        if (xke->state & ControlMask && ct.found) {
            slide_region(grace->gui, ct.bbox, x - last_b1down_x, y - last_b1down_y, FALSE);
            ct.found = FALSE;
        }
        break;
    default:
	break;
    }
    
    if (abort_action) {
        /* abort action */
        /* xfree(points) */
        npoints = 0;
        /* return RETURN_FAILURE to caller */
    } else
    if (undo_point) {
        /* previous action */
    } else
    if (npoints == npoints_requested) {
        /* return points to caller */
    }
}

static int hook(Quark *q, void *udata, QTraverseClosure *closure)
{
    if (q->fid == QFlavorGraph) {
        Quark *pr = get_parent_project(q);
        VPoint *vp = (VPoint *) udata;
        view v;
        
        closure->descend = FALSE;
        
	if (graph_is_active(q)        == TRUE &&
            graph_get_viewport(q, &v) == RETURN_SUCCESS &&
            is_vpoint_inside(&v, vp, 0.0) == TRUE &&
            graph_get_current(pr) != q) {
            switch_current_graph(q);
            return FALSE;
        }
    } else
    if (q->fid == QFlavorFrame && !frame_is_active(q)) {
        closure->descend = FALSE;
    }

    return TRUE;
}

/*
 * Given the graph quark, find the (non-hidden) graph that contains
 * the VPoint.
 */
Quark *next_graph_containing(Quark *q, VPoint *vp)
{
    Quark *pr = get_parent_project(q);

    quark_traverse(pr, hook, vp);

    return graph_get_current(pr);
}

/*
 * locator on main_panel
 */
void update_locator_lab(Quark *cg, VPoint *vpp)
{
    static VPoint vp = {0.0, 0.0};
    WPoint wp;
    view v;
    double wx, wy, xtmp, ytmp;
    short x, y;
    double dsx = 0.0, dsy = 0.0;
    char buf[256], bufx[64], bufy[64], *s;
    GLocator *locator;
    
    if (vpp != NULL) {
        vp = *vpp;
    }

    if (!graph_is_active(cg)) {
        set_tracker_string("[No graphs]");
        return;
    }
    
    graph_get_viewport(cg, &v);
    if (!is_vpoint_inside(&v, &vp, 0.0)) {
        set_tracker_string("[Out of frame]");
        return;
    }
    
    Vpoint2Wpoint(cg, &vp, &wp);
    wx = wp.x;
    wy = wp.y;
    
    locator = graph_get_locator(cg);
    
    if (locator->pointset) {
	dsx = locator->dsx;
	dsy = locator->dsy;
    }
    
    switch (locator->pt_type) {
    case 0:
        if (graph_get_type(cg) == GRAPH_POLAR) {
            polar2xy(wx, wy, &xtmp, &ytmp);
        } else {
            xtmp = wx;
            ytmp = wy;
        }
        break;
    case 1:
        xtmp = wx - dsx;
        ytmp = wy - dsy;
        break;
    case 2:
        if (graph_get_type(cg) == GRAPH_POLAR) {
            polar2xy(wx, wy, &xtmp, &ytmp);
        } else {
            xtmp = wx;
            ytmp = wy;
        }
        xtmp = hypot(dsx - xtmp, dsy - ytmp);
        ytmp = 0.0;
        break;
    case 3:
        if (dsx - wx != 0.0 || dsy - wy != 0.0) {
            xy2polar(dsx - wx, dsy - wy, &xtmp, &ytmp);
        } else {
            xtmp = 0.0;
            ytmp = 0.0;
        }
        break;
    case 4:
        xtmp = vp.x;
        ytmp = vp.y;
        break;
    case 5:
        x11_VPoint2dev(&vp, &x, &y);
        xtmp = x;
        ytmp = y;
        break;
    default:
        return;
    }
    s = create_fstring(get_parent_project(cg), locator->fx, locator->px, xtmp, LFORMAT_TYPE_PLAIN);
    strcpy(bufx, s);
    s = create_fstring(get_parent_project(cg), locator->fy, locator->py, ytmp, LFORMAT_TYPE_PLAIN);
    strcpy(bufy, s);
    sprintf(buf, "%s: %s = [%s, %s]", QIDSTR(cg), typestr[locator->pt_type], bufx, bufy);

    set_tracker_string(buf);
}

void switch_current_graph(Quark *gr)
{
    
    if (graph_is_active(gr)) {
        Grace *grace = grace_from_quark(gr);
        Quark *cg = graph_get_current(grace->project);
        
        select_graph(gr);
        draw_focus(cg);
        draw_focus(gr);
        update_all();
        graph_set_selectors(gr);
        update_locator_lab(cg, NULL);
    }
}

/*
 * action callback
 */
void set_actioncb(Widget but, void *data)
{
}

/*
 * set the action_flag to the desired action (actions are
 * defined in defines.h), if 0 then cleanup the results
 * from previous actions.
 */
void set_action(CanvasAction act)
{
}

/* -------------------------------------------------------------- */
/* canvas_actions */
void autoscale_action(Widget w, XEvent *e, String *p, Cardinal *c)
{
}

void autoscale_on_near_action(Widget w, XEvent *e, String *p, Cardinal *c)
{
}

void draw_line_action(Widget w, XEvent *e, String *p, Cardinal *c)
{
}

void draw_box_action(Widget w, XEvent *e, String *p, Cardinal *c)
{
}

void draw_ellipse_action(Widget w, XEvent *e, String *p, Cardinal *c)
{
}

void write_string_action(Widget w, XEvent *e, String *p, Cardinal *c)
{
}


void delete_object_action(Widget w, XEvent *e, String *p, Cardinal *c)
{
}

void refresh_hotlink_action(Widget w, XEvent *e, String *p, Cardinal *c)
{
    do_hotupdate_proc(NULL);
}

void set_viewport_action(Widget w, XEvent *e, String *p, Cardinal *c)
{
}

void enable_zoom_action(Widget w, XEvent *e, String *p, Cardinal *c)
{
}

/*
 * update the sets in the current graph
 */
void do_hotupdate_proc(void *data)
{
}

/*
 * switch on the area calculator
 */
void do_select_area(void)
{
}

/*
 * switch on the perimeter calculator
 */
void do_select_peri(void)
{
}

/*
 * define a (polygon) region
 */
void do_select_region(void)
{
}

