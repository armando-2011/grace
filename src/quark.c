/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 * 
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 * 
 * Copyright (c) 2002,2003 Grace Development Team
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

#include <stdlib.h>
#include <string.h>

#include "grace.h"

static void _quark_free(Quark *q);

static void quark_storage_free(void *data)
{
    _quark_free((Quark *) data);
}

static Quark *quark_new_raw(Quark *parent, unsigned int fid, void *data)
{
    Quark *q;

    q = xmalloc(sizeof(Quark));
    if (q) {
        memset(q, 0, sizeof(Quark));
        
        q->fid = fid;
        q->data = data;
        
        q->children = storage_new(quark_storage_free, NULL, NULL);
        
        if (!q->children) {
            xfree(q);
            return NULL;
        }
        
        if (parent) {
            q->parent = parent;
            q->grace = parent->grace;
            parent->refcount++;
            storage_add(parent->children, q);
            
            quark_dirtystate_set(parent, TRUE);
        }
    }
    
    return q;
}

Quark *quark_root(Grace *grace, unsigned int fid)
{
    Quark *q;
    QuarkFlavor *qf;
    void *data;
    
    qf = quark_flavor_get(grace, fid);
    
    data = qf->data_new();
    q = quark_new_raw(NULL, fid, data);
    q->grace = grace;
    
    return q;
}

Quark *quark_new(Quark *parent, unsigned int fid)
{
    Quark *q;
    QuarkFlavor *qf;
    void *data;
    
    qf = quark_flavor_get(parent->grace, fid);
    
    if (!qf) {
        return NULL;
    }
    
    data = qf->data_new();
    q = quark_new_raw(parent, fid, data);
    
    return q;
}

static void _quark_free(Quark *q)
{
    if (q) {
        QuarkFlavor *qf;
        Quark *parent = q->parent;
        
        qf = quark_flavor_get(q->grace, q->fid);
        if (q->cb) {
            q->cb(q, QUARK_ETYPE_DELETE, q->cbdata);
        }
        qf->data_free(q->data);
        if (parent) {
            parent->refcount--;
        }
        
        storage_free(q->children);
        xfree(q->idstr);
        xfree(q);
        if (q->refcount != 0) {
            errmsg("Freed a referenced quark!");
        }
    }
}

void quark_free(Quark *q)
{
    if (q) {
        Quark *parent = q->parent;
        
        if (parent) {
            quark_dirtystate_set(parent, TRUE);
            storage_extract_data(parent->children, q);
        }
        
        _quark_free(q);
    }
}

Quark *quark_copy2(Quark *newparent, const Quark *q);

static int copy_hook(unsigned int step, void *data, void *udata)
{
    Quark *child = (Quark *) data;
    Quark *newparent = (Quark *) udata;
    Quark *newchild;

    newchild = quark_copy2(newparent, child);
    
    storage_add(newparent->children, newchild);
    
    return TRUE;
}

Quark *quark_copy2(Quark *newparent, const Quark *q)
{
    Quark *new;
    QuarkFlavor *qf;
    void *data;
    
    qf = quark_flavor_get(q->grace, q->fid);
    data = qf->data_copy(q->data);
    new = quark_new_raw(newparent, q->fid, data);

    storage_traverse(q->children, copy_hook, new);
    
    return new;
}

Quark *quark_copy(const Quark *q)
{
    return quark_copy2(q->parent, q);
}

void quark_dirtystate_set(Quark *q, int flag)
{
    if (flag) {
        q->dirtystate++;
        if (q->cb) {
            q->cb(q, QUARK_ETYPE_MODIFY, q->cbdata);
        }
        if (q->parent) {
            quark_dirtystate_set(q->parent, TRUE);
        }
    } else {
        q->dirtystate = 0;
        /* FIXME: for i in q->children do i->dirtystate = 0 */
    }
}

int quark_dirtystate_get(const Quark *q)
{
    return q->dirtystate;
}

void quark_idstr_set(Quark *q, const char *s)
{
    if (q) {
        q->idstr = copy_string(q->idstr, s);
    }
}

char *quark_idstr_get(const Quark *q)
{
    return q->idstr;
}

typedef struct {
    char *s;
    Quark *child;
} QTFindHookData;

static int find_hook(unsigned int step, void *data, void *udata)
{
    Quark *q = (Quark *) data;
    QTFindHookData *_cbdata = (QTFindHookData *) udata;
    
    if (compare_strings(q->idstr, _cbdata->s)) {
        _cbdata->child = q;
        return FALSE;
    } else {
        return TRUE;
    }
}

Quark *quark_find_child_by_idstr(Quark *q, const char *s)
{
    QTFindHookData _cbdata;
    _cbdata.s = (char *) s;
    _cbdata.child = NULL;
    if (q && s) {
        storage_traverse(q->children, find_hook, &_cbdata);
        
    }
    return _cbdata.child;
}

int quark_cb_set(Quark *q, Quark_cb cb, void *cbdata)
{
    if (q) {
        q->cb     = cb;
        q->cbdata = cbdata;
        
        return RETURN_SUCCESS;
    } else {
        return RETURN_FAILURE;
    }
}

typedef struct {
    unsigned int depth;
    unsigned int step;
    Quark_traverse_hook hook;
    void *udata;
} QTHookData;

static int _quark_traverse(Quark *q, QTHookData *_cbdata);

static int hook(unsigned int step, void *data, void *udata)
{
    Quark *q = (Quark *) data;
    QTHookData *_cbdata = (QTHookData *) udata;
    
    _cbdata->step = step;
    
    return _quark_traverse(q, _cbdata);
}

static int _quark_traverse(Quark *q, QTHookData *_cbdata)
{
    if (_cbdata->hook(q, _cbdata->depth, _cbdata->step, _cbdata->udata) == TRUE) {
        _cbdata->depth++;

        storage_traverse(q->children, hook, _cbdata);
        
        return TRUE;
    } else {
        return FALSE;
    }
}

void quark_traverse(Quark *q, Quark_traverse_hook hook, void *udata)
{
    QTHookData _cbdata;
    _cbdata.depth = 0;
    _cbdata.step  = 0;
    _cbdata.hook  = hook;
    _cbdata.udata = udata;
    
    _quark_traverse(q, &_cbdata);
}
