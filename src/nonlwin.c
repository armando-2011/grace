/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 * 
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 * 
 * Copyright (c) 1991-1995 Paul J Turner, Portland, OR
 * Copyright (c) 1996-2002 Grace Development Team
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
 * non linear curve fitting
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include <Xm/Xm.h>
#include <Xm/ScrolledW.h>

#include "globals.h"
#include "defines.h"
#include "graphs.h"
#include "utils.h"
#include "files.h"
#include "parser.h"
#include "motifinc.h"
#include "protos.h"

/* nonlprefs.load possible values */
#define LOAD_VALUES         0
#define LOAD_RESIDUALS      1
#define LOAD_FUNCTION       2

#define  WEIGHT_NONE    0
#define  WEIGHT_Y       1
#define  WEIGHT_Y2      2
#define  WEIGHT_DY      3
#define  WEIGHT_CUSTOM  4

/* prefs for non-linear fit */
typedef struct {
    int autoload;       /* do autoload */
    int load;           /* load to... */
    int npoints;        /* # of points to evaluate function at */
    double start;       /* start... */
    double stop;        /* stop ... */
} nonlprefs;

static char buf[256];

static nonlprefs nonl_prefs = {TRUE, LOAD_VALUES, 10, 0.0, 1.0};

static TransformStructure *tdialog;

static TextStructure *nonl_formula_item;
static Widget nonl_title_item;
static SrcDestStructure *nonl_set_item;
static Widget nonl_parm_item[MAXPARM];
static Widget nonl_value_item[MAXPARM];
static Widget nonl_constr_item[MAXPARM];
static Widget nonl_lowb_item[MAXPARM];
static Widget nonl_uppb_item[MAXPARM];
static Widget nonl_tol_item;
static OptionStructure *nonl_nparm_item;
static SpinStructure *nonl_nsteps_item;
static OptionStructure *nonl_load_item;
static Widget nonl_autol_item;
static Widget nonl_npts_item;
static Widget nonl_start_item, nonl_stop_item;
static Widget nonl_fload_rc;
static RestrictionStructure *restr_item;
static OptionStructure *nonl_weigh_item;
static Widget nonl_wfunc_item;

static int do_nonl_proc(void *data);
static void do_nonl_toggle(int onoff, void *data);
static void nonl_wf_cb(int value, void *data);
static void do_constr_toggle(int onoff, void *data);

static void update_nonl_frame_cb(void *data);
static void reset_nonl_frame_cb(void *data);

static void do_nparm_toggle(int value, void *data);
static void create_openfit_popup(void *data);
static void create_savefit_popup(void *data);
static int do_openfit_proc(char *filename, void *data);
static int do_savefit_proc(char *filename, void *data);

static int load_nonl_fit(Quark *pset, Quark *pdest, int force);
static void load_nonl_fit_cb(void *data);


/* ARGSUSED */
void create_nonl_frame(void *data)
{
    set_wait_cursor();
    if (!tdialog) {
        int i;
        OptionItem np_option_items[MAXPARM + 1], option_items[5];
        Widget nonl_frame, menubar, menupane;
        Widget nonl_tab, nonl_main, nonl_advanced;
        Widget sw, title_fr, fr3, rc1, rc2, rc3, lab;

	tdialog = CreateTransformDialogForm(app_shell,
            "Non-linear curve fitting", LIST_TYPE_SINGLE);
        nonl_frame = tdialog->form;

#if 0
        menubar = CreateMenuBar(nonl_frame);
        
        menupane = CreateMenu(menubar, "File", 'F', FALSE);
        CreateMenuButton(menupane, "Open...", 'O', create_openfit_popup, NULL);
        CreateMenuButton(menupane, "Save...", 'S', create_savefit_popup, NULL);
        CreateMenuSeparator(menupane);
        CreateMenuButton(menupane, "Close", 'C', destroy_dialog_cb, GetParent(nonl_frame));

        menupane = CreateMenu(menubar, "Edit", 'E', FALSE);

        CreateMenuButton(menupane, "Reset fit parameters", 'R', reset_nonl_frame_cb, NULL);
        CreateMenuSeparator(menupane);
        CreateMenuButton(menupane, "Load current fit", 'L', load_nonl_fit_cb, NULL);

        menupane = CreateMenu(menubar, "View", 'V', FALSE);
   
        nonl_autol_item = CreateMenuToggle(menupane, "Autoload", 'A',
	    NULL, NULL);
        CreateMenuSeparator(menupane);
        CreateMenuButton(menupane, "Update", 'U', update_nonl_frame_cb, NULL);

        menupane = CreateMenu(menubar, "Help", 'H', TRUE);

        CreateMenuHelpButton(menupane, "On fit", 'f',
            nonl_frame, "doc/UsersGuide.html#non-linear-fit");

        ManageChild(menubar);
        AddDialogFormChild(nonl_frame, menubar);
#endif
        
	title_fr = CreateFrame(nonl_frame, NULL);
	XtVaSetValues(title_fr, XmNshadowType, XmSHADOW_ETCHED_OUT, NULL);
	nonl_title_item = CreateLabel(title_fr, grace->rt->nlfit->title);
        AddDialogFormChild(nonl_frame, title_fr);

        /* ------------ Tabs --------------*/

        nonl_tab = CreateTab(nonl_frame);        


        /* ------------ Main tab --------------*/
        
        nonl_main = CreateTabPage(nonl_tab, "Main");
    	
	nonl_formula_item = CreateTextInput(nonl_main, "Formula:");
	rc1 = CreateHContainer(nonl_main);
	
	for (i = 0; i < MAXPARM + 1; i++) {
	    np_option_items[i].value = i;
            sprintf(buf, "%d", i);
	    np_option_items[i].label = copy_string(NULL, buf);
        }
	nonl_nparm_item = CreateOptionChoice(rc1,
            "Parameters:", 1, MAXPARM + 1, np_option_items);
        AddOptionChoiceCB(nonl_nparm_item, do_nparm_toggle, NULL);
        
	nonl_tol_item = CreateTextItem2(rc1, 8, "Tolerance:");
        
	nonl_nsteps_item = CreateSpinChoice(rc1, "Iterations:", 3,
            SPIN_TYPE_INT, 0.0, 500.0, 5.0);
	SetSpinChoice(nonl_nsteps_item, 5.0);
        
	sw = XtVaCreateManagedWidget("sw",
				     xmScrolledWindowWidgetClass, nonl_main,
				     XmNheight, 180,
				     XmNscrollingPolicy, XmAUTOMATIC,
				     NULL);

	rc2 = CreateVContainer(sw);
	for (i = 0; i < MAXPARM; i++) {
	    nonl_parm_item[i] = CreateHContainer(rc2);
	    sprintf(buf, "A%1d: ", i);
	    nonl_value_item[i] = CreateTextItem2(nonl_parm_item[i], 10, buf);

	    nonl_constr_item[i] = CreateToggleButton(nonl_parm_item[i], "Bounds:");
	    AddToggleButtonCB(nonl_constr_item[i], do_constr_toggle, (void *) i);

	    nonl_lowb_item[i] = CreateTextItem2(nonl_parm_item[i], 6, "");
	    
	    sprintf(buf, "< A%1d < ", i);
	    lab = CreateLabel(nonl_parm_item[i], buf);

	    nonl_uppb_item[i] = CreateTextItem2(nonl_parm_item[i], 6, "");
	}

        /* ------------ Advanced tab --------------*/

        nonl_advanced = CreateTabPage(nonl_tab, "Advanced");

	restr_item =
            CreateRestrictionChoice(nonl_advanced, "Source data filtering");

	fr3 = CreateFrame(nonl_advanced, "Weighting");
        rc3 = CreateHContainer(fr3);
        option_items[0].value = WEIGHT_NONE;
        option_items[0].label = "None";
        option_items[1].value = WEIGHT_Y;
        option_items[1].label = "1/Y";
        option_items[2].value = WEIGHT_Y2;
        option_items[2].label = "1/Y^2";
        option_items[3].value = WEIGHT_DY;
        option_items[3].label = "1/dY^2";
        option_items[4].value = WEIGHT_CUSTOM;
        option_items[4].label = "Custom";
	nonl_weigh_item = CreateOptionChoice(rc3, "Weights", 1, 5, option_items);
	nonl_wfunc_item = CreateTextItem2(rc3, 30, "Function:");
	AddOptionChoiceCB(nonl_weigh_item, nonl_wf_cb, (void *) nonl_wfunc_item);

	fr3 = CreateFrame(nonl_advanced, "Load options");
        rc3 = CreateVContainer(fr3);
        option_items[0].value = LOAD_VALUES;
        option_items[0].label = "Fitted values";
        option_items[1].value = LOAD_RESIDUALS;
        option_items[1].label = "Residuals";
        option_items[2].value = LOAD_FUNCTION;
        option_items[2].label = "Function";
	nonl_load_item = CreateOptionChoice(rc3, "Load", 1, 3, option_items);
	nonl_fload_rc = CreateHContainer(rc3);
	nonl_start_item = CreateTextItem2(nonl_fload_rc, 6, "Start load at:");
	nonl_stop_item = CreateTextItem2(nonl_fload_rc, 6, "Stop load at:");
	nonl_npts_item = CreateTextItem2(nonl_fload_rc, 4, "# of points:");
        AddOptionChoiceCB(nonl_load_item, do_nonl_toggle, (void *) nonl_fload_rc);

	CreateAACDialog(nonl_frame, nonl_tab, do_nonl_proc, NULL);
    }
    update_nonl_frame();
    
    RaiseWindow(GetParent(tdialog->form));
    
    unset_wait_cursor();
}

static void do_nparm_toggle(int value, void *data)
{
    int i;
    for (i = 0; i < MAXPARM; i++) {
        if (i < value) {
            ManageChild(nonl_parm_item[i]);
        } else {
            UnmanageChild(nonl_parm_item[i]);
        }
    }
}

static void reset_nonl_frame_cb(void *data)
{
    reset_nonl(grace->rt->nlfit);
    update_nonl_frame();
}

static void update_nonl_frame_cb(void *data)
{
    update_nonl_frame();
}

void update_nonl_frame(void)
{
    int i;
    
    if (tdialog) {
        XmString str = XmStringCreateLocalized(grace->rt->nlfit->title);
        XtVaSetValues(nonl_title_item, XmNlabelString, str, NULL);
/* 
 * If I define only XmALIGNMENT_CENTER (default!) then it's ignored - bug in Motif???
 */
    	XtVaSetValues(nonl_title_item, XmNalignment, XmALIGNMENT_BEGINNING, NULL);
        XtVaSetValues(nonl_title_item, XmNalignment, XmALIGNMENT_CENTER, NULL);
        XmStringFree(str);
        
        SetTextString(nonl_formula_item, grace->rt->nlfit->formula);
        sprintf(buf, "%g", grace->rt->nlfit->tolerance);
        xv_setstr(nonl_tol_item, buf);
        SetOptionChoice(nonl_nparm_item, grace->rt->nlfit->parnum);
        for (i = 0; i < MAXPARM; i++) {
            nonlparm *nlp = &grace->rt->nlfit->parms[i];
            sprintf(buf, "%g", nlp->value);
            xv_setstr(nonl_value_item[i], buf);
            SetToggleButtonState(nonl_constr_item[i], nlp->constr);
            sprintf(buf, "%g", nlp->min);
            xv_setstr(nonl_lowb_item[i], buf);
            SetSensitive(nonl_lowb_item[i], nlp->constr);
            sprintf(buf, "%g", nlp->max);
            xv_setstr(nonl_uppb_item[i], buf);
            SetSensitive(nonl_uppb_item[i], nlp->constr);
            if (i < grace->rt->nlfit->parnum) {
                if (!XtIsManaged (nonl_parm_item[i])) {
                    ManageChild(nonl_parm_item[i]);
                }
            } else {
                if (XtIsManaged (nonl_parm_item[i])) {
                    UnmanageChild(nonl_parm_item[i]);
                }
            }
        }
        
        SetToggleButtonState(nonl_autol_item, nonl_prefs.autoload);
        SetOptionChoice(nonl_load_item, nonl_prefs.load);
        
        if (nonl_prefs.load == LOAD_FUNCTION) {
            SetSensitive(nonl_fload_rc, True);
        } else {
            SetSensitive(nonl_fload_rc, False);
        }

        if (GetOptionChoice(nonl_weigh_item) == WEIGHT_CUSTOM) {
            SetSensitive(GetParent(nonl_wfunc_item), True);
        } else {
            SetSensitive(GetParent(nonl_wfunc_item), False);
        }
        
        sprintf(buf, "%g", nonl_prefs.start);
        xv_setstr(nonl_start_item, buf);
        sprintf(buf, "%g", nonl_prefs.stop);
        xv_setstr(nonl_stop_item, buf);
        sprintf(buf, "%d", nonl_prefs.npoints);
        xv_setstr(nonl_npts_item, buf);
    }

}

static void nonl_wf_cb(int value, void *data)
{
    Widget rc = GetParent((Widget) data);
    
    if (value == WEIGHT_CUSTOM) {
    	SetSensitive(rc, True);
    } else {
    	SetSensitive(rc, False);
    }
}

static void do_nonl_toggle(int value, void *data)
{
    Widget rc = (Widget) data;
    
    if (value == LOAD_FUNCTION) {
    	SetSensitive(rc, True);
    } else {
    	SetSensitive(rc, False);
    }
}

static void do_constr_toggle(int onoff, void *data)
{
    int value = (int) data;
    if (onoff) {
    	SetSensitive(nonl_lowb_item[value], True);
    	SetSensitive(nonl_uppb_item[value], True);
    	grace->rt->nlfit->parms[value].constr = TRUE;
    } else {
    	SetSensitive(nonl_lowb_item[value], False);
    	SetSensitive(nonl_uppb_item[value], False);
    	grace->rt->nlfit->parms[value].constr = FALSE;
    }
}

/* ARGSUSED */
static int do_nonl_proc(void *data)
{
    int i;
    int nsteps;
    int resno;
    char *fstr;
    int nlen, wlen;
    int weight_method;
    double *ytmp, *warray;
    int restr_type, restr_negate;
    char *rarray;
    int nssrc;
    Quark *psrc, *pdest, **srcsets, **destsets;
    
    if (GetTransformDialogSettings(tdialog, TRUE, &nssrc, &srcsets, &destsets)
        != RETURN_SUCCESS) {
    	return RETURN_FAILURE;
    }
    
    psrc  = srcsets[0];
    pdest = destsets[0];
    
    grace->rt->nlfit->formula = copy_string(grace->rt->nlfit->formula, GetTextString(nonl_formula_item));
    nsteps = (int) GetSpinChoice(nonl_nsteps_item);
    grace->rt->nlfit->tolerance = atof(xv_getstr(nonl_tol_item));
    
    grace->rt->nlfit->parnum = GetOptionChoice(nonl_nparm_item);
    for (i = 0; i < grace->rt->nlfit->parnum; i++) {
	nonlparm *nlp = &grace->rt->nlfit->parms[i];
        strcpy(buf, xv_getstr(nonl_value_item[i]));
	if (sscanf(buf, "%lf", &nlp->value) != 1) {
	    errmsg("Invalid input in parameter field");
	    return RETURN_FAILURE;
	}
	
	nlp->constr = GetToggleButtonState(nonl_constr_item[i]);
	if (nlp->constr) {
	    strcpy(buf, xv_getstr(nonl_lowb_item[i]));
	    if (sscanf(buf, "%lf", &nlp->min) != 1) {
	    	errmsg("Invalid input in low-bound field");
	    	return RETURN_FAILURE;
	    }
	    strcpy(buf, xv_getstr(nonl_uppb_item[i]));
	    if (sscanf(buf, "%lf", &nlp->max) != 1) {
	    	errmsg("Invalid input in upper-bound field");
	    	return RETURN_FAILURE;
	    }
	    if ((nlp->value < nlp->min) || (nlp->value > nlp->max)) {
	    	errmsg("Initial values must be within bounds");
	    	return RETURN_FAILURE;
	    }
	}
    }
    
    if (nsteps) {
        /* apply weigh function */
    	nlen = getsetlength(psrc);
	weight_method = GetOptionChoice(nonl_weigh_item);
        switch (weight_method) {
        case WEIGHT_Y:
        case WEIGHT_Y2:
            ytmp = getcol(psrc, DATA_Y);
            for (i = 0; i < nlen; i++) {
                if (ytmp[i] == 0.0) {
	            errmsg("Divide by zero while calculating weights");
                    return RETURN_FAILURE;
                }
            }
            warray = xmalloc(nlen*SIZEOF_DOUBLE);
            if (warray == NULL) {
	        errmsg("xmalloc failed in do_nonl_proc()");
                return RETURN_FAILURE;
            }
            for (i = 0; i < nlen; i++) {
                if (weight_method == WEIGHT_Y) {
                    warray[i] = 1/ytmp[i];
                } else {
                    warray[i] = 1/(ytmp[i]*ytmp[i]);
                }
            }
            break;
        case WEIGHT_DY:
            ytmp = getcol(psrc, DATA_Y1);
            if (ytmp == NULL) {
	        errmsg("The set doesn't have dY data column");
                return RETURN_FAILURE;
            }
            for (i = 0; i < nlen; i++) {
                if (ytmp[i] == 0.0) {
	            errmsg("Divide by zero while calculating weights");
                    return RETURN_FAILURE;
                }
            }
            warray = xmalloc(nlen*SIZEOF_DOUBLE);
            if (warray == NULL) {
	        errmsg("xmalloc failed in do_nonl_proc()");
            }
            for (i = 0; i < nlen; i++) {
                warray[i] = 1/(ytmp[i]*ytmp[i]);
            }
            break;
        case WEIGHT_CUSTOM:
            if (set_parser_setno(psrc) != RETURN_SUCCESS) {
                errmsg("Bad set");
                return RETURN_FAILURE;
            }
            
            fstr = xv_getstr(nonl_wfunc_item);
            if (v_scanner(fstr, &wlen, &warray) != RETURN_SUCCESS) {
                errmsg("Error evaluating expression for weights");
                return RETURN_FAILURE;
            }
            if (wlen != nlen) {
                errmsg("The array of weights has different length");
                xfree(warray);
                return RETURN_FAILURE;
            }
            break;
        default:
            warray = NULL;
            break;
        }

        /* apply restriction */
        restr_type = GetOptionChoice(restr_item->r_sel);
        restr_negate = GetToggleButtonState(restr_item->negate);
        resno = get_restriction_array(psrc,
            restr_type, restr_negate, &rarray);
	if (resno != RETURN_SUCCESS) {
	    errmsg("Error in restriction evaluation");
	    xfree(warray);
            return RETURN_FAILURE;
	}

        /* The fit itself! */
    	resno = do_nonlfit(psrc, warray, rarray, nsteps);
	xfree(warray);
	xfree(rarray);
    	if (resno != RETURN_SUCCESS) {
	    errmsg("Fatal error in do_nonlfit()");  
	    return RETURN_FAILURE;  	
    	}
   	    	
    	for (i = 0; i < grace->rt->nlfit->parnum; i++) {
	    sprintf(buf, "%g", grace->rt->nlfit->parms[i].value);
	    xv_setstr(nonl_value_item[i], buf);
    	}
    }

/*
 * Select & activate a set to load results to
 */    
    load_nonl_fit(psrc, pdest, FALSE);
    
    return RETURN_SUCCESS;
}

static void load_nonl_fit_cb(void *data)
{
#if 0
    int psrc;
    
    if (GetSingleListChoice(nonl_set_item->src->graph_sel, &src_gno) !=
        RETURN_SUCCESS) {
    	errmsg("No source graph selected");
    	return;
    }
    if (GetSingleListChoice(nonl_set_item->src->set_sel, &src_setno) !=
        RETURN_SUCCESS) {
    	errmsg("No source set selected");
    	return;
    }
    load_nonl_fit(psrc, TRUE);
#endif
}

static int load_nonl_fit(Quark *psrc, Quark *pdest, int force)
{
    int i, npts = 0;
    double delx, *xfit, *y, *yfit;
    
    nonl_prefs.autoload = GetToggleButtonState(nonl_autol_item);
    nonl_prefs.load = GetOptionChoice(nonl_load_item);
    
    if (nonl_prefs.load == LOAD_FUNCTION) {
	if (xv_evalexpr(nonl_start_item, &nonl_prefs.start) != RETURN_SUCCESS) {
	    errmsg("Invalid input in start field");
	    return RETURN_FAILURE;
	}
	if (xv_evalexpr(nonl_stop_item, &nonl_prefs.stop) != RETURN_SUCCESS) {
	    errmsg("Invalid input in start field");
	    return RETURN_FAILURE;
	}
	if (xv_evalexpri(nonl_npts_item, &nonl_prefs.npoints) != RETURN_SUCCESS) {
	    errmsg("Invalid input in start field");
	    return RETURN_FAILURE;
	}
    	if (nonl_prefs.npoints <= 1) {
    	    errmsg("Number of points must be > 1");
	    return RETURN_FAILURE;
    	}
    }
    
    if (force || nonl_prefs.autoload) {
    	switch (nonl_prefs.load) {
    	case LOAD_VALUES:
    	case LOAD_RESIDUALS:
    	    npts = getsetlength(psrc);
    	    setlength(pdest, npts);
    	    copycol2(psrc, pdest, DATA_X);
    	    break;
    	case LOAD_FUNCTION:
    	    npts  = nonl_prefs.npoints;
 
    	    setlength(pdest, npts);
 
    	    delx = (nonl_prefs.stop - nonl_prefs.start)/(npts - 1);
    	    xfit = getx(pdest);
	    for (i = 0; i < npts; i++) {
	        xfit[i] = nonl_prefs.start + i * delx;
	    }
    	    break;
    	}
    	
    	setcomment(pdest, grace->rt->nlfit->formula);
    	
    	do_compute(pdest, pdest, NULL, grace->rt->nlfit->formula);
    	
    	if (nonl_prefs.load == LOAD_RESIDUALS) { /* load residuals */
    	    y = gety(psrc);
    	    yfit = gety(pdest);
    	    for (i = 0; i < npts; i++) {
	        yfit[i] -= y[i];
	    }
    	}
    	
    	update_set_lists(pdest->parent);
        SelectStorageChoices(nonl_set_item->dest->set_sel, 1, (void **) &pdest);
    	xdrawgraph();
    }
    
    return RETURN_SUCCESS;
}


static void create_openfit_popup(void *data)
{
    static FSBStructure *fsb = NULL;

    set_wait_cursor();

    if (fsb == NULL) {
        fsb = CreateFileSelectionBox(app_shell, "Open fit parameter file");
	AddFileSelectionBoxCB(fsb, do_openfit_proc, NULL);
        ManageChild(fsb->FSB);
    }
    
    RaiseWindow(fsb->dialog);

    unset_wait_cursor();
}

static int do_openfit_proc(char *filename, void *data)
{
    reset_nonl(grace->rt->nlfit);
    getparms(filename);
    update_nonl_frame();
    
    return FALSE;
}


static void create_savefit_popup(void *data)
{
    static FSBStructure *fsb = NULL;
    static Widget title_item = NULL;

    set_wait_cursor();

    if (fsb == NULL) {
        Widget fr;
        
        fsb = CreateFileSelectionBox(app_shell, "Save fit parameter file");
	fr = CreateFrame(fsb->rc, NULL);
	title_item = CreateTextItem2(fr, 25, "Title: ");
	AddFileSelectionBoxCB(fsb, do_savefit_proc, (void *) title_item);
        ManageChild(fsb->FSB);
    }
    
    xv_setstr(title_item, grace->rt->nlfit->title);
    
    RaiseWindow(fsb->dialog);

    unset_wait_cursor();
}

static int do_savefit_proc(char *filename, void *data)
{
    FILE *pp;
    Widget title_item = (Widget) data;
    
    pp = grace_openw(filename);
    if (pp != NULL) {
        grace->rt->nlfit->title = copy_string(grace->rt->nlfit->title, xv_getstr(title_item));
        errwin("Not implemented yet");
        /* FIXME */;
        grace_close(pp);
    }
    return TRUE;
}
