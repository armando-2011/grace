/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 * 
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 * 
 * Copyright (c) 1996-99 Grace Development Team
 * Copyright (c) 1991-95 Paul J Turner, Portland, OR
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
 * spreadsheet-like editing of data points
 *
 */

#include <config.h>
#include <cmath.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "globals.h"
#include "graphs.h"
#include "utils.h"
#include "files.h"
#include "plotone.h"
#include "protos.h"

#include <Xm/Xm.h>
#include <Xm/DialogS.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/Protocols.h>
#include <Xbae/Matrix.h>
#include "motifinc.h"


typedef struct _EditPoints {
    struct _EditPoints *next;
    int gno;
    int setno;
    int ncols;
    int nrows;
    Widget top;
    Widget mw;
    int cformat[MAX_SET_COLS];
    int cprec[MAX_SET_COLS];
} EditPoints;

void update_cells(EditPoints *ep);
void do_update_cells(Widget w, XtPointer client_data, XtPointer call_data);

/* default cell value precision */
#define CELL_PREC 8

/* default cell value format (0 - Decimal; 1 - General; 2 - Exponential) */
#define CELL_FORMAT 1

/* default cell width */
#define CELL_WIDTH 12

char *scformat[3] =
{"%.*lf", "%.*lg", "%.*le"};

/*
 * delete the selected row
 */
void del_point_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    int i,j;
    EditPoints *ep = (EditPoints *) client_data;

    XbaeMatrixGetCurrentCell(ep->mw, &i, &j);
    if(i >= ep->nrows || j >= ep->ncols) {
        errwin("Selected cell out of range");
        return;
    }
    del_point(ep->gno, ep->setno, i);
    update_set_lists(ep->gno);
    if(is_set_active(ep->gno, ep->setno)) {
        update_cells(ep);
    }
}


/*
 * add a point to a set by copying the selected cell and placing it after it
 */
void add_pt_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    int i,j, k;
    Datapoint dpoint;
    EditPoints *ep = (EditPoints *)client_data;
    int gno = ep->gno, setno = ep->setno;

    XbaeMatrixGetCurrentCell(ep->mw, &i, &j);
    if ( i>= ep->nrows || j >= ep->ncols || i < 0 || j < 0){
        errmsg("Selected cell out of range");
        return;
    }
    zero_datapoint(&dpoint);
    for (k = 0; k < ep->ncols; k++) {
        dpoint.ex[k] = *(getcol(gno, setno, k) + i);
    }
    add_point_at(gno, setno, i + 1, &dpoint);
    if(is_set_active(gno, setno)) {
        update_cells(ep);
    }
    update_set_lists(gno);
}

static Widget *editp_col_item;
static Widget *editp_format_item;
static Widget *editp_precision_item;

static void update_props(EditPoints *ep)
{
    int col;

    col = GetChoice(editp_col_item);
    if (col >= MAX_SET_COLS) {
    	col = 0;
    }

    SetChoice(editp_format_item, ep->cformat[col]); 

    SetChoice(editp_precision_item, ep->cprec[col]);
}

static void do_accept_props(Widget w, XtPointer client_data, XtPointer call_data)
{
    int i, col, cformat, cprec;
    EditPoints *ep = (EditPoints *) client_data;

    col = GetChoice(editp_col_item);
    cformat = GetChoice(editp_format_item);
    cprec = GetChoice(editp_precision_item);
    
    if (col < MAX_SET_COLS) {
        ep->cformat[col] = cformat;
        ep->cprec[col] = cprec;
    } else {	    /* do it for all columns */
    	for (i = 0; i < MAX_SET_COLS; i++) {
    	    ep->cformat[i] = cformat;
    	    ep->cprec[i] = cprec;
        }
    } 	
}

void do_update_cells(Widget w, XtPointer client_data, XtPointer call_data)
{
    update_cells((EditPoints *) client_data);
}

/*
 * redo frame since number of data points or set type, etc.,  may change 
 */
void update_cells(EditPoints *ep)
{
    int i, nr, nc;
    short widths[MAX_SET_COLS] =
       {CELL_WIDTH, CELL_WIDTH, CELL_WIDTH, CELL_WIDTH, CELL_WIDTH, CELL_WIDTH};
    short width;
    char buf[32];
    char **rowlabels;
	
    ep->nrows = getsetlength(ep->gno, ep->setno);
    ep->ncols = dataset_cols(ep->gno, ep->setno);
    /* get current size of widget and update rows/columns as needed */
    XtVaGetValues(ep->mw,
        XmNcolumns, &nc,
        XmNrows, &nr,
        NULL);
    if (ep->nrows > nr) {
        XbaeMatrixAddRows(ep->mw, 0, NULL, NULL, NULL, ep->nrows - nr);
    } else if (ep->nrows < nr) {
        XbaeMatrixDeleteRows(ep->mw, 0, nr - ep->nrows);
    }
    if (ep->ncols > nc) {
        XbaeMatrixAddColumns(ep->mw, 0, NULL, NULL, widths, NULL, 
            NULL, NULL, NULL, ep->ncols-nc);
    } else if (ep->ncols < nc) {
        XbaeMatrixDeleteColumns(ep->mw, 0, nc - ep->ncols);
    }
		
    rowlabels = xmalloc(ep->nrows*sizeof(char *));
    for (i = 0; i < ep->nrows; i++) {
    	sprintf(buf, "%d", i);
    	rowlabels[i] = xmalloc((sizeof(buf) + 1)*SIZEOF_CHAR);
    	strcpy(rowlabels[i], buf);
    }
    width = (short) ceil(log10(i)) + 2;	/* increase row label width by 1 */

    XtVaSetValues(ep->mw,
        XmNrowLabels, rowlabels,
	XmNrowLabelWidth, width,
	NULL);

    /* free memory used to hold strings */
    for (i = 0; i < ep->nrows; i++) {
	xfree(rowlabels[i]);
    }
    xfree(rowlabels);
}

void do_props_proc(Widget w, XtPointer client_data, XtPointer call_data)
{
    static Widget top, dialog;
    EditPoints *ep;
    static Widget but1[2];

    set_wait_cursor();
    ep = (EditPoints *) client_data;
    if (top == NULL) {
        char *label1[2];
        label1[0] = "Accept";
        label1[1] = "Close";
	top = XmCreateDialogShell(app_shell, "Edit set props", NULL, 0);
	handle_close(top);
	dialog = XmCreateRowColumn(top, "dialog_rc", NULL, 0);

	editp_col_item = CreatePanelChoice(dialog, "Apply to column:",
				    8, "1", "2", "3", "4", "5", "6", "All",
					   NULL, 0);

	editp_format_item = CreatePanelChoice(dialog, "Format:",
					      4,
					      "Decimal",
					      "General",
					      "Exponential",
					      NULL, 0);

	editp_precision_item = CreatePanelChoice(dialog, "Precision:",
						 16,
						 "0", "1", "2", "3", "4",
						 "5", "6", "7", "8", "9",
						 "10", "11", "12", "13", "14",
						 NULL, 0);

	CreateSeparator(dialog);

	CreateCommandButtons(dialog, 2, but1, label1);
	XtAddCallback(but1[1], XmNactivateCallback,
	    	(XtCallbackProc) destroy_dialog, (XtPointer) top);
	XtManageChild(dialog);
    }
    /* TODO: remove the dirty stuff */
    XtRemoveAllCallbacks(but1[0], XmNactivateCallback);
    XtAddCallback(but1[0], XmNactivateCallback,
    	    (XtCallbackProc) do_accept_props, (XtPointer) ep);
    update_props(ep);
    XtRaise(top);
    unset_wait_cursor();
}


static void leaveCB(Widget w, XtPointer client_data, XtPointer calld)
{
    double *datap;
    char buf[128];
    EditPoints *ep = (EditPoints *) client_data;
    XbaeMatrixLeaveCellCallbackStruct *cs =
    	    (XbaeMatrixLeaveCellCallbackStruct *) calld;

    datap = getcol(ep->gno, ep->setno, cs->column);
    sprintf(buf, scformat[(ep->cformat[cs->column])], ep->cprec[cs->column],
    	    datap[cs->row]);
    if (strcmp(buf, cs->value) != 0) {
        /* TODO: add edit_point() function to setutils.c */
	datap[cs->row] = atof(cs->value);

        set_dirtystate();
        update_set_lists(ep->gno);
        drawgraph();
    }
}


static void drawcellCB(Widget w, XtPointer client_data, XtPointer calld)
{
    int i, j;
    double *datap;
    EditPoints *ep = (EditPoints *) client_data;
    static char buf[128];
    XbaeMatrixDrawCellCallbackStruct *cs =
    	    (XbaeMatrixDrawCellCallbackStruct *) calld;

    i = cs->row;
    j = cs->column;
    datap = getcol(ep->gno, ep->setno, j);
    sprintf(buf, scformat[(ep->cformat[j])], ep->cprec[j], datap[i]);
    cs->type = XbaeString;
    cs->string = XtNewString(buf);
}

static void selectCB(Widget w, XtPointer client_data, XtPointer call_data)
{
    XbaeMatrixSelectCellCallbackStruct *sc =
        (XbaeMatrixSelectCellCallbackStruct *) call_data;

    XbaeMatrixSelectCell(w, sc->row, sc->column);
}

static void writeCB(Widget w, XtPointer client_data, XtPointer call_data)
{
}


static EditPoints *ep_start = NULL;

void delete_ep(EditPoints *ep)
{
    EditPoints *ep_tmp = ep_start;
    
    if (ep == NULL) {
        return;
    }
    
    if (ep == ep_start) {
        ep_start = ep_start->next;
        XCFREE(ep);
        return;
    }
    
    while (ep_tmp != NULL) {
        if (ep_tmp->next == ep) {
            ep_tmp->next = ep->next;
            XCFREE(ep);
            return;
        }
        ep_tmp = ep_tmp->next;
    }
}

EditPoints *get_ep(int gno, int setno)
{
    EditPoints *ep_tmp = ep_start;

    while (ep_tmp != NULL) {
        if (ep_tmp->gno == gno && ep_tmp->setno == setno) {
            break;
        }
        ep_tmp = ep_tmp->next;
    }
    return ep_tmp;
}

void destroy_ep(Widget w, XtPointer client_data, XtPointer call_data)
{
    EditPoints *ep;
    
    ep = (EditPoints *) client_data;
    deletewidget(ep->top);
    delete_ep(ep);
}

void create_ss_frame(int gno, int setno)
{
    int i;
    char *collabels[MAX_SET_COLS];
    short cwidths[MAX_SET_COLS];
    unsigned char column_label_alignments[MAX_SET_COLS];
    char wname[256];
    char *label1[3] = {"Props...", "Update", "Close"};
    char *label2[2] = {"Delete", "Add"};
    EditPoints *ep;
    Atom WM_DELETE_WINDOW;
    Widget dialog;
    Widget but1[3], but2[2];
    
    ep = get_ep(gno, setno);
    if (ep != NULL) {
        XtRaise(ep->top);
        return;
    }
    
    set_wait_cursor();

    ep = xmalloc(sizeof(EditPoints));
    ep->next = ep_start;
    ep_start = ep;
    
    ep->gno = gno;
    ep->setno = setno;
    ep->ncols = dataset_cols(gno, setno);
    ep->nrows = getsetlength(gno, setno);
    for (i = 0; i < MAX_SET_COLS; i++) {
        collabels[i] = copy_string(NULL, dataset_colname(i));
        cwidths[i] = CELL_WIDTH;
        column_label_alignments[i] = XmALIGNMENT_CENTER;
        ep->cprec[i] = CELL_PREC;
        ep->cformat[i] = CELL_FORMAT;
    }

    sprintf(wname, "Edit set: S%d of G%d", ep->setno, ep->gno);
    ep->top = XmCreateDialogShell(app_shell, wname, NULL, 0);
    XtVaSetValues(ep->top, XmNdeleteResponse, XmDO_NOTHING, NULL);
    WM_DELETE_WINDOW = XmInternAtom(XtDisplay(app_shell),
        "WM_DELETE_WINDOW", False);
    XmAddProtocolCallback(ep->top,
        XM_WM_PROTOCOL_ATOM(ep->top), WM_DELETE_WINDOW,
        destroy_ep, (XtPointer) ep);

    dialog = XmCreateRowColumn(ep->top, "dialog_rc", NULL, 0);

    ep->mw = XtVaCreateManagedWidget("mw",
        xbaeMatrixWidgetClass, dialog,
        XmNrows, ep->nrows,
        XmNcolumns, ep->ncols,
        XmNvisibleRows, 10,
        XmNvisibleColumns, 2,
        XmNcolumnWidths, cwidths,
        XmNcolumnLabels, collabels,
        XmNcolumnLabelAlignments, column_label_alignments,
        XmNallowColumnResize, True,
        XmNgridType, XmGRID_CELL_SHADOW,
        XmNcellShadowType, XmSHADOW_ETCHED_OUT,
        XmNcellShadowThickness, 2,
        XmNaltRowCount, 0,
        NULL);

    update_cells(ep);

    XtAddCallback(ep->mw, XmNselectCellCallback, selectCB, ep);	
    XtAddCallback(ep->mw, XmNdrawCellCallback, drawcellCB, ep);	
    XtAddCallback(ep->mw, XmNleaveCellCallback, leaveCB, ep);
    XtAddCallback(ep->mw, XmNwriteCellCallback, writeCB, ep);  

    CreateSeparator(dialog);
    
    CreateCommandButtons(dialog, 2, but2, label2);
    XtAddCallback(but2[0], XmNactivateCallback, (XtCallbackProc) del_point_cb,
    	    (XtPointer) ep);
    XtAddCallback(but2[1], XmNactivateCallback, (XtCallbackProc) add_pt_cb,
    	    (XtPointer) ep);
    
    CreateSeparator(dialog);

    CreateCommandButtons(dialog, 3, but1, label1);
    XtAddCallback(but1[0], XmNactivateCallback, (XtCallbackProc) do_props_proc,
    	    (XtPointer) ep);
    XtAddCallback(but1[1], XmNactivateCallback, (XtCallbackProc) do_update_cells,
    	    (XtPointer) ep);
    XtAddCallback(but1[2], XmNactivateCallback, (XtCallbackProc) destroy_ep,
    	    (XtPointer) ep);

    XtManageChild(dialog);
    XtRaise(ep->top);
    unset_wait_cursor();
}

/*
 * Start up external editor
 */
void do_ext_editor(int gno, int setno)
{
    char *fname, ebuf[256];
    FILE *cp;
    int save_autos;

    fname = tmpnam(NULL);
    cp = grace_openw(fname);
    if (cp == NULL) {
        return;
    }

    write_set(gno, setno, cp, sformat, FALSE);
    grace_close(cp);

    sprintf(ebuf, "%s %s", get_editor(), fname);
    system_wrap(ebuf);

    /* temporarily disable autoscale */
    save_autos = autoscale_onread;
    autoscale_onread = AUTOSCALE_NONE;
    if (is_set_active(gno, setno)) {
        curtype = dataset_type(gno, setno);
	killsetdata(gno, setno);	
    }
    getdata(gno, fname, SOURCE_DISK, LOAD_SINGLE);
    autoscale_onread = save_autos;
    unlink(fname);
    update_all();
    drawgraph();
}
