/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 * 
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 * 
 * Copyright (c) 2005 Grace Development Team
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
 * Numeric stuff
 *
 */

#ifndef __NUMERICS_H_
#define __NUMERICS_H_

#include "grace.h"

typedef struct {
    unsigned int size;
    double *x;
    int allocated;
} DArray;

DArray *darray_new(unsigned int size);
void darray_free(DArray *da);

int darray_min(const DArray *da, double *val);
int darray_max(const DArray *da, double *val);
int darray_avg(const DArray *da, double *val);
int darray_std(const DArray *da, double *val);

#endif /* __NUMERICS_H_ */
