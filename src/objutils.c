/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 * 
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 * 
 * Copyright (c) 1991-1995 Paul J Turner, Portland, OR
 * Copyright (c) 1996-2001 Grace Development Team
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
 * operations on objects (strings, lines, boxes, and arcs)
 *
 */

#include <config.h>
#include <cmath.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "globals.h"

#include "graphs.h"
#include "draw.h"
#include "utils.h"
#include "objutils.h"
#include "protos.h"

static int object_data_size(OType type)
{
    int size;

    switch (type) {
    case DO_LINE:
        size = sizeof(DOLineData);
        break;
    case DO_BOX:
        size = sizeof(DOBoxData);
        break;
    case DO_ARC:
        size = sizeof(DOArcData);
        break;
    case DO_STRING:
        size = sizeof(DOStringData);
        break;
    default:
        size = 0;
    }
    
    return size;
}

char *object_types(OType type)
{
    char *s = "";
    switch (type) {
    case DO_LINE:
        s = "line";
        break;
    case DO_BOX:
        s = "box";
        break;
    case DO_ARC:
        s = "arc";
        break;
    case DO_STRING:
        s = "string";
        break;
    case DO_NONE:
        s = "none";
        break;
    }
    
    return s;
}

DObject *object_new(void)
{
    DObject *o;

    o = xmalloc(sizeof(DObject));
    memset(o, 0, sizeof(DObject));
    if (o) {
        o->active = FALSE;
        
        o->type = DO_NONE;

        o->loctype = COORD_VIEW;
        o->ap.x = 0.0;
        o->ap.y = 0.0;
    
        o->offset.x = 0.0;
        o->offset.y = 0.0;
        o->angle = 0.0;
    
        o->line.pen.color = 1;
        o->line.pen.pattern = 1;
        o->line.style = 1;
        o->line.width = 1.0;
        o->fillpen.color = 1;
        o->fillpen.pattern = 0;
        
        o->odata = NULL;
        
        /* o->bb = ; */
    }
    
    return o;
}

void *object_data_new(OType type)
{
    void *odata;
    odata = xmalloc(object_data_size(type));
    if (odata == NULL) {
        return NULL;
    }
    switch (type) {
    case DO_LINE:
        {
            DOLineData *l = (DOLineData *) odata;
            l->length    = 0.0;
            l->arrow_end = 0; 
            set_default_arrow(&l->arrow);
        }
        break;
    case DO_BOX:
        {
            DOBoxData *b = (DOBoxData *) odata;
            b->width  = 0.0;
            b->height = 0.0;
        }
        break;
    case DO_ARC:
        {
            DOArcData *e = (DOArcData *) odata;
            e->width  = 0.0;
            e->height = 0.0;
            e->angle1 =   0.0;
            e->angle2 = 360.0;
            e->fillmode = ARCFILL_CHORD;
        }
        break;
    case DO_STRING:
        {
            DOStringData *s = (DOStringData *) odata;
            s->s    = NULL;
            s->font = 0;
            s->just = 0;
            s->size = 1.0;
        }
        break;
    case DO_NONE:
        break;
    }
    
    return odata;
}

DObject *object_new_complete(OType type)
{
    DObject *o;
    
    o = object_new();
    if (o) {
        o->active = TRUE;
        o->type   = type;
        o->odata  = object_data_new(type);
        if (o->odata == NULL) {
            xfree(o);
            return NULL;
        }
    }
    
    return o;
}

static int object_set_data(DObject *o, void *odata)
{
    int size = object_data_size(o->type);
    
    if (!size) {
        return RETURN_FAILURE;
    }

    memcpy(o->odata, (void *) odata, size);
    
    if (o->type == DO_STRING) {
        ((DOStringData *) (o->odata))->s =
            copy_string(NULL, ((DOStringData *) odata)->s);
    }
    
    return RETURN_SUCCESS;
}

DObject *object_copy(DObject *osrc)
{
    DObject *odest;
    void *odata;
    
    if (!osrc) {
        return NULL;
    }
    
    odest = object_new_complete(osrc->type);
    if (!odest) {
        return NULL;
    }
    
    /* Save odata pointer before memcpy overwrites it */
    odata = odest->odata;
    
    memcpy(odest, osrc, sizeof(DObject));
    
    /* Restore odata */
    odest->odata = odata;
    
    if (object_set_data(odest, osrc->odata) != RETURN_SUCCESS) {
        object_free(odest);
        return NULL;
    }
    
    return odest;
}

void object_free(DObject *o)
{
    if (o) {
        if (o->type == DO_STRING) {
            DOStringData *s = (DOStringData *) o->odata;
            xfree(s->s);
        }
        xfree(o->odata);
        xfree(o);
    }
}

DObject *next_object(graph *g, OType type)
{
    DObject *o;
    Storage *objects;
    
    if (!g) {
        return NULL;
    }
    
    objects = g->dobjects;
    o = object_new_complete(type);
    if (o) {
        if (storage_add(objects, (void *) o) == RETURN_SUCCESS) {
            set_dirtystate();
            return o;
        } else {
            object_free(o);
            return NULL;
        }
    } else {
        return NULL;
    }
}

DObject *object_get(graph *g, int id)
{
    DObject *o;
    Storage *objects = g->dobjects;
    
    if (storage_get_data_by_id(objects, id, (void **) &o) != RETURN_SUCCESS) {
        o = NULL;
    }
    
    return o;
}

int get_object_bb(DObject *o, view *bb)
{
    if (o) {
        *bb = o->bb;
        return RETURN_SUCCESS;
    } else {
        return RETURN_FAILURE;
    }
}

int kill_object(graph *g, int id)
{
    Storage *objects = g->dobjects;
    if (storage_delete_by_id(objects, id) == RETURN_SUCCESS) {
        set_dirtystate();
        return RETURN_SUCCESS;
    } else {
        return RETURN_FAILURE;
    }
}

DObject *duplicate_object(graph *g, int id)
{
    return storage_duplicate(g->dobjects, id);
}

void move_object(DObject *o, VVector shift)
{
    if (!o) {
        return;
    }

    if (o->loctype == COORD_VIEW) {
        o->ap.x += shift.x;
        o->ap.y += shift.y;
    } else {
        WPoint wp;
        VPoint vp;
        
        wp.x = o->ap.x;
        wp.y = o->ap.y;
        
        vp = Wpoint2Vpoint(wp);
        vp.x += shift.x;
        vp.y += shift.y;
        
        view2world(vp.x, vp.y, &o->ap.x, &o->ap.y);
    }
    
    set_dirtystate();
}

int object_place_at_vp(DObject *o, VPoint vp)
{
    if (!o) {
        return RETURN_FAILURE;
    }
    
    if (o->loctype == COORD_VIEW) {
        o->ap.x = vp.x;
        o->ap.y = vp.y;
    } else {
        view2world(vp.x, vp.y, &o->ap.x, &o->ap.y);
    }
    
    set_dirtystate();
    return RETURN_SUCCESS;
}

int isactive_object(DObject *o)
{
    return o->active;
}

void set_plotstr_string(plotstr *pstr, char *s)
{
    pstr->s = copy_string(pstr->s, s);
}
