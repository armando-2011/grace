/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 * 
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 * 
 * Copyright (c) 2001-2004 Grace Development Team
 * 
 * Maintained by Evgeny Stambulchik
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

#include <config.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifdef HAVE_CUPS
#  include <cups/cups.h>
#endif

#include "defines.h"
#include "grace.h"
#include "utils.h"
#include "dicts.h"
#include "core_utils.h"
#include "parser.h"
#include "protos.h"

GUI *gui_new(Grace *grace)
{
    GUI *gui;
    
    gui = xmalloc(sizeof(GUI));
    if (!gui) {
        return NULL;
    }
    memset(gui, 0, sizeof(GUI));
    
    gui->P = grace;

    gui->zoom            = 1.0;

    gui->inwin           = FALSE;
    gui->invert          = TRUE;
    gui->auto_redraw     = TRUE;
    gui->focus_policy    = FOCUS_CLICK;
    gui->draw_focus_flag = TRUE;
    gui->noask           = FALSE;

    gui->instant_update  = FALSE;
    gui->toolbar         = TRUE;
    gui->statusbar       = TRUE;
    gui->locbar          = TRUE;

    gui->install_cmap    = CMAP_INSTALL_AUTO;
    gui->private_cmap    = FALSE;
    
    return gui;
}

void gui_free(GUI *gui)
{
    xfree(gui);
}


void *container_data_new(void)
{
    return NULL;
}

void container_data_free(void *data)
{
}

void *container_data_copy(void *data)
{
    return data;
}

RunTime *runtime_new(Grace *grace)
{
    RunTime *rt;
    char *s;

    QuarkFlavor project_qf = {
        QFlavorProject,
        (Quark_data_new) project_data_new,
        (Quark_data_free) project_data_free,
        NULL
    };

    QuarkFlavor ssd_qf = {
        QFlavorSSD,
        (Quark_data_new) ssd_data_new,
        (Quark_data_free) ssd_data_free,
        (Quark_data_copy) ssd_data_copy
    };

    QuarkFlavor frame_qf = {
        QFlavorFrame,
        (Quark_data_new) frame_data_new,
        (Quark_data_free) frame_data_free,
        (Quark_data_copy) frame_data_copy
    };

    QuarkFlavor graph_qf = {
        QFlavorGraph,
        (Quark_data_new) graph_data_new,
        (Quark_data_free) graph_data_free,
        (Quark_data_copy) graph_data_copy
    };

    QuarkFlavor set_qf = {
        QFlavorSet,
        (Quark_data_new) set_data_new,
        (Quark_data_free) set_data_free,
        (Quark_data_copy) set_data_copy
    };

    QuarkFlavor axisgrid_qf = {
        QFlavorAGrid,
        (Quark_data_new) axisgrid_data_new,
        (Quark_data_free) axisgrid_data_free,
        (Quark_data_copy) axisgrid_data_copy
    };

    QuarkFlavor axis_qf = {
        QFlavorAxis,
        (Quark_data_new) axis_data_new,
        (Quark_data_free) axis_data_free,
        (Quark_data_copy) axis_data_copy
    };

    QuarkFlavor dobject_qf = {
        QFlavorDObject,
        (Quark_data_new) object_data_new,
        (Quark_data_free) object_data_free,
        (Quark_data_copy) object_data_copy
    };

    QuarkFlavor atext_qf = {
        QFlavorAText,
        (Quark_data_new) atext_data_new,
        (Quark_data_free) atext_data_free,
        (Quark_data_copy) atext_data_copy
    };

    QuarkFlavor region_qf = {
        QFlavorRegion,
        (Quark_data_new) region_data_new,
        (Quark_data_free) region_data_free,
        (Quark_data_copy) region_data_copy
    };

    QuarkFlavor container_qf = {
        QFlavorContainer,
        container_data_new,
        container_data_free,
        container_data_copy
    };

    rt = xmalloc(sizeof(RunTime));
    if (!rt) {
        return NULL;
    }
    memset(rt, 0, sizeof(RunTime));

    rt->P = grace;
    
    rt->qfactory = qfactory_new();
    if (!rt->qfactory) {
        runtime_free(rt);
        return NULL;
    }
    quark_factory_set_udata(rt->qfactory, grace);
    quark_flavor_add(rt->qfactory, &project_qf);
    quark_flavor_add(rt->qfactory, &ssd_qf);
    quark_flavor_add(rt->qfactory, &frame_qf);
    quark_flavor_add(rt->qfactory, &graph_qf);
    quark_flavor_add(rt->qfactory, &set_qf);
    quark_flavor_add(rt->qfactory, &axisgrid_qf);
    quark_flavor_add(rt->qfactory, &axis_qf);
    quark_flavor_add(rt->qfactory, &dobject_qf);
    quark_flavor_add(rt->qfactory, &atext_qf);
    quark_flavor_add(rt->qfactory, &region_qf);
    quark_flavor_add(rt->qfactory, &container_qf);

    rt->canvas = canvas_new();
    if (!rt->canvas) {
        runtime_free(rt);
        return NULL;
    }
    canvas_set_udata(rt->canvas, grace);
    canvas_set_fmap_proc(rt->canvas, fmap_proc);
    canvas_set_csparse_proc(rt->canvas, csparse_proc);
    
    rt->safe_mode = TRUE;
    
    /* allocatables */
    rt->grace_home   = NULL;
    rt->print_dests  = NULL;
    rt->print_cmd    = NULL;
    rt->grace_editor = NULL;
    rt->help_viewer  = NULL;
    rt->workingdir   = NULL;
    rt->username     = NULL;
    rt->userhome     = NULL;
    
    /* grace home directory */
    if ((s = getenv("GRACE_HOME")) == NULL) {
	s = bi_home();
    }
    rt->grace_home = copy_string(NULL, s);

#ifdef HAVE_CUPS
    rt->use_cups = TRUE;
#else
    rt->use_cups = FALSE;
#endif

    /* print command */
    if ((s = getenv("GRACE_PRINT_CMD")) == NULL) {
	s = bi_print_cmd();
    }
    rt->print_cmd = copy_string(NULL, s);
    if (rt->use_cups) {
        rt->ptofile = FALSE;
    } else
    /* if no print command defined, print to file by default */
    if (is_empty_string(rt->print_cmd)) {
        rt->ptofile = TRUE;
    } else {
        rt->ptofile = FALSE;
    }

    /* editor */
    if ((s = getenv("GRACE_EDITOR")) == NULL) {
	s = bi_editor();
    }
    rt->grace_editor = copy_string(NULL, s);

    /* html viewer */
    if ((s = getenv("GRACE_HELPVIEWER")) == NULL) {
	s = bi_helpviewer();
    }
    rt->help_viewer = copy_string(NULL, s);
    if (!strstr(rt->help_viewer, "%s")) {
        rt->help_viewer = concat_strings(rt->help_viewer, " %s");
    }

    /* working directory */
    rt->workingdir = xmalloc(GR_MAXPATHLEN);
    getcwd(rt->workingdir, GR_MAXPATHLEN - 1);
    if (rt->workingdir[strlen(rt->workingdir)-1] != '/') {
        rt->workingdir = concat_strings(rt->workingdir, "/");
    }

    /* username */
    s = getenv("LOGNAME");
    if (is_empty_string(s)) {
        s = getlogin();
        if (is_empty_string(s)) {
            s = "a user";
        }
    }
    rt->username = copy_string(NULL, s);
    canvas_set_username(rt->canvas, rt->username);

    /* userhome */
    if ((s = getenv("HOME")) == NULL) {
        s = "/";
    }
    rt->userhome = copy_string(NULL, s);
    if (rt->userhome[strlen(rt->userhome) - 1] != '/') {
        rt->userhome = concat_strings(rt->userhome, "/");
    }

    if (!rt->grace_home   ||
        !rt->print_cmd    ||
        !rt->grace_editor ||
        !rt->help_viewer  ||
        !rt->workingdir   ||
        !rt->username     ||
        !rt->userhome) {
        runtime_free(rt);
        return NULL;
    }
    
    /* dictionaries */
    if (grace_rt_init_dicts(rt) != RETURN_SUCCESS) {
        runtime_free(rt);
        return NULL;
    }

    if (grace_init_print(rt) != RETURN_SUCCESS) {
        runtime_free(rt);
        return NULL;
    }

    rt->print_file[0] = '\0';

    rt->tdevice = 0;
    rt->hdevice = 0;
    
    rt->target_set = NULL;
    
    rt->timer_delay = 200;

    rt->autoscale_onread = AUTOSCALE_XY;

    /* FIXME curstuff */
    rt->curtype   = SET_XY;
    rt->cursource = SOURCE_DISK;

    rt->scrollper = 0.05;
    rt->shexper = 0.05;
    
    rt->resfp = NULL;

    rt->emergency_save = FALSE;
    rt->interrupts = 0;

#ifdef DEBUG
    rt->debuglevel = 0;
#endif

    return rt;
}

void runtime_free(RunTime *rt)
{
    int i;
    
    for (i = 0; i < rt->num_print_dests; i++) {
        PrintDest *pd = &rt->print_dests[i];
        int j;
        xfree(pd->name);
        xfree(pd->inst);
        xfree(pd->printer);
        for (j = 0; j < pd->nogroups; j++) {
            PrintOptGroup *og = &pd->ogroups[j];
            int k;
            xfree(og->name);
            xfree(og->text);
            for (k = 0; k < og->nopts; k++) {
                PrintOption *po = &og->opts[k];
                xfree(po->name);
                xfree(po->text);
                dict_free(po->choices);
            }
            xfree(og->opts);
        }
        xfree(pd->ogroups);
    }
    xfree(rt->print_dests);
    
    xfree(rt->grace_home);
    xfree(rt->print_cmd);
    xfree(rt->grace_editor);
    xfree(rt->help_viewer);
    xfree(rt->workingdir);
    xfree(rt->username);
    xfree(rt->userhome);
    
    qfactory_free(rt->qfactory);
    
    canvas_free(rt->canvas);
    
    grace_rt_free_dicts(rt);

    if (rt->resfp) {
        grace_close(rt->resfp);
    }
    
    xfree(rt);
}

Grace *grace_new(void)
{
    Grace *grace;
    
    grace = xmalloc(sizeof(Grace));
    if (!grace) {
        return NULL;
    }
    memset(grace, 0, sizeof(Grace));

    grace->rt = runtime_new(grace);
    if (!grace->rt) {
        grace_free(grace);
        return NULL;
    }
    
    grace->gui = gui_new(grace);
    if (!grace->gui) {
        grace_free(grace);
        return NULL;
    }

    return grace;
}

void grace_free(Grace *grace)
{
    if (!grace) {
        return;
    }
    
    quark_free(grace->project);
    gui_free(grace->gui);
    runtime_free(grace->rt);
    
    xfree(grace);
}

Grace *grace_from_quark(const Quark *q)
{
    Grace *grace = NULL;
    if (q) {
        grace = (Grace *) quark_factory_get_udata(quark_get_qfactory(q));
    }
    
    return grace;
}

RunTime *rt_from_quark(const Quark *q)
{
    Grace *grace = grace_from_quark(q);
    if (grace) {
        return grace->rt;
    } else {
        return NULL;
    }
}

GUI *gui_from_quark(const Quark *q)
{
    Grace *grace = grace_from_quark(q);
    if (grace) {
        return grace->gui;
    } else {
        return NULL;
    }
}

int grace_set_project(Grace *grace, Quark *project)
{
    if (grace && project) {
        Project *pr = project_get_data(project);
        int i;
        
        quark_free(grace->project);
        grace->project = project;
        parser_state_reset(project);
        
        /* Reset colormap */
        canvas_cmap_reset(grace->rt->canvas);
        for (i = 0; i < pr->ncolors; i++) {
            Colordef *c = &pr->colormap[i];
            canvas_store_color(grace->rt->canvas, c->id, &c->rgb);
        }
        
        /* Set dimensions of all devices */
        set_page_dimensions(grace, pr->page_wpp, pr->page_hpp, TRUE);
        
        /* Reset set autocolorization index */
        grace->rt->setcolor = 0;
        
        /* Request update of color selectors */
        grace->gui->need_colorsel_update = TRUE;

        /* Request update of font selectors */
        grace->gui->need_fontsel_update = TRUE;

        return RETURN_SUCCESS;
    } else {
        return RETURN_FAILURE;
    }
}

/*
 * flag to indicate destination of hardcopy output,
 * ptofile = 0 means print to printer, otherwise print to file
 */
void set_ptofile(Grace *grace, int flag)
{
    grace->rt->ptofile = flag;
}

int get_ptofile(const Grace *grace)
{
    return grace->rt->ptofile;
}

/*
 * set the current print device
 */
int set_printer(Grace *grace, int device)
{
    Canvas *canvas = grace->rt->canvas;
    Device_entry *d = get_device_props(canvas, device);
    if (!d || d->type == DEVICE_TERM) {
        return RETURN_FAILURE;
    } else {
        grace->rt->hdevice = device;
	if (d->type != DEVICE_PRINT) {
            set_ptofile(grace, TRUE);
        }
        return RETURN_SUCCESS;
    }
}

int set_printer_by_name(Grace *grace, const char *dname)
{
    int device;
    
    device = get_device_by_name(grace->rt->canvas, dname);
    
    return set_printer(grace, device);
}

int set_page_dimensions(Grace *grace, int wpp, int hpp, int rescale)
{
    int i;
    Canvas *canvas = grace->rt->canvas;
    
    if (wpp <= 0 || hpp <= 0) {
        return RETURN_FAILURE;
    } else {
        int wpp_old, hpp_old;
        Project *pr = project_get_data(grace->project);
	wpp_old = pr->page_wpp;
	hpp_old = pr->page_hpp;
        
        pr->page_wpp = wpp;
	pr->page_hpp = hpp;
        if (rescale) {
            if (hpp*wpp_old - wpp*hpp_old != 0) {
                /* aspect ratio changed */
                double ext_x, ext_y;
                double old_aspectr, new_aspectr;
                
                old_aspectr = (double) wpp_old/hpp_old;
                new_aspectr = (double) wpp/hpp;
                if (old_aspectr >= 1.0 && new_aspectr >= 1.0) {
                    ext_x = new_aspectr/old_aspectr;
                    ext_y = 1.0;
                } else if (old_aspectr <= 1.0 && new_aspectr <= 1.0) {
                    ext_x = 1.0;
                    ext_y = old_aspectr/new_aspectr;
                } else if (old_aspectr >= 1.0 && new_aspectr <= 1.0) {
                    ext_x = 1.0/old_aspectr;
                    ext_y = 1.0/new_aspectr;
                } else {
                    ext_x = new_aspectr;
                    ext_y = old_aspectr;
                }

                rescale_viewport(grace->project, ext_x, ext_y);
            } 
        }
        for (i = 0; i < number_of_devices(canvas); i++) {
            Device_entry *d = get_device_props(canvas, i);
            d->pg.width  = (unsigned long) wpp*d->pg.dpi/72;
            d->pg.height = (unsigned long) hpp*d->pg.dpi/72;
        }
        return RETURN_SUCCESS;
    }
}

#ifdef HAVE_CUPS
static void parse_group(PrintDest *pd, ppd_group_t *group)
{
    int           i, j;           /* Looping vars */
    ppd_option_t  *option;        /* Current option */
    ppd_choice_t  *choice;        /* Current choice */
    ppd_group_t   *subgroup;      /* Current subgroup */
    PrintOptGroup *og;
    PrintOption   *po;

    pd->ogroups = xrealloc(pd->ogroups, (pd->nogroups + 1)*sizeof(PrintOptGroup));
    if (!pd->ogroups) {
        return;
    }
    
    og = &pd->ogroups[pd->nogroups];
    pd->nogroups++;
    
    og->name = copy_string(NULL, group->name);
    og->text = copy_string(NULL, group->text);
    
    og->opts = xcalloc(group->num_options, sizeof(PrintOption));
    if (!og->opts) {
        return;
    }
    og->nopts = 0;

    for (i = 0, option = group->options, po = og->opts; i < group->num_options; i++, option++) {
        po->name = copy_string(NULL, option->keyword);
        po->text = copy_string(NULL, option->text);

        po->choices = dict_new();
        po->selected = -1;
        dict_resize(po->choices, option->num_choices);
        for (j = 0, choice = option->choices; j < option->num_choices; j++, choice++) {
            DictEntry de;
            
            de.key   = j;
            de.name  = choice->choice;
            de.descr = choice->text;
            dict_entry_copy(&po->choices->entries[j], &de);

            if (choice->marked) {
                po->selected = de.key;
                dict_entry_copy(&po->choices->defaults, &de);
            }
        }
        if (po->selected != -1) {
            og->nopts++;
            po++;
            pd->nopts++;
        } else {
            xfree(po->name);
            xfree(po->text);
            dict_free(po->choices);
        }
    }

    for (i = 0, subgroup = group->subgroups; i < group->num_subgroups; i++, subgroup++) {
        parse_group(pd, subgroup);
    }
}
#endif

int grace_init_print(RunTime *rt)
{
#ifdef HAVE_CUPS
    int i;
    cups_dest_t *dests;
    
    rt->print_dest = 0;
    rt->num_print_dests = cupsGetDests(&dests);
    rt->print_dests = xcalloc(rt->num_print_dests, sizeof(PrintDest));
    
    if (rt->print_dests == NULL) {
        return RETURN_FAILURE;
    }
    
    for (i = 0; i < rt->num_print_dests; i++) {
        cups_dest_t *dest = &dests[i];
        PrintDest *pd = &rt->print_dests[i];

        char *printer;
        int           j;
        const char    *filename;        /* PPD filename */
        ppd_file_t    *ppd;             /* PPD data */
        ppd_group_t   *group;           /* Current group */
        
        printer = copy_string(NULL, dest->name);
        if (dest->instance) {
            printer = concat_strings(printer, "/");
            printer = concat_strings(printer, dest->instance);
        }
        
        pd->name    = copy_string(NULL, dest->name);
        pd->inst    = copy_string(NULL, dest->instance);
        pd->printer = printer;
        
        if (dest->is_default) {
            rt->print_dest = i;
        }
        
        if ((filename = cupsGetPPD(dest->name)) == NULL) {
            break;
        }

        if ((ppd = ppdOpenFile(filename)) == NULL) {
            unlink(filename);
            break;
        }

        ppdMarkDefaults(ppd);
        cupsMarkOptions(ppd, dest->num_options, dest->options);

        for (j = 0, group = ppd->groups; j < ppd->num_groups; j++, group++) {
            parse_group(pd, group);
        }

        ppdClose(ppd);
        unlink(filename);
    }
    cupsFreeDests(rt->num_print_dests, dests);
#else
    rt->print_dest = 0;
    rt->num_print_dests = 0;
#endif

    return RETURN_SUCCESS;
}

int grace_print(const Grace *grace, const char *fname)
{
    char tbuf[128];
    int retval = RETURN_SUCCESS;
#ifdef HAVE_CUPS
    if (grace->rt->use_cups) {
        PrintDest *pd = &grace->rt->print_dests[grace->rt->print_dest];
        int i, j;
        int jobid;
        int num_options = 0;
        cups_option_t *options = NULL;

        for (i = 0; i < pd->nogroups; i++) {
            PrintOptGroup *og = &pd->ogroups[i];

            for (j = 0; j < og->nopts; j++) {
                PrintOption *po = &og->opts[j];
                char *value;

                if (po->selected != po->choices->defaults.key) {
                    dict_get_name_by_key(po->choices, po->selected, &value);
                    num_options = cupsAddOption(po->name, value, num_options, &options);
                }
            }
        }
        
        jobid = cupsPrintFile(pd->name, fname, "Grace", num_options, options);
        if (jobid == 0) {
            errmsg(ippErrorString(cupsLastError()));
            retval = RETURN_FAILURE;
        }
    } else
#endif
    {
        sprintf(tbuf, "%s %s", get_print_cmd(grace), fname);
        system_wrap(tbuf);
#ifndef PRINT_CMD_UNLINKS
        unlink(fname);
#endif
    }
    
    return retval;
}

int gui_is_page_free(const GUI *gui)
{
    return gui->page_free;
}

void gui_set_page_free(GUI *gui, int onoff)
{
    if (gui->page_free == onoff) {
        return;
    }
    
    if (gui->inwin) {
        errmsg("Can not change layout after initialization of GUI");
        return;
    } else {
        gui->page_free = onoff;
    }
}

void gui_set_barebones(GUI *gui)
{
    gui->locbar    = FALSE;
    gui->toolbar   = FALSE;
    gui->statusbar = FALSE;
}
