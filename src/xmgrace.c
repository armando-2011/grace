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
 * Main Motif interface
 *
 */

#include <config.h>

#include <unistd.h>

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/keysym.h>
#include <X11/StringDefs.h>

#ifdef WITH_EDITRES
#  include <X11/Xmu/Editres.h>
#endif

#include <Xm/Xm.h>
#include <Xm/Protocols.h>
#include <Xm/MainW.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/Label.h>
#include <Xm/ScrolledW.h>
#include <Xm/DrawingA.h>
#include <Xm/RepType.h>

#if defined(HAVE_XPM_H)
#  include <xpm.h>
#else
#  if defined (HAVE_X11_XPM_H)
#    include <X11/xpm.h>
#  endif
#endif

#include "globals.h"
#include "bitmaps.h"
#include "utils.h"
#include "files.h"
#include "device.h"
#include "x11drv.h"
#include "graphs.h"
#include "graphutils.h"
#include "plotone.h"
#include "events.h"
#include "protos.h"

#include "motifinc.h"


/* used globally */
XtAppContext app_con;
Widget app_shell;

static Widget canvas;

static Widget drawing_window;		/* container for drawing area */


Widget loclab;			/* locator label */
Widget statlab;			/* status line at the bottom */
Widget stack_depth_item;	/* stack depth item on the main panel */
Widget curw_item;		/* current world stack item on the main panel */


Display *disp = NULL;
Window xwin;
extern Window root;
extern GC gc;
extern int screennumber;
extern int depth;
extern Colormap cmap;
extern unsigned long xvlibcolors[];

/* used locally */
static Widget main_frame;
static Widget menu_bar;
static Widget frleft, frtop, frbot;	/* dialogs along canvas edge */
static Widget form;		/* form for mainwindow */

static void MenuCB(void *data);
static Widget CreateMainMenuBar(Widget parent);

static int toolbar_visible = 1;
static int statusbar_visible = 1;
static int locbar_visible = 1;

static Widget windowbarw[3];

static void graph_scroll_proc(void *data);
static void graph_zoom_proc(void *data);
static void world_stack_proc(void *data);
static void load_example(void *data);

static void wm_exit_cb(Widget w, XtPointer client_data, XtPointer call_data);

#define WSTACK_PUSH         0
#define WSTACK_POP          1
#define WSTACK_CYCLE        2
#define WSTACK_PUSH_ZOOM    3

/*
 * establish action routines
 */
static XtActionsRec canvas_actions[] = {
	{ "autoscale", (XtActionProc) autoscale_action },	
	{ "autoscale_on_near", (XtActionProc) autoscale_on_near_action },	
	{ "draw_line", (XtActionProc) draw_line_action },	
	{ "draw_box", (XtActionProc) draw_box_action },	
	{ "draw_ellipse", (XtActionProc) draw_ellipse_action },	
	{ "write_string", (XtActionProc) write_string_action },	
	{ "delete_object", (XtActionProc) delete_object_action },	
	{ "place_legend", (XtActionProc) place_legend_action },	
	{ "place_timestamp", (XtActionProc) place_timestamp_action },	
	{ "move_object", (XtActionProc) move_object_action },	
	{ "refresh_hotlink", (XtActionProc) refresh_hotlink_action },
	{ "set_viewport", (XtActionProc) set_viewport_action },	
	{ "enable_zoom", (XtActionProc) enable_zoom_action }
};

static XtActionsRec list_select_actions[] = {
    {"list_selectall_action",   list_selectall_action},
    {"list_unselectall_action", list_unselectall_action},
    {"list_invertselection_action", list_invertselection_action}
};

static XtActionsRec cstext_actions[] = {
    {"cstext_edit_action", cstext_edit_action}
};

static char canvas_table[] = "#override\n\
	Ctrl Alt <Key>l: draw_line()\n\
	Ctrl Alt <Key>b: draw_box()\n\
	Ctrl Alt <Key>e: draw_ellipse()\n\
	Ctrl Alt <Key>t: write_string()\n\
	Ctrl <Key>a: autoscale()\n\
	Ctrl <Key>d: delete_object()\n\
	Ctrl <Key>l: place_legend()\n\
	Ctrl <Key>m: move_object()\n\
	Ctrl <Key>t: place_timestamp()\n\
	Ctrl <Key>u: refresh_hotlink()\n\
	Ctrl <Key>v: set_viewport()\n\
	Ctrl <Key>z: enable_zoom()";

/*
 * establish resource stuff
 */
typedef struct {
    Boolean invert;
    Boolean allow_dc;
    Boolean auto_redraw;
    Boolean logwindow;
    Boolean toolbar;
    Boolean statusbar;
    Boolean locatorbar;
}
ApplicationData, *ApplicationDataPtr;

static XtResource resources[] =
{
    {"invertDraw", "InvertDraw", XtRBoolean, sizeof(Boolean),
     XtOffset(ApplicationDataPtr, invert), XtRImmediate,
     (XtPointer) TRUE},
    {"allowDoubleClick", "AllowDoubleClick", XtRBoolean, sizeof(Boolean),
     XtOffset(ApplicationDataPtr, allow_dc), XtRImmediate,
     (XtPointer) TRUE},
    {"allowRedraw", "AllowRedraw", XtRBoolean, sizeof(Boolean),
     XtOffset(ApplicationDataPtr, auto_redraw), XtRImmediate,
     (XtPointer) TRUE},
    {"logWindow", "LogWindow", XtRBoolean, sizeof(Boolean),
     XtOffset(ApplicationDataPtr, logwindow), XtRImmediate,
     (XtPointer) FALSE},
    {"toolBar", "ToolBar", XtRBoolean, sizeof(Boolean),
     XtOffset(ApplicationDataPtr, toolbar), XtRImmediate,
     (XtPointer) TRUE},
    {"statusBar", "StatusBar", XtRBoolean, sizeof(Boolean),
     XtOffset(ApplicationDataPtr, statusbar), XtRImmediate,
     (XtPointer) TRUE},
    {"locatorBar", "LocatorBar", XtRBoolean, sizeof(Boolean),
     XtOffset(ApplicationDataPtr, locatorbar), XtRImmediate,
     (XtPointer) TRUE}
};

String fallbackResources[] = {
    "XMgrace*fontList:-adobe-helvetica-medium-r-normal-*-12-*-*-*-*-*-*-*",
    "XMgrace*tabFontList:-adobe-helvetica-medium-r-normal-*-12-*-*-*-*-*-*-*",
    "XMgrace*background: #e5e5e5",
    "XMgrace*foreground: #000000",
    "XMgrace*XbaeMatrix.oddRowBackground: #cccccc",
    "XMgrace*XbaeMatrix.evenRowBackground: #cfe7e7",
    "XMgrace*fontTable.evenRowBackground: #e5e5e5",
    "XMgrace*XmPushButton.background: #b0c4de",
    "XMgrace*XmMenuShell*XmPushButton.background: #e5e5e5",
    "XMgrace*XmText*background: #cfe7e7",
    "XMgrace*XmToggleButton.selectColor: #ff0000",
    "XMgrace*XmToggleButton.fillOnSelect: true",
    "XMgrace*XmSeparator.margin: 0",
    "*menuBar*tearOffModel: XmTEAR_OFF_ENABLED",
    "*dragInitiatorProtocolStyle: XmDRAG_NONE",
    "*dragReceiverProtocolStyle:  XmDRAG_NONE",
    "*fileMenu.newButton.acceleratorText: Ctrl+N",
    "*fileMenu.newButton.accelerator: Ctrl<Key>n",
    "*fileMenu.openButton.acceleratorText: Ctrl+O",
    "*fileMenu.openButton.accelerator: Ctrl<Key>o",
    "*fileMenu.saveButton.acceleratorText: Ctrl+S",
    "*fileMenu.saveButton.accelerator: Ctrl<Key>s",
    "*fileMenu.exitButton.acceleratorText: Ctrl+Q",
    "*fileMenu.exitButton.accelerator: Ctrl<Key>q",
    "*fileMenu.printButton.acceleratorText: Ctrl+P",
    "*fileMenu.printButton.accelerator: Ctrl<Key>p",
    "*helpMenu.onContextButton.acceleratorText: Shift+F1",
    "*helpMenu.onContextButton.accelerator: Shift<Key>F1",
    NULL
};

/*
 * main menubar
 */
/* #define MENU_HELP	200 */
#define MENU_EXIT	201
#define MENU_NEW	203
#define MENU_OPEN	204
#define MENU_SAVE	205
#define MENU_SAVEAS	206
#define MENU_REVERT	207
#define MENU_PRINT	208


int initialize_gui(int *argc, char **argv)
{
    ApplicationData rd;

    XtToolkitInitialize();
    app_con = XtCreateApplicationContext();
    XtAppSetFallbackResources(app_con, fallbackResources);
    disp = XtOpenDisplay(app_con, NULL, NULL, "XMgrace", NULL, 0, argc, argv);
    if (disp == NULL) {
        return GRACE_EXIT_FAILURE;
    }

    app_shell = XtAppCreateShell(NULL, "XMgrace", applicationShellWidgetClass,
        disp, NULL, 0);
    
    XtGetApplicationResources(app_shell, &rd,
        resources, XtNumber(resources), NULL, 0);
    
    invert = rd.invert;
    allow_dc = rd.allow_dc;
    logwindow = rd.logwindow;
    auto_redraw = rd.auto_redraw;
    toolbar_visible = rd.toolbar;
    statusbar_visible = rd.statusbar;
    locbar_visible = rd.locatorbar;

    XtAppAddActions(app_con, canvas_actions, XtNumber(canvas_actions));
    XtAppAddActions(app_con, list_select_actions, XtNumber(list_select_actions));
    XtAppAddActions(app_con, cstext_actions, XtNumber(cstext_actions));

    return GRACE_EXIT_SUCCESS;
}

static void do_drawgraph(void *data)
{
    drawgraph();
}


static void MenuCB(void *data)
{
    char *s;
    
    switch ((int) data) {
    case MENU_EXIT:
	bailout();
	break;
    case MENU_NEW:
	new_project(NULL);

	update_all();
        xdrawgraph();
	break;
    case MENU_OPEN:
	create_openproject_popup();
	break;
    case MENU_SAVE:
	if (strcmp (docname, NONAME) != 0) {
	    set_wait_cursor();
	    
	    save_project(docname);
	    
	    unset_wait_cursor();
	} else {
	    create_saveproject_popup();
	}
	break;
    case MENU_SAVEAS:
	create_saveproject_popup();
	break;
    case MENU_REVERT:
	set_wait_cursor();
	s = copy_string(NULL, docname);
	if (strcmp (s, NONAME) != 0) {
            load_project(s);
        } else {
	    new_project(NULL);
        }
        free(s);
	update_all();
        xdrawgraph();
	unset_wait_cursor();
	break;
    case MENU_PRINT:
	set_wait_cursor();
	do_hardcopy();
	unset_wait_cursor();
	break;
    default:
	break;
    }
}

/*
 * service the autoscale buttons on the main panel
 */
static void autoscale_proc(void *data)
{
    int cg = get_cg();
    
    if (activeset(cg)) {
	autoscale_graph(cg, (int) data);
	update_ticks(cg);
        drawgraph();
    } else {
	errwin("No active sets!");
    }
}

void autoon_proc(void *data)
{
    set_action(0);
    set_action(AUTO_NEAREST);
}

/*
 * service the autoticks button on the main panel
 */
void autoticks_proc(void *data)
{
    autotick_axis(get_cg(), ALL_AXES);
    update_ticks(get_cg());
    drawgraph();
}

/*
 * set the message in the left footer
 */
void set_left_footer(char *s)
{
    if (s == NULL) {
        char hbuf[64];
        char buf[GR_MAXPATHLEN + 100];
        gethostname(hbuf, 63);
        sprintf(buf, "%s, %s, %s", hbuf, display_name(), docname);
        SetLabel(statlab, buf);
    } else {
        SetLabel(statlab, s);
    }
    XmUpdateDisplay(statlab);
}

/*
 * for world stack
 */
void set_stack_message(void)
{
    char buf[16];
    if (stack_depth_item) {
	sprintf(buf, " SD:%1d ", graph_world_stack_size(get_cg()));
	SetLabel(stack_depth_item, buf);
        sprintf(buf, " CW:%1d ", get_world_stack_current(get_cg()));
	SetLabel(curw_item, buf);
    }
}


/*
 * clear the locator reference point
 */
void do_clear_point(void *data)
{
    GLocator locator;
    
    get_graph_locator(get_cg(), &locator);
    locator.pointset = FALSE;
    set_graph_locator(get_cg(), &locator);
    xdrawgraph();
}

/*
 * set visibility of the toolbars
 */
static void set_view_items(void)
{
    if (statusbar_visible) {
	SetToggleButtonState(windowbarw[1], TRUE);
	XtManageChild(frbot);
	XtVaSetValues(drawing_window,
		      XmNbottomAttachment, XmATTACH_WIDGET,
		      XmNbottomWidget, frbot,
		      NULL);
	if (toolbar_visible) {
	    XtVaSetValues(frleft,
			  XmNbottomAttachment, XmATTACH_WIDGET,
			  XmNbottomWidget, frbot,
			  NULL);
	}
    } else {
	SetToggleButtonState(windowbarw[1], FALSE);
	XtVaSetValues(drawing_window,
		      XmNbottomAttachment, XmATTACH_FORM,
		      NULL);
	XtUnmanageChild(frbot);
	if (toolbar_visible) {
	    XtVaSetValues(frleft,
			  XmNbottomAttachment, XmATTACH_FORM,
			  NULL);
	}
    }
    if (toolbar_visible) {
	SetToggleButtonState(windowbarw[2], TRUE);
	XtManageChild(frleft);
	if (statusbar_visible) {
	    XtVaSetValues(frleft,
			  XmNbottomAttachment, XmATTACH_WIDGET,
			  XmNbottomWidget, frbot,
			  NULL);
	}
	if (locbar_visible) {
	    XtVaSetValues(frleft,
			  XmNtopAttachment, XmATTACH_WIDGET,
			  XmNtopWidget, frtop,
			  NULL);
	}
	XtVaSetValues(drawing_window,
		      XmNleftAttachment, XmATTACH_WIDGET,
		      XmNleftWidget, frleft,
		      NULL);
    } else {
	SetToggleButtonState(windowbarw[2], FALSE);
	XtUnmanageChild(frleft);
	XtVaSetValues(drawing_window,
		      XmNleftAttachment, XmATTACH_FORM,
		      NULL);
    }
    if (locbar_visible) {
	SetToggleButtonState(windowbarw[0], TRUE);
	XtManageChild(frtop);
	XtVaSetValues(drawing_window,
		      XmNtopAttachment, XmATTACH_WIDGET,
		      XmNtopWidget, frtop,
		      NULL);
	if (toolbar_visible) {
	    XtVaSetValues(frleft,
			  XmNtopAttachment, XmATTACH_WIDGET,
			  XmNtopWidget, frtop,
			  NULL);
	}
    } else {
	SetToggleButtonState(windowbarw[0], FALSE);
	XtUnmanageChild(frtop);
	XtVaSetValues(drawing_window,
		      XmNtopAttachment, XmATTACH_FORM,
		      NULL);
	if (toolbar_visible) {
	    XtVaSetValues(frleft,
			  XmNtopAttachment, XmATTACH_FORM,
			  NULL);
	}
    }
}

/*
 * service routines for the View pulldown
 */
void set_statusbar(int onoff, void *data)
{
    if (onoff) {
	statusbar_visible = 1;
    } else {
	statusbar_visible = 0;
    }
    set_view_items();
}

void set_toolbar(int onoff, void *data)
{
    if (onoff) {
	toolbar_visible = 1;
    } else {
	toolbar_visible = 0;
    }
    set_view_items();
}

void set_locbar(int onoff, void *data)
{
    if (onoff) {
	locbar_visible = 1;
    } else {
	locbar_visible = 0;
    }
    set_view_items();
}

/*
 * create the main menubar
 */
static Widget CreateMainMenuBar(Widget parent)
{
    Widget menubar;
    Widget menupane, submenupane, sub2menupane;

    menubar = CreateMenuBar(parent);

/*
 * File menu
 */
    menupane = CreateMenu(menubar, "File", 'F', FALSE);

    CreateMenuButton(menupane, "New", 'N', MenuCB, (void *) MENU_NEW);
    CreateMenuButton(menupane, "Open...", 'O', MenuCB, (void *) MENU_OPEN);
    CreateMenuButton(menupane, "Save", 'S', MenuCB, (void *) MENU_SAVE);
    CreateMenuButton(menupane, "Save as...", 'a', MenuCB, (void *) MENU_SAVEAS);
    CreateMenuButton(menupane, "Revert to saved", 'v', MenuCB, (void *) MENU_REVERT);
    CreateMenuButton(menupane, "Describe...", 'D', create_describe_popup, NULL);

    CreateMenuSeparator(menupane);

/*
 * Read submenu
 */

    submenupane = CreateMenu(menupane, "Read", 'R', FALSE);

    CreateMenuButton(submenupane, "Sets...", 'S', create_file_popup, NULL);
#ifdef HAVE_NETCDF
    CreateMenuButton(submenupane, "NetCDF...", 'N', create_netcdfs_popup, NULL);
#endif
    CreateMenuButton(submenupane, "Parameters...", 'P', create_rparams_popup, NULL);
   
/*
 * Write submenu
 */  
    submenupane = CreateMenu(menupane, "Write", 'W', FALSE);

    CreateMenuButton(submenupane, "Sets...", 'S', create_write_popup, NULL);
    CreateMenuButton(submenupane, "Parameters...", 'P', create_wparam_frame, NULL);

    CreateMenuSeparator(menupane);
    CreateMenuButton(menupane, "Print", 'P', MenuCB, (void *) MENU_PRINT);
    CreateMenuButton(menupane, "Device setup...", 't', create_printer_setup, NULL);
    CreateMenuSeparator(menupane);
    CreateMenuButton(menupane, "Exit", 'x', MenuCB, (void *) MENU_EXIT);

/*
 * Edit menu
 */
    menupane = CreateMenu(menubar, "Edit", 'E', FALSE);

    CreateMenuButton(menupane, "Data sets...", 'D', create_datasetprop_popup, NULL);
    CreateMenuButton(menupane, "Set operations...", 'o', create_setop_popup, NULL);
    CreateMenuSeparator(menupane);
    CreateMenuButton(menupane, "Arrange graphs...", 'r', create_arrange_frame, NULL);
    CreateMenuButton(menupane, "Overlay graphs...", 'O', create_overlay_frame, NULL);
    CreateMenuButton(menupane, "Autoscale graphs...", 'A', create_autos_frame, NULL);
    CreateMenuSeparator(menupane);

    submenupane = CreateMenu(menupane, "Regions", 'i', FALSE);
    CreateMenuButton(submenupane, "Status...", 'S', define_status_popup, NULL);
    CreateMenuButton(submenupane, "Define...", 'D', create_define_frame, NULL);
    CreateMenuButton(submenupane, "Clear...", 'C', create_clear_frame, NULL);
    CreateMenuSeparator(submenupane);
    CreateMenuButton(submenupane, "Report on...", 'R', create_reporton_frame, NULL);


    CreateMenuButton(menupane, "Hot links...", 'l', create_hotlinks_popup, NULL);

    CreateMenuSeparator(menupane);

    CreateMenuButton(menupane, "Set locator fixed point", 'f', set_actioncb, (void *) SEL_POINT);
    CreateMenuButton(menupane, "Clear locator fixed point", 'C', do_clear_point, NULL);
    CreateMenuButton(menupane, "Locator props...", 'p', create_locator_frame, NULL);
    
    CreateMenuSeparator(menupane);

    CreateMenuButton(menupane, "Preferences...", 'r', create_props_frame, NULL);

/*
 * Data menu
 */
    menupane = CreateMenu(menubar, "Data", 'D', FALSE);

    CreateMenuButton(menupane, "Data set operations...", 'o', create_datasetop_popup, NULL);

    submenupane = CreateMenu(menupane, "Transformations", 'T', FALSE);

    CreateMenuButton(submenupane, "Evaluate expression...", 'E', create_eval_frame, NULL);
    CreateMenuSeparator(submenupane);
    CreateMenuButton(submenupane, "Histograms...", 'H', create_histo_frame, NULL);
    CreateMenuButton(submenupane, "Fourier transforms...", 'u', create_fourier_frame, NULL);
    CreateMenuSeparator(submenupane);
    CreateMenuButton(submenupane, "Running averages...", 'a', create_run_frame, NULL);
    CreateMenuButton(submenupane, "Differences...", 'D', create_diff_frame, NULL);
    CreateMenuButton(submenupane, "Seasonal differences...", 'o', create_seasonal_frame, NULL);
    CreateMenuButton(submenupane, "Integration...", 'I', create_int_frame, NULL);
    CreateMenuSeparator(submenupane);
    CreateMenuButton(submenupane, "Interpolation...", 't', create_interp_frame, NULL);
    CreateMenuButton(submenupane, "Splines...", 'S', create_spline_frame, NULL);
    CreateMenuButton(submenupane, "Regression...", 'R', create_reg_frame, NULL);
    CreateMenuButton(submenupane, "Non-linear curve fitting...", 'N', create_nonl_frame, NULL);
    CreateMenuSeparator(submenupane);
    CreateMenuButton(submenupane, "Cross/auto correlation...", 'C', create_xcor_frame, NULL);
    CreateMenuButton(submenupane, "Digital filter...", 'f', create_digf_frame, NULL);
    CreateMenuButton(submenupane, "Linear convolution...", 'v', create_lconv_frame, NULL);
    CreateMenuSeparator(submenupane);
    CreateMenuButton(submenupane, "Geometric transforms...", 'G', create_geom_frame, NULL);
    CreateMenuSeparator(submenupane);
    CreateMenuButton(submenupane, "Sample points...", 'm', create_samp_frame, NULL);
    CreateMenuButton(submenupane, "Prune data...", 'P', create_prune_frame, NULL);

    CreateMenuButton(menupane, "Feature extraction...", 'x', create_featext_frame, NULL);

/* Plot menu */
    menupane = CreateMenu(menubar, "Plot", 'P', FALSE);

    CreateMenuButton(menupane, "Plot appearance...", 'p', create_plot_frame_cb, NULL);
    CreateMenuButton(menupane, "Graph appearance...", 'G', create_graphapp_frame_cb, (void *) -1);
    CreateMenuButton(menupane, "Set appearance...", 'S', define_symbols_popup, (void *) -1);
    CreateMenuButton(menupane, "Axis properties...", 'x', create_axes_dialog_cb, NULL);


/* View menu */
    menupane = CreateMenu(menubar, "View", 'V', FALSE);
   
    windowbarw[0] = CreateMenuToggle(menupane, "Show locator bar", 'L', set_locbar, NULL);
    windowbarw[1] = CreateMenuToggle(menupane, "Show status bar", 'S', set_statusbar, NULL);
    windowbarw[2] = CreateMenuToggle(menupane, "Show tool bar", 'T', set_toolbar, NULL);

    CreateMenuSeparator(menupane);

    CreateMenuButton(menupane, "Redraw", 'R', do_drawgraph, NULL);

    CreateMenuSeparator(menupane);

    CreateMenuButton(menupane, "Update all", 'U', update_all_cb, NULL);

/* Window menu */
    menupane = CreateMenu(menubar, "Window", 'W', FALSE);
   
    CreateMenuButton(menupane, "Commands...", 'C', open_command, NULL);
    CreateMenuButton(menupane, "Point tracking", 'P', create_points_frame, NULL);
    CreateMenuButton(menupane, "Drawing objects", 'o', define_objects_popup, NULL);
    CreateMenuButton(menupane, "Font tool", 'F', create_fonttool_cb, NULL);

/*
 *     CreateMenuButton(menupane, "Area/perimeter...", 'A', create_area_frame, NULL);
 */

    CreateMenuButton(menupane, "Results...", 'R', create_monitor_frame, NULL);
    

/* help menu */

    menupane = CreateMenu(menubar, "Help", 'H', TRUE);

    CreateMenuButton(menupane, "User's Guide", 'G', HelpCB, (void *) "UsersGuide.html");
    CreateMenuButton(menupane, "Tutorial", 'T', HelpCB, (void *) "Tutorial.html");
    CreateMenuButton(menupane, "FAQ", 'Q', HelpCB, (void *) "FAQ.html");
    CreateMenuButton(menupane, "Changes", 'C', HelpCB, (void *) "CHANGES.html");

    CreateMenuSeparator(menupane);
 
    submenupane = CreateMenu(menupane, "Examples", 'E', FALSE);
    sub2menupane = CreateMenu(submenupane, "General intro", 'i', FALSE);
    CreateMenuButton(sub2menupane, "Explain", '\0', load_example, "explain.agr");
    CreateMenuButton(sub2menupane, "Properties", '\0', load_example, "props.agr");
    CreateMenuButton(sub2menupane, "Axes", '\0',load_example, "axes.agr");
    CreateMenuButton(sub2menupane, "Fonts", '\0', load_example, "tfonts.agr");
    CreateMenuButton(sub2menupane, "Arrows", '\0', load_example, "arrows.agr");
    CreateMenuButton(sub2menupane, "More symbols", '\0', load_example, "moresyms.agr");
    CreateMenuButton(sub2menupane, "Symbols and lines", '\0', load_example, "symslines.agr");
    CreateMenuButton(sub2menupane, "Fills", '\0', load_example, "fills.agr");
    CreateMenuButton(sub2menupane, "World stack", '\0', load_example, "tstack.agr");
    CreateMenuButton(sub2menupane, "Inset graphs", '\0', load_example, "tinset.agr");
    CreateMenuButton(sub2menupane, "Many graphs", '\0', load_example, "manygraphs.agr");

    sub2menupane = CreateMenu(submenupane, "XY graphs", 'g', FALSE);
    CreateMenuButton(sub2menupane, "Log scale", '\0', load_example, "tlog.agr");
    CreateMenuButton(sub2menupane, "Log2 scale", '\0', load_example, "log2.agr");
    CreateMenuButton(sub2menupane, "Error bars", '\0', load_example, "terr.agr");
    CreateMenuButton(sub2menupane, "More error bars", '\0', load_example, "terr2.agr");
    CreateMenuButton(sub2menupane, "Date/time axis formats", '\0', load_example, "times.agr");
    CreateMenuButton(sub2menupane, "Australia map", '\0', load_example, "au.agr");
    CreateMenuButton(sub2menupane, "A CO2 analysis", '\0', load_example, "co2.agr");

    sub2menupane = CreateMenu(submenupane, "XY charts", 'c', FALSE);
    CreateMenuButton(sub2menupane, "Bar chart", '\0', load_example, "bar.agr");
    CreateMenuButton(sub2menupane, "Stacked bar", '\0', load_example, "stackedb.agr");
    CreateMenuButton(sub2menupane, "Bar chart with error bars", '\0', load_example, "chartebar.agr");
    CreateMenuButton(sub2menupane, "Different charts", '\0', load_example, "charts.agr");

    sub2menupane = CreateMenu(submenupane, "Polar graphs", 'c', FALSE);
    CreateMenuButton(sub2menupane, "Polar graph", '\0', load_example, "polar.agr");

    sub2menupane = CreateMenu(submenupane, "Special set presentations", 'S', FALSE);
    CreateMenuButton(sub2menupane, "HILO", '\0', load_example, "hilo.agr");
    CreateMenuButton(sub2menupane, "XY Radius", '\0', load_example, "txyr.agr");
    CreateMenuButton(sub2menupane, "XYZ", '\0', load_example, "xyz.agr");

    sub2menupane = CreateMenu(submenupane, "Type setting", 'T', FALSE);
    CreateMenuButton(sub2menupane, "Simple", '\0', load_example, "test2.agr");
    CreateMenuButton(sub2menupane, "Advanced", '\0', load_example, "typeset.agr");

    sub2menupane = CreateMenu(submenupane, "Calculus", 'u', FALSE);
    CreateMenuButton(sub2menupane, "Non-linear fit", '\0', load_example, "logistic.agr");
 
    CreateMenuSeparator(menupane);

    CreateMenuButton(menupane, "Comments", 'm', HelpCB, (void *) "http://plasma-gate.weizmann.ac.il/Grace/comments.html");
    CreateMenuSeparator(menupane);
    CreateMenuButton(menupane, "License terms", 'L', HelpCB, (void *) "GPL.html");
    CreateMenuButton(menupane, "About...", 'A', create_about_grtool, NULL);

    return (menubar);
}


/*
 * build the GUI
 */
void startup_gui(void)
{
    Widget bt, rc3, rcleft;
    Pixmap icon, shape;
    Atom WM_DELETE_WINDOW;

/* 
 * Allow users to change tear off menus with X resources
 */
    XmRepTypeInstallTearOffModelConverter();

#ifdef WITH_EDITRES    
    XtAddEventHandler(app_shell, (EventMask) 0, True,
    			_XEditResCheckMessages, NULL);
#endif

/*
 *     XtAddEventHandler(app_shell, StructureNotifyMask, False,
 *         		     (XtEventHandler) resize, NULL);
 */

    savewidget(app_shell);
    
    xlibinit();
    XtVaSetValues(app_shell, XmNcolormap, cmap, NULL);
    

/*
 * We handle important WM events ourselves
 */
    WM_DELETE_WINDOW = XmInternAtom(disp, "WM_DELETE_WINDOW", False);
    XmAddWMProtocolCallback(app_shell, WM_DELETE_WINDOW, wm_exit_cb, NULL);
    XtVaSetValues(app_shell, XmNdeleteResponse, XmDO_NOTHING, NULL);
    
/*
 * build the UI here
 */
    main_frame = XtVaCreateManagedWidget("main", xmMainWindowWidgetClass, app_shell,
					 XmNshadowThickness, 0,
					 XmNwidth, 680,
					 XmNheight, 700,
					 NULL);

    menu_bar = CreateMainMenuBar(main_frame);
    XtManageChild(menu_bar);

    form = XmCreateForm(main_frame, "form", NULL, 0);

    frleft = CreateFrame(form, NULL);
    rcleft = XtVaCreateManagedWidget("toolBar", xmRowColumnWidgetClass, frleft,
				     XmNorientation, XmVERTICAL,
				     XmNpacking, XmPACK_TIGHT,
				     XmNspacing, 0,
				     XmNentryBorder, 0,
				     XmNmarginWidth, 0,
				     XmNmarginHeight, 0,
				     NULL);

    frtop = CreateFrame(form, NULL);
    frbot = CreateFrame(form, NULL);

    statlab = XtVaCreateManagedWidget("statlab", xmLabelWidgetClass, frbot,
				      XmNalignment, XmALIGNMENT_BEGINNING,
				      XmNrecomputeSize, True,
				      NULL);

    loclab = XtVaCreateManagedWidget("label Locate", xmLabelWidgetClass, frtop,
				     XmNalignment, XmALIGNMENT_BEGINNING,
				     XmNrecomputeSize, True,
				     NULL);

    if (get_pagelayout() == PAGE_FIXED) {

        drawing_window = XtVaCreateManagedWidget("drawing_window",
				     xmScrolledWindowWidgetClass, form,
				     XmNnavigationType, XmEXCLUSIVE_TAB_GROUP,
				     XmNscrollingPolicy, XmAUTOMATIC,
				     XmNvisualPolicy, XmVARIABLE,
				     NULL);
        canvas = XtVaCreateManagedWidget("canvas",
                                     xmDrawingAreaWidgetClass, drawing_window,
				     XmNwidth, (Dimension) DEFAULT_PAGE_WIDTH,
				     XmNheight, (Dimension) DEFAULT_PAGE_HEIGHT,
				     XmNresizePolicy, XmRESIZE_ANY,
                                     XmNbackground,
				     xvlibcolors[0],
				     NULL);
    } else {
        canvas = XtVaCreateManagedWidget("canvas",
                                     xmDrawingAreaWidgetClass, form,
				     XmNwidth, (Dimension) DEFAULT_PAGE_WIDTH,
				     XmNheight, (Dimension) DEFAULT_PAGE_HEIGHT,
				     XmNresizePolicy, XmRESIZE_ANY,
				     XmNnavigationType, XmEXCLUSIVE_TAB_GROUP,
                                     XmNbackground,
				     xvlibcolors[0],
				     NULL);
        drawing_window = canvas;
    }
    
    XtAddCallback(canvas, XmNexposeCallback,
                            (XtCallbackProc) expose_resize, NULL);
    XtAddCallback(canvas, XmNresizeCallback,
                            (XtCallbackProc) expose_resize, NULL);

    XtAddEventHandler(canvas, EnterWindowMask
		      | LeaveWindowMask
		      | ButtonPressMask
		      | PointerMotionMask
		      | KeyPressMask
		      | ColormapChangeMask,
		      False,
		      (XtEventHandler) my_proc, NULL);
		      
    XtOverrideTranslations(canvas, XtParseTranslationTable(canvas_table));
		      

    XtVaSetValues(frleft,
		  XmNtopAttachment, XmATTACH_WIDGET,
		  XmNtopWidget, frtop,
		  XmNbottomAttachment, XmATTACH_WIDGET,
		  XmNbottomWidget, frbot,
		  XmNleftAttachment, XmATTACH_FORM,
		  NULL);
    XtVaSetValues(frtop,
		  XmNtopAttachment, XmATTACH_FORM,
		  XmNleftAttachment, XmATTACH_FORM,
		  XmNrightAttachment, XmATTACH_FORM,
		  NULL);
    XtVaSetValues(drawing_window,
		  XmNtopAttachment, XmATTACH_WIDGET,
		  XmNtopWidget, frtop,
		  XmNbottomAttachment, XmATTACH_WIDGET,
		  XmNbottomWidget, frbot,
		  XmNrightAttachment, XmATTACH_FORM,
		  XmNleftAttachment, XmATTACH_WIDGET,
		  XmNleftWidget, frleft,
		  NULL);
    XtVaSetValues(frbot,
		  XmNbottomAttachment, XmATTACH_FORM,
		  XmNrightAttachment, XmATTACH_FORM,
		  XmNleftAttachment, XmATTACH_FORM,
		  NULL);

    XtManageChild(form);

    XmMainWindowSetAreas(main_frame, menu_bar, NULL, NULL, NULL, form);

    bt = CreateButton(rcleft, "Draw");
    AddButtonCB(bt, do_drawgraph, NULL);

/* zoom and autoscale */
    rc3 = XtVaCreateManagedWidget("rc", xmRowColumnWidgetClass, rcleft,
				  XmNorientation, XmHORIZONTAL,
				  XmNpacking, XmPACK_COLUMN,
				  XmNnumColumns, 4,
				  XmNspacing, 0,
				  XmNentryBorder, 0,
				  NULL);
    bt = CreateBitmapButton(rc3, 16, 16, zoom_bits);
    AddButtonCB(bt, set_actioncb, (void *) ZOOM_1ST);
    bt = CreateBitmapButton(rc3, 16, 16, auto_bits);
    AddButtonCB(bt, autoscale_proc, (void *) AUTOSCALE_XY);

/* expand/shrink */
    bt = CreateBitmapButton(rc3, 16, 16, expand_bits);
    AddButtonCB(bt, graph_zoom_proc, (void *) GZOOM_EXPAND);
    bt = CreateBitmapButton(rc3, 16, 16, shrink_bits);
    AddButtonCB(bt, graph_zoom_proc, (void *) GZOOM_SHRINK);

/*
 * scrolling buttons
 */
    bt = CreateBitmapButton(rc3, 16, 16, left_bits);
    AddButtonCB(bt, graph_scroll_proc, (void *) GSCROLL_LEFT);
    bt = CreateBitmapButton(rc3, 16, 16, right_bits);
    AddButtonCB(bt, graph_scroll_proc, (void *) GSCROLL_RIGHT);

    bt = CreateBitmapButton(rc3, 16, 16, down_bits);
    AddButtonCB(bt, graph_scroll_proc, (void *) GSCROLL_DOWN);
    bt = CreateBitmapButton(rc3, 16, 16, up_bits);
    AddButtonCB(bt, graph_scroll_proc, (void *) GSCROLL_UP);

    CreateSeparator(rcleft);

    bt = CreateButton(rcleft, "AutoT");
    AddButtonCB(bt, autoticks_proc, NULL);

    bt = CreateButton(rcleft, "AutoO");
    AddButtonCB(bt, autoon_proc, NULL);

    rc3 = XtVaCreateManagedWidget("rc", xmRowColumnWidgetClass, rcleft,
				  XmNorientation, XmHORIZONTAL,
				  XmNpacking, XmPACK_COLUMN,
				  XmNnumColumns, 4,
				  XmNspacing, 0,
				  XmNentryBorder, 0,
				  NULL);
    bt = CreateButton(rc3, "ZX");
    AddButtonCB(bt, set_actioncb, (void *) ZOOMX_1ST);
    bt = CreateButton(rc3, "ZY");
    AddButtonCB(bt, set_actioncb, (void *) ZOOMY_1ST);

    bt = CreateButton(rc3, "AX");
    AddButtonCB(bt, autoscale_proc, (void *) AUTOSCALE_X);
    bt = CreateButton(rc3, "AY");
    AddButtonCB(bt, autoscale_proc, (void *) AUTOSCALE_Y);

    bt = CreateButton(rc3, "PZ");
    AddButtonCB(bt, world_stack_proc, (void *) WSTACK_PUSH_ZOOM);
    bt = CreateButton(rc3, "Pu");
    AddButtonCB(bt, world_stack_proc, (void *) WSTACK_PUSH);

    bt = CreateButton(rc3, "Po");
    AddButtonCB(bt, world_stack_proc, (void *) WSTACK_POP);
    bt = CreateButton(rc3, "Cy");
    AddButtonCB(bt, world_stack_proc, (void *) WSTACK_CYCLE);

    stack_depth_item = XtVaCreateManagedWidget("stackdepth",
                                               xmLabelWidgetClass, rcleft,
					       NULL);

    curw_item = XtVaCreateManagedWidget("curworld", xmLabelWidgetClass, rcleft,
					NULL);

    bt = CreateButton(rcleft, "Exit");
    AddButtonCB(bt, MenuCB, (void *) MENU_EXIT);

/*
 * initialize cursors
 */
    init_cursors();

/*
 * initialize some option menus
 */
    init_option_menus();

/*
 * initialize the tool bars
 */
    set_view_items();

    SetLabel(loclab, "G0:[X, Y] = ");
    set_stack_message();
    set_left_footer(NULL);

/*
 * set icon
 */
#if defined(HAVE_XPM)
    XpmCreatePixmapFromData(disp, root,
			    grace_icon_xpm, &icon, &shape, NULL);
#else
    icon = XCreateBitmapFromData(disp,
        RootWindowOfScreen(XtScreen(app_shell)), (char *)grace_icon_bits,
        grace_icon_width, grace_icon_height);
    shape = XCreateBitmapFromData(disp,
        RootWindowOfScreen(XtScreen(app_shell)), (char *)grace_mask_bits,
        grace_icon_width, grace_icon_height);
#endif
    XtVaSetValues(app_shell, XtNiconPixmap, icon, XtNiconMask, shape, NULL);

    XtRealizeWidget(app_shell);
    xwin = XtWindow(canvas);
    inwin = 1;
    
/*
 * set the title
 */
    update_app_title();

/*
 * If logging is on, initialize
 */
    log_results("Startup");

    XtAppMainLoop(app_con);
}

static void wm_exit_cb(Widget w, XtPointer client_data, XtPointer call_data)
{
    bailout();
}

void get_canvas_size(Dimension *w, Dimension *h)
{
    XtVaGetValues(canvas,
                  XmNwidth, w,
                  XmNheight, h,
                  NULL);
}

void set_canvas_size(Dimension w, Dimension h)
{
    XtVaSetValues(canvas,
                  XmNwidth, w,
                  XmNheight, h,
                  NULL);
}

static int page_layout = PAGE_FIXED;

int get_pagelayout(void)
{
    return page_layout;
}

void set_pagelayout(int layout)
{
    if (page_layout == layout) {
        return;
    }
    
    if (inwin) {
        errmsg("Can not change layout after initialization of GUI");
        return;
    } else {
        page_layout = layout;
    }
    
/*
 *     if (layout == PAGE_FREE) {
 *         XtVaSetValues(scrollw,
 *         	      XmNscrollingPolicy, XmAPPLICATION_DEFINED,
 * 		      XmNvisualPolicy, XmVARIABLE,
 *                       NULL);
 *     } else {
 *         XtVaSetValues(scrollw,
 *         	      XmNscrollingPolicy, XmAUTOMATIC,
 * 		      XmNvisualPolicy, XmCONSTANT,
 *                       NULL);
 *     }
 */
}

static void graph_scroll_proc(void *data)
{
    graph_scroll((int) data);
    drawgraph();
}

static void graph_zoom_proc(void *data)
{
    graph_zoom((int) data);
    drawgraph();
}

static void world_stack_proc(void *data)
{
    switch ((int) data) {
    case WSTACK_PUSH_ZOOM:
        push_and_zoom();
        break;
    case WSTACK_PUSH:
        push_world();
        break;
    case WSTACK_POP:
        pop_world();
        break;
    case WSTACK_CYCLE:
        cycle_world_stack();
        break;
    default:
        return;
    }
    update_all();
    drawgraph();
}

static void load_example(void *data)
{
    char *s, buf[128];
    
    set_wait_cursor();
    
    s = (char *) data;
    sprintf(buf, "examples/%s", s);
    load_project_file(buf, FALSE);
    update_all();
    drawgraph();

    unset_wait_cursor();
}
