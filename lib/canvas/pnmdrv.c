/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 * 
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 * 
 * Copyright (c) 1996-2003 Grace Development Team
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
 * Grace PNM driver (based upon the generic xrst driver)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CANVAS_BACKEND_API
#include "grace/canvas.h"

#ifdef HAVE_LIBXMI

#define DEFAULT_PNM_FORMAT PNM_FORMAT_PPM

static int pnm_op_parser(const Canvas *canvas, void *data, const char *opstring)
{
    PNM_data *pnmdata = (PNM_data *) data;
    
    if (!strcmp(opstring, "rawbits:on")) {
        pnmdata->rawbits = TRUE;
        return RETURN_SUCCESS;
    } else if (!strcmp(opstring, "rawbits:off")) {
        pnmdata->rawbits = FALSE;
        return RETURN_SUCCESS;
    } else if (!strcmp(opstring, "format:pbm")) {
        pnmdata->format = PNM_FORMAT_PBM;
        return RETURN_SUCCESS;
    } else if (!strcmp(opstring, "format:pgm")) {
        pnmdata->format = PNM_FORMAT_PGM;
        return RETURN_SUCCESS;
    } else if (!strcmp(opstring, "format:ppm")) {
        pnmdata->format = PNM_FORMAT_PPM;
        return RETURN_SUCCESS;
    } else {
        return RETURN_FAILURE;
    }
}

static int pnm_output(const Canvas *canvas, void *data,
    unsigned int ncolors, unsigned int *colors, Xrst_pixmap *pm)
{
    PNM_data *pnmdata = (PNM_data *) data;
    FILE *fp;
    int w, h;
    int i, j, k;
    int c;
    unsigned char r, g, b;
    unsigned char y, pbm_buf;
    
    fp = canvas_get_prstream(canvas);
    w = pm->width;
    h = pm->height;
    
    if (pnmdata->rawbits == TRUE) {
        switch (pnmdata->format) {
        case PNM_FORMAT_PBM:
            fprintf(fp, "P4\n");
            break;
        case PNM_FORMAT_PGM:
            fprintf(fp, "P5\n");
            break;
        case PNM_FORMAT_PPM:
            fprintf(fp, "P6\n");
            break;
        }
    } else {
        switch (pnmdata->format) {
        case PNM_FORMAT_PBM:
            fprintf(fp, "P1\n");
            break;
        case PNM_FORMAT_PGM:
            fprintf(fp, "P2\n");
            break;
        case PNM_FORMAT_PPM:
            fprintf(fp, "P3\n");
            break;
        }
    }
    
    fprintf(fp, "#Creator: %s\n", "Grace/libcanvas");
    
    fprintf(fp, "%d %d\n", w, h);
    
    if (pnmdata->format != PNM_FORMAT_PBM) {
        fprintf(fp, "255\n");
    }
    
    k = 0;
    pbm_buf = 0;
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            RGB rgb;
            c = pm->matrix[i][j];
            if (get_rgb(canvas, c, &rgb) == RETURN_SUCCESS) {
                r = rgb.red   >> (CANVAS_BPCC - 8);
                g = rgb.green >> (CANVAS_BPCC - 8);
                b = rgb.blue  >> (CANVAS_BPCC - 8);
            } else {
                r = 0;
                g = 0;
                b = 0;
            }
            if (pnmdata->rawbits == TRUE) {
                switch (pnmdata->format) {
                case PNM_FORMAT_PBM:
                    y = (r == 255 &&  g == 255 && b == 255 ? 0x00:0x01);
                    pbm_buf |= (y << (7 - k));
                    k++;
                    /* completed byte or padding line */
                    if (k == 8 || j == w - 1) {
                        fwrite(&pbm_buf, 1, 1, fp);
                        k = 0;
                        pbm_buf = 0;
                    }
                    break;
                case PNM_FORMAT_PGM:
                    y = INTENSITY(r, g, b);
                    fwrite(&y, 1, 1, fp);
                    break;
                case PNM_FORMAT_PPM:
                    fwrite(&r, 1, 1, fp);
                    fwrite(&g, 1, 1, fp);
                    fwrite(&b, 1, 1, fp);
                    break;
                }
            } else {
                switch (pnmdata->format) {
                case PNM_FORMAT_PBM:
                    y = (r == 255 &&  g == 255 && b == 255 ? 0:1);
                    fprintf(fp, "%1d\n", y);
                    break;
                case PNM_FORMAT_PGM:
                    y = INTENSITY(r, g, b);
                    fprintf(fp, "%3d\n", y);
                    break;
                case PNM_FORMAT_PPM:
                    fprintf(fp, "%3d %3d %3d\n", r, g, b);
                    break;
                }
            }
        }
    }
    
    return RETURN_SUCCESS;
}

int register_pnm_drv(Canvas *canvas)
{
    XrstDevice_entry xdev;
    PNM_data *pnmdata;
    
    pnmdata = xmalloc(sizeof(PNM_data));
    if (pnmdata) {
        memset(pnmdata, 0, sizeof(PNM_data));
        pnmdata->format  = DEFAULT_PNM_FORMAT;
        pnmdata->rawbits = TRUE;
        
        xdev.type     = DEVICE_FILE;
        xdev.name     = "PNM";
        xdev.fext     = "pnm";
        xdev.fontaa   = TRUE;
        xdev.parser   = pnm_op_parser;
        xdev.dump     = pnm_output;
        xdev.devdata  = pnmdata;
        xdev.freedata = xfree;

        return register_xrst_device(canvas, &xdev);
    } else {
        return -1;
    }
}

#else
void _pnmdrv_c_dummy_func(void) {}
#endif
