/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 * 
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 * 
 * Copyright (c) 1991-1995 Paul J Turner, Portland, OR
 * Copyright (c) 1996-2005 Grace Development Team
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

/*
 *
 * spreadsheet data stuff
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core_utils.h"
#include "files.h"
#include "ssdata.h"
#include "parser.h"

#include "protos.h"

double *copy_data_column_simple(double *src, int nrows)
{
    double *dest;
    
    if (!src) {
        return NULL;
    }
    
    dest = xmalloc(nrows*SIZEOF_DOUBLE);
    if (dest != NULL) {
        memcpy(dest, src, nrows*SIZEOF_DOUBLE);
    }
    return dest;
}

/* TODO: index_shift */
double *allocate_index_data(int nrows)
{
    int i;
    double *retval;
    
    retval = xmalloc(nrows*SIZEOF_DOUBLE);
    if (retval != NULL) {
        for (i = 0; i < nrows; i++) {
            retval[i] = i;
        }
    }
    return retval;
}

double *allocate_mesh(double start, double stop, int len)
{
    int i;
    double *retval;
    
    retval = xmalloc(len*SIZEOF_DOUBLE);
    if (retval != NULL) {
        double s = (start + stop)/2, d = (stop - start)/2;
        for (i = 0; i < len; i++) {
            retval[i] = s + d*((double) (2*i + 1 - len)/(len - 1));
        }
    }
    return retval;
}

int field_string_to_cols(const char *fs, int *nc, int **cols, int *scol)
{
    int col;
    char *s, *buf;

    buf = copy_string(NULL, fs);
    if (buf == NULL) {
        return RETURN_FAILURE;
    }

    s = buf;
    *nc = 0;
    while ((s = strtok(s, ":")) != NULL) {
	(*nc)++;
	s = NULL;
    }
    *cols = xmalloc((*nc)*SIZEOF_INT);
    if (*cols == NULL) {
        xfree(buf);
        return RETURN_FAILURE;
    }

    strcpy(buf, fs);
    s = buf;
    *nc = 0;
    *scol = -1;
    while ((s = strtok(s, ":")) != NULL) {
        int strcol;
        if (*s == '{') {
            char *s1;
            strcol = TRUE;
            s++;
            if ((s1 = strchr(s, '}')) != NULL) {
                *s1 = '\0';
            }
        } else {
            strcol = FALSE;
        }
        col = atoi(s);
        col--;
        if (strcol) {
            *scol = col;
        } else {
            (*cols)[*nc] = col;
	    (*nc)++;
        }
	s = NULL;
    }
    
    xfree(buf);
    
    return RETURN_SUCCESS;
}

char *cols_to_field_string(int nc, int *cols, int scol)
{
    int i;
    char *s, buf[32];
    
    s = NULL;
    for (i = 0; i < nc; i++) {
        sprintf(buf, "%d", cols[i] + 1);
        if (i != 0) {
            s = concat_strings(s, ":");
        }
        s = concat_strings(s, buf);
    }
    if (scol >= 0) {
        sprintf(buf, ":{%d}", scol + 1);
        s = concat_strings(s, buf);
    }
    
    return s;
}


static char *next_token(char *s, char **token, int *quoted)
{
    *quoted = FALSE;
    *token = NULL;
    
    if (s == NULL) {
        return NULL;
    }
    
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    if (*s == '"') {
        s++;
        *token = s;
        while (*s != '\0' && (*s != '"' || (*s == '"' && *(s - 1) == '\\'))) {
            s++;
        }
        if (*s == '"') {
            /* successfully identified a quoted string */
            *quoted = TRUE;
        }
    } else {
        *token = s;
        if (**token == '\n') {
            /* EOL reached */
            return NULL;
        }
        while (*s != '\n' && *s != '\0' && *s != ' ' && *s != '\t') {
            s++;
        }
    }
    
    if (*s != '\0') {
        *s = '\0';
        s++;
        return s;
    } else {
        return NULL;
    }
}

int parse_ss_row(Quark *pr, const char *s, int *nncols, int *nscols, int **formats)
{
    int ncols;
    int quoted;
    char *buf, *s1, *token;
    double value;
    Dates_format df_pref, ddummy;
    const char *sdummy;

    *nscols = 0;
    *nncols = 0;
    *formats = NULL;
    df_pref = get_date_hint();
    buf = copy_string(NULL, s);
    s1 = buf;
    while ((s1 = next_token(s1, &token, &quoted)) != NULL) {
        if (token == NULL) {
            *nscols = 0;
            *nncols = 0;
            XCFREE(*formats);
            xfree(buf);
            return RETURN_FAILURE;
        }
        
        ncols = *nncols + *nscols;
        /* reallocate the formats array */
        if (ncols % 10 == 0) {
            *formats = xrealloc(*formats, (ncols + 10)*SIZEOF_INT);
        }

        if (quoted) {
            (*formats)[ncols] = FFORMAT_STRING;
            (*nscols)++;
        } else if (parse_date(pr, token, df_pref, FALSE, &value, &ddummy) ==
            RETURN_SUCCESS) {
            (*formats)[ncols] = FFORMAT_DATE;
            (*nncols)++;
        } else if (parse_float(token, &value, &sdummy) == RETURN_SUCCESS) {
            (*formats)[ncols] = FFORMAT_NUMBER;
            (*nncols)++;
        } else {
            /* last resort - treat the field as string, even if not quoted */
            (*formats)[ncols] = FFORMAT_STRING;
            (*nscols)++;
        }
    }
    xfree(buf);
    
    return RETURN_SUCCESS;
}


/* NOTE: the input string will be corrupted! */
int insert_data_row(Quark *q, unsigned int row, char *s)
{
    unsigned int i, ncols = ssd_get_ncols(q);
    int *formats = ssd_get_formats(q);
    char *token;
    int quoted;
    char  **sp;
    double *np;
    Dates_format df_pref, ddummy;
    const char *sdummy;
    int res;
    AMem *amem = quark_get_amem(q);
    ss_data *ssd = ssd_get_data(q);
    Quark *pr = get_parent_project(q); 
    
    df_pref = get_date_hint();
    for (i = 0; i < ncols; i++) {
        s = next_token(s, &token, &quoted);
        if (s == NULL || token == NULL) {
            /* invalid line */
            return RETURN_FAILURE;
        } else {
            if (formats[i] == FFORMAT_STRING) {
                sp = (char **) ssd->data[i];
                sp[row] = amem_strcpy(amem, sp[row], token);
                if (sp[row] != NULL) {
                    res = RETURN_SUCCESS;
                } else {
                    res = RETURN_FAILURE;
                }
            } else if (formats[i] == FFORMAT_DATE) {
                np = (double *) ssd->data[i];
                res = parse_date(pr, token, df_pref, FALSE, &np[row], &ddummy);
            } else {
                np = (double *) ssd->data[i];
                res = parse_float(token, &np[row], &sdummy);
            }
            if (res != RETURN_SUCCESS) {
                return RETURN_FAILURE;
            }
        }
    }
    
    return RETURN_SUCCESS;
}

/*
 * return the next available set in graph gr
 * If target is allocated but with no data, choose it (used for loading sets
 * from project files when sets aren't packed)
 */
static Quark *nextset(Quark *gr)
{
    Quark *pset, **psets;
    RunTime *rt = rt_from_quark(gr);
    
    if (!rt) {
        return NULL;
    }
    
    pset = rt->target_set;
    
    if (pset && get_parent_graph(pset) == gr && set_is_dataless(pset)) {
        rt->target_set = NULL;
        return pset;
    } else {
        int i, nsets;
        
        nsets = quark_get_descendant_sets(gr, &psets);
        for (i = 0; i < nsets; i++) {
            pset = psets[i];
            if (set_is_dataless(pset) == TRUE) {
	        return pset;
	    }
        }
        xfree(psets);
        
        return grace_set_new(gr);
    }
}

int store_data(Quark *q, int load_type)
{
    int ncols, nncols, nncols_req, nscols, nrows;
    int i, j;
    int *formats;
    double *xdata;
    Quark *gr, *pset;
    int x_from_index;
    Quark *pr = get_parent_project(q); 
    RunTime *rt = rt_from_quark(pr);
    ss_data *ssd = ssd_get_data(q);
    
    if (ssd == NULL || rt == NULL) {
        return RETURN_FAILURE;
    }
    ncols = ssd_get_ncols(q);
    nrows = ssd_get_nrows(q);
    formats = ssd_get_formats(q);
    if (ncols <= 0 || nrows <= 0) {
        return RETURN_FAILURE;
    }

    nncols = 0;
    for (j = 0; j < ncols; j++) {
        if (formats[j] != FFORMAT_STRING) {
            nncols++;
        }
    }
    nscols = ncols - nncols;
    
    gr = get_parser_gno();
    if (!gr) {
        return RETURN_FAILURE;
    }
    
    switch (load_type) {
    case LOAD_SINGLE:
        if (nscols > 1) {
            errmsg("Can not use more than one column of strings per set");
            return RETURN_FAILURE;
        }

        nncols_req = settype_cols(rt->curtype);
        x_from_index = FALSE;
        if (nncols_req == nncols + 1) {
            x_from_index = TRUE;
        } else if (nncols_req != nncols) {
	    errmsg("Column count incorrect");
	    return RETURN_FAILURE;
        }

        pset = nextset(gr);
        set_set_type(pset, rt->curtype);

        nncols = 0;
        if (x_from_index) {
            xdata = allocate_index_data(nrows);
            if (xdata == NULL) {
                return RETURN_FAILURE;
            }
            set_set_col(pset, DATA_X, xdata, nrows);
            xfree(xdata);
            nncols++;
        }
        for (j = 0; j < ncols; j++) {
            if (formats[j] == FFORMAT_STRING) {
                set_set_strings(pset, nrows, (char **) ssd->data[j]);
            } else {
                set_set_col(pset, nncols, (double *) ssd->data[j], nrows);
                nncols++;
            }
        }
        if (!set_get_comment(pset)) {
            set_set_comment(pset, ssd->label);
        }
        
        quark_free(q);
        break;
    case LOAD_NXY:
        if (nscols != 0) {
            errmsg("Can not yet use strings when reading in data as NXY");
            return RETURN_FAILURE;
        }
        
        for (i = 0; i < ncols - 1; i++) {
            pset = grace_set_new(gr);
            if (!pset) {
                return RETURN_FAILURE;
            }
            xdata = (double *) ssd->data[0];
            set_set_type(pset, SET_XY);
            set_set_col(pset, DATA_X, xdata, nrows);
            set_set_col(pset, DATA_Y, (double *) ssd->data[i + 1], nrows);
            set_set_comment(pset, ssd->label);
        }
    
        break;
    case LOAD_BLOCK:
        quark_idstr_set(q, ssd->label);
        break;
    default:
        errmsg("Internal error");
        return RETURN_FAILURE;
    }
    
    return RETURN_SUCCESS;
}

int create_set_fromblock(const Quark *ss, Quark *pset,
    int type, int nc, int *coli, int scol, int autoscale)
{
    int i, ncols, blockncols, blocklen, column;
    double *cdata;
    char buf[256], *s;
    int *formats;
    ss_data *blockdata = ssd_get_data(ss);
    if (!blockdata) {
        return RETURN_FAILURE;
    }

    blockncols = ssd_get_ncols(ss);
    if (blockncols <= 0) {
        errmsg("No block data read");
        return RETURN_FAILURE;
    }

    blocklen = ssd_get_nrows(ss);
    
    ncols = settype_cols(type);
    if (nc > ncols) {
        errmsg("Too many columns scanned in column string");
        return RETURN_FAILURE;
    }
    if (nc < ncols) {
	errmsg("Too few columns scanned in column string");
	return RETURN_FAILURE;
    }
    
    for (i = 0; i < nc; i++) {
	if (coli[i] < -1 || coli[i] >= blockncols) {
	    errmsg("Column index out of range");
	    return RETURN_FAILURE;
	}
    }
    
    if (scol >= blockncols) {
	errmsg("String column index out of range");
	return RETURN_FAILURE;
    }

    formats = ssd_get_formats(ss);
    
    /* clear data stored in the set, if any */
    killsetdata(pset);
    
    set_set_type(pset, type);

    for (i = 0; i < nc; i++) {
        column = coli[i];
        if (column == -1) {
            cdata = allocate_index_data(blocklen);
        } else {
            if (formats[column] != FFORMAT_STRING) {
                cdata = copy_data_column_simple(
                    (double *) blockdata->data[column], blocklen);
            } else {
                errmsg("Tried to read doubles from strings!");
                killsetdata(pset);
                return RETURN_FAILURE;
            }
        }
        if (cdata == NULL) {
            killsetdata(pset);
            return RETURN_FAILURE;
        }
        set_set_col(pset, i, cdata, blocklen);
        xfree(cdata);
    }

    /* strings, if any */
    if (scol >= 0) {
        if (formats[scol] != FFORMAT_STRING) {
            errmsg("Tried to read strings from doubles!");
            killsetdata(pset);
            return RETURN_FAILURE;
        } else {
            set_set_strings(pset, blocklen, blockdata->data[scol]);
        }
    }

    s = cols_to_field_string(nc, coli, scol);
    sprintf(buf, "%s, cols %s", blockdata->label, s);
    xfree(s);
    set_set_comment(pset, buf);

    autoscale_graph(get_parent_graph(pset), autoscale);

    return RETURN_SUCCESS;
}
