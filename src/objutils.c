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
 * operations on objects (strings, lines, and boxes)
 *
 */

#include <config.h>
#include <cmath.h>

#include <stdio.h>
#include <stdlib.h>

#include "globals.h"

#include "graphs.h"
#include "utils.h"
#include "protos.h"

static int maxboxes = 0;
static int maxlines = 0;
static int maxstr = 0;
static int maxellipses = 0;

int number_of_boxes(void) {
    return maxboxes;
}

int number_of_ellipses(void) {
    return maxellipses;
}

int number_of_lines(void) {
    return maxlines;
}

int number_of_strings(void) {
    return maxstr;
}

void move_object(int type, int id, VVector shift)
{
    double xtmp, ytmp;
    boxtype box;
    ellipsetype ellipse;
    linetype line;
    plotstr str;

    switch (type) {
    case OBJECT_BOX:
	if (isactive_box(id)) {
	    get_graph_box(id, &box);
	    if (box.loctype == COORD_VIEW) {
		box.x1 += shift.x;
		box.y1 += shift.y;
		box.x2 += shift.x;
		box.y2 += shift.y;
	    } else {
		world2view(box.x1, box.y1, &xtmp, &ytmp);
		xtmp += shift.x;
		ytmp += shift.y;
                view2world(xtmp, ytmp, &box.x1, &box.y1);
		world2view(box.x2, box.y2, &xtmp, &ytmp);
		xtmp += shift.x;
		ytmp += shift.y;
                view2world(xtmp, ytmp, &box.x2, &box.y2);
	    }
            set_graph_box(id, &box);
            set_dirtystate();
        }
	break;
    case OBJECT_ELLIPSE:
	if (isactive_ellipse(id)) {
	    get_graph_ellipse(id, &ellipse);
	    if (ellipse.loctype == COORD_VIEW) {
		ellipse.x1 += shift.x;
		ellipse.y1 += shift.y;
		ellipse.x2 += shift.x;
		ellipse.y2 += shift.y;
	    } else {
		world2view(ellipse.x1, ellipse.y1, &xtmp, &ytmp);
		xtmp += shift.x;
		ytmp += shift.y;
                view2world(xtmp, ytmp, &ellipse.x1, &ellipse.y1);
		world2view(ellipse.x2, ellipse.y2, &xtmp, &ytmp);
		xtmp += shift.x;
		ytmp += shift.y;
                view2world(xtmp, ytmp, &ellipse.x2, &ellipse.y2);
	    }
            set_graph_ellipse(id, &ellipse);
            set_dirtystate();
        }
	break;
    case OBJECT_LINE:
	if (isactive_line(id)) {
	    get_graph_line(id, &line);
	    if (line.loctype == COORD_VIEW) {
		line.x1 += shift.x;
		line.y1 += shift.y;
		line.x2 += shift.x;
		line.y2 += shift.y;
	    } else {
		world2view(line.x1, line.y1, &xtmp, &ytmp);
		xtmp += shift.x;
		ytmp += shift.y;
                view2world(xtmp, ytmp, &line.x1, &line.y1);
		world2view(line.x2, line.y2, &xtmp, &ytmp);
		xtmp += shift.x;
		ytmp += shift.y;
                view2world(xtmp, ytmp, &line.x2, &line.y2);
	    }
            set_graph_line(id, &line);
            set_dirtystate();
        }
	break;
    case OBJECT_STRING:
	if (isactive_string(id)) {
	    get_graph_string(id, &str);
	    if (str.loctype == COORD_VIEW) {
		str.x += shift.x;
		str.y += shift.y;
	    } else {
		world2view(str.x, str.y, &xtmp, &ytmp);
		xtmp += shift.x;
		ytmp += shift.y;
                view2world(xtmp, ytmp, &str.x, &str.y);
	    }
            set_graph_string(id, &str);
            set_dirtystate();
        }
	break;
    }
}


int isactive_line(int lineno)
{
    if (0 <= lineno && lineno < maxlines)
	return (lines[lineno].active == TRUE);
    return (0);
}

int isactive_box(int boxno)
{
    if (0 <= boxno && boxno < maxboxes)
	return (boxes[boxno].active == TRUE);
    return (0);
}

int isactive_string(int strno)
{
    if (0 <= strno && strno < maxstr)
	return (pstr[strno].s[0]);
    return (0);
}

int isactive_ellipse(int ellipno)
{
    if (0 <= ellipno && ellipno < maxellipses)
	return (ellip[ellipno].active == TRUE);
    return (0);
}

int next_line(void)
{
    int i;

    for (i = 0; i < maxlines; i++) {
	if (!isactive_line(i)) {
	    lines[i].active = TRUE;
	    set_dirtystate();
	    return (i);
	}
    }
    errmsg("Error - no lines available");
    return (-1);
}

int next_box(void)
{
    int i;

    for (i = 0; i < maxboxes; i++) {
	if (!isactive_box(i)) {
	    boxes[i].active = TRUE;
	    set_dirtystate();
	    return (i);
	}
    }
    errmsg("Error - no boxes available");
    return (-1);
}

int next_string(void)
{
    int i;

    for (i = 0; i < maxstr; i++) {
	if (!isactive_string(i)) {
	    set_dirtystate();
	    return (i);
	}
    }
    errmsg("Error - no strings available");
    return (-1);
}

int next_ellipse(void)
{
    int i;

    for (i = 0; i < maxellipses; i++) {
	if (!isactive_ellipse(i)) {
	    ellip[i].active = TRUE;
	    set_dirtystate();
	    return (i);
	}
    }
    errmsg("Error - no ellipses available");
    return (-1);
}

int next_object(int type)
{
    switch (type) {
    case OBJECT_BOX:
        return next_box();
        break;
    case OBJECT_ELLIPSE:
        return next_ellipse();
        break;
    case OBJECT_LINE:
        return next_line();
        break;
    case OBJECT_STRING:
        return next_string();
        break;
    default:
        return -1;
        break;
    }
}

int kill_object(int type, int id)
{
    switch (type) {
    case OBJECT_BOX:
        kill_box(id);
        break;
    case OBJECT_ELLIPSE:
        kill_ellipse(id);
        break;
    case OBJECT_LINE:
        kill_line(id);
        break;
    case OBJECT_STRING:
        kill_string(id);
        break;
    default:
        return GRACE_EXIT_FAILURE;
        break;
    }
    return GRACE_EXIT_SUCCESS;
}

void copy_object(int type, int from, int to)
{
    char *tmpbuf;
    switch (type) {
	case OBJECT_BOX:
	boxes[to] = boxes[from];
	break;
 	case OBJECT_ELLIPSE:
	ellip[to] = ellip[from];
	break;
    case OBJECT_LINE:
	lines[to] = lines[from];
	break;
    case OBJECT_STRING:
	kill_string(to);
	free(pstr[to].s);
	tmpbuf = (char *) malloc((strlen(pstr[from].s) + 1) * sizeof(char));
	pstr[to] = pstr[from];
	pstr[to].s = tmpbuf;
	strcpy(pstr[to].s, pstr[from].s);
	break;
    }
    set_dirtystate();
}

int duplicate_object(int type, int id)
{
    int newid;
    
    if ((newid = next_object(type)) >= 0) {
        copy_object(type, id, newid);
    } else {
        newid = -1;
    }
    
    return newid;
}

plotstr copy_plotstr(plotstr p)
{
    static plotstr pto;
    pto = p;
    if (p.s != NULL) {
	pto.s = (char *) malloc((strlen(p.s) + 1) * sizeof(char));
	if (pto.s != NULL) {
	    strcpy(pto.s, p.s);
	}
    } else {
	pto.s = NULL;
    }
    return pto;
}

void kill_plotstr(plotstr p)
{
    if (p.s != NULL) {
	free(p.s);
    }
}

void kill_box(int boxno)
{
    boxes[boxno].active = FALSE;
    set_dirtystate();
}

void kill_ellipse(int ellipseno)
{
    ellip[ellipseno].active = FALSE;
    set_dirtystate();
}

void kill_line(int lineno)
{
    lines[lineno].active = FALSE;
    set_dirtystate();
}

void kill_string(int stringno)
{
    if (pstr[stringno].s != NULL) {
	free(pstr[stringno].s);
    }
    pstr[stringno].s = (char *) malloc(sizeof(char));
    pstr[stringno].s[0] = 0;
    pstr[stringno].active = FALSE;
    set_dirtystate();
}

int get_object_bb(int type, int id, view *bb)
{
    switch (type) {
    case OBJECT_BOX:
        *bb = boxes[id].bb;
        break;
    case OBJECT_ELLIPSE:
        *bb = ellip[id].bb;
        break;
    case OBJECT_LINE:
        *bb = lines[id].bb;
        break;
    case OBJECT_STRING:
        *bb = pstr[id].bb;
        break;
    default:
        return GRACE_EXIT_FAILURE;
        break;
    }
    return GRACE_EXIT_SUCCESS;
}

void set_plotstr_string(plotstr * pstr, char *buf)
{
    int n;
    
    if (pstr->s != buf) {
        if (pstr->s != NULL) {
          free(pstr->s);
        }
        pstr->s = NULL;
    }
    
    if (buf != NULL) {
	n = strlen(buf);
	pstr->s = (char *) malloc(sizeof(char) * (n + 1));
	strcpy(pstr->s, buf);
	pstr->s[n] = 0;
    } else {
	pstr->s = (char *) malloc(sizeof(char));
	pstr->s[0] = 0;
    }
}

void init_line(int id, VPoint vp1, VPoint vp2)
{
    if (id < 0 || id > number_of_lines()) {
        return;
    }
    lines[id].active = TRUE;
    lines[id].color = line_color;
    lines[id].lines = line_lines;
    lines[id].linew = line_linew;
    lines[id].loctype = line_loctype;
    if (line_loctype == COORD_VIEW) {
        lines[id].x1 = vp1.x;
        lines[id].y1 = vp1.y;
        lines[id].x2 = vp2.x;
        lines[id].y2 = vp2.y;
        lines[id].gno = -1;
    } else {
        lines[id].gno = get_cg();
        view2world(vp1.x, vp1.y, &lines[id].x1, &lines[id].y1);
        view2world(vp2.x, vp2.y, &lines[id].x2, &lines[id].y2);
    }
    lines[id].arrow_end = line_arrow_end;
    set_default_arrow(&lines[id].arrow);
    set_dirtystate();
}

void init_box(int id, VPoint vp1, VPoint vp2)
{
    if (id < 0 || id > number_of_boxes()) {
        return;
    }
    boxes[id].color = box_color;
    boxes[id].fillcolor = box_fillcolor;
    boxes[id].fillpattern = box_fillpat;
    boxes[id].lines = box_lines;
    boxes[id].linew = box_linew;
    boxes[id].loctype = box_loctype;
    boxes[id].active = TRUE;
    if (box_loctype == COORD_VIEW) {
        boxes[id].x1 = vp1.x;
        boxes[id].y1 = vp1.y;
        boxes[id].x2 = vp2.x;
        boxes[id].y2 = vp2.y;
        boxes[id].gno = -1;
    } else {
        boxes[id].gno = get_cg();
        view2world(vp1.x, vp1.y, &boxes[id].x1, &boxes[id].y1);
        view2world(vp2.x, vp2.y, &boxes[id].x2, &boxes[id].y2);
    }
    set_dirtystate();
}

void init_ellipse(int id, VPoint vp1, VPoint vp2)
{
    if (id < 0 || id > number_of_ellipses()) {
        return;
    }
    ellip[id].color = ellipse_color;
    ellip[id].fillcolor = ellipse_fillcolor;
    ellip[id].fillpattern = ellipse_fillpat;
    ellip[id].lines = ellipse_lines;
    ellip[id].linew = ellipse_linew;
    ellip[id].loctype = ellipse_loctype;
    ellip[id].active = TRUE;
    if (ellipse_loctype == COORD_VIEW) {
        ellip[id].x1 = vp1.x;
        ellip[id].y1 = vp1.y;
        ellip[id].x2 = vp2.x;
        ellip[id].y2 = vp2.y;
        ellip[id].gno = -1;
    } else {
        ellip[id].gno = get_cg();
        view2world(vp1.x, vp1.y, &ellip[id].x1, &ellip[id].y1);
        view2world(vp2.x, vp2.y, &ellip[id].x2, &ellip[id].y2);
    }
    set_dirtystate();
}

void init_string(int id, VPoint vp)
{
    if (id < 0 || id > number_of_strings()) {
        return;
    }
    pstr[id].s = copy_string(NULL, "\0");
    pstr[id].font = string_font;
    pstr[id].color = string_color;
    pstr[id].rot = string_rot;
    pstr[id].charsize = string_size;
    pstr[id].loctype = string_loctype;
    pstr[id].just = string_just;
    pstr[id].active = TRUE;
    if (string_loctype == COORD_VIEW) {
        pstr[id].x = vp.x;
        pstr[id].y = vp.y;
        pstr[id].gno = -1;
    } else {
        pstr[id].gno = get_cg();
        view2world(vp.x, vp.y, &pstr[id].x, &pstr[id].y);
    }
    set_dirtystate();
}

void do_clear_lines(void)
{
    int i;

    for (i = 0; i < maxlines; i++) {
	kill_line(i);
    }
}

void do_clear_boxes(void)
{
    int i;

    for (i = 0; i < maxboxes; i++) {
	kill_box(i);
    }
}

void do_clear_ellipses(void)
{
    int i;

    for (i = 0; i < maxboxes; i++) {
	kill_ellipse(i);
    }
}

void do_clear_text(void)
{
    int i;

    for (i = 0; i < maxstr; i++) {
	kill_string(i);
    }
}

void realloc_lines(int n)
{
    int i;
    if (n > maxlines) {
	lines = (linetype *) xrealloc(lines, n * sizeof(linetype));
	for (i = maxlines; i < n; i++) {
	    set_default_line(&lines[i]);
	}
	maxlines = n;
    }
}

void realloc_boxes(int n)
{
    int i;
    if (n > maxboxes) {
	boxes = (boxtype *) xrealloc(boxes, n * sizeof(boxtype));
	for (i = maxboxes; i < n; i++) {
	    set_default_box(&boxes[i]);
	}
	maxboxes = n;
    }
}

void realloc_ellipses(int n)
{
    int i;
    if (n > maxellipses) {
	ellip = (ellipsetype *) xrealloc(ellip, n * sizeof(ellipsetype));
	for (i = maxellipses; i < n; i++) {
	    set_default_ellipse(&ellip[i]);
	}
	maxellipses = n;
    }
}

void realloc_strings(int n)
{
    int i;
    if (n > maxstr) {
	pstr = (plotstr *) xrealloc(pstr, n * sizeof(plotstr));
	for (i = maxstr; i < n; i++) {
	    set_default_string(&pstr[i]);
	}
	maxstr = n;
    }
}


void get_graph_box(int i, boxtype * b)
{
    memcpy(b, &boxes[i], sizeof(boxtype));
}

void get_graph_ellipse(int i, ellipsetype * b)
{
    memcpy(b, &ellip[i], sizeof(ellipsetype));
}

void get_graph_line(int i, linetype * l)
{
    memcpy(l, &lines[i], sizeof(linetype));
}

void get_graph_string(int i, plotstr * s)
{
    memcpy(s, &pstr[i], sizeof(plotstr));
}

void set_graph_box(int i, boxtype * b)
{
    memcpy(&boxes[i], b, sizeof(boxtype));
}

void set_graph_line(int i, linetype * l)
{
    memcpy(&lines[i], l, sizeof(linetype));
}

void set_graph_ellipse(int i, ellipsetype * e)
{
    memcpy(&ellip[i], e, sizeof(ellipsetype));
}

void set_graph_string(int i, plotstr * s)
{
    memcpy(&pstr[i], s, sizeof(plotstr));
}

