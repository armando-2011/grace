/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 * 
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 * 
 * Copyright (c) 2001-2003 Grace Development Team
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

#include <config.h>

#include "globals.h"
#include "defines.h"
#include "utils.h"
#include "dicts.h"
#include "grace/canvas.h"
#include "graphs.h"
#include "graphutils.h"
#include "objutils.h"
#include "files.h"
#include "xfile.h"
#include "xstrings.h"
#include "protos.h"

#define w_or_v(x) ((x == COORD_WORLD) ? VStrWorld:VStrView)

/*
 * XML project output
 */

static void xmlio_set_active(Attributes *attrs, int active)
{
    attributes_set_bval(attrs, AStrActive, active);
}

static void xmlio_set_world_value(Attributes *attrs, char *name, double value)
{
    attributes_set_dval_formatted(attrs, name, value,
        project_get_sformat(grace->project));
}

static void xmlio_set_inout_placement(RunTime *rt, Attributes *attrs, int inout)
{
    char *s;
    
    s = inout_placement_name(rt, inout);
    
    attributes_set_sval(attrs, AStrInoutPlacement, s);
}

static void xmlio_set_side_placement(RunTime *rt, Attributes *attrs, PlacementType placement)
{
    char *s;
    
    s = side_placement_name(rt, placement);

    attributes_set_sval(attrs, AStrSidePlacement, s);
}

static void xmlio_set_offset(Attributes *attrs, double offset1, double offset2)
{
    char buf[32];
    sprintf(buf, "(%g, %g)", offset1, offset2);
    attributes_set_sval(attrs, AStrOffset, buf);
}

static void xmlio_set_offset_placement(Attributes *attrs,
    int autoplace, double offset1, double offset2)
{
    if (autoplace) {
        attributes_set_sval(attrs, AStrOffset, VStrAuto);
    } else {
        xmlio_set_offset(attrs, offset1, offset2);
    }
}

static void xmlio_set_angle(Attributes *attrs, double angle)
{
    attributes_set_dval(attrs, AStrAngle, angle);
}

static void xmlio_set_font_ref(Attributes *attrs, int font)
{
    attributes_set_ival(attrs, AStrFontId, font);
}

static void xmlio_set_color_ref(Attributes *attrs, int color)
{
    attributes_set_ival(attrs, AStrColorId, color);
}

static void xmlio_set_pattern_ref(Attributes *attrs, int pattern)
{
    attributes_set_ival(attrs, AStrPatternId, pattern);
}


static void xmlio_write_location(XFile *xf, Attributes *attrs,
    int loctype, double x, double y)
{
    attributes_reset(attrs);
    attributes_set_sval(attrs, AStrType, w_or_v(loctype));
    if (loctype == COORD_WORLD) {
        xmlio_set_world_value(attrs, AStrX, x);
        xmlio_set_world_value(attrs, AStrY, y);
    } else {
        attributes_set_dval(attrs, AStrX, x);
        attributes_set_dval(attrs, AStrY, y);
    }
    xfile_empty_element(xf, EStrLocation, attrs);
}

static void xmlio_write_format_spec(XFile *xf, Attributes *attrs,
    char *name, int format, int prec)
{
    attributes_reset(attrs);
    attributes_set_sval(attrs, AStrFormat, get_format_types(format));
    attributes_set_ival(attrs, AStrPrec, prec);
    xfile_empty_element(xf, name, attrs);
}

static void xmlio_write_face_spec(XFile *xf, Attributes *attrs,
    int font, double charsize, int color)
{
    attributes_reset(attrs);

    xmlio_set_font_ref(attrs, font);
    xmlio_set_color_ref(attrs, color);
    attributes_set_dval(attrs, AStrCharSize, charsize);
    xfile_empty_element(xf, EStrFaceSpec, attrs);
}

static void xmlio_write_fill_spec(XFile *xf, Attributes *attrs, Pen *pen)
{
    attributes_reset(attrs);

    xmlio_set_color_ref(attrs, pen->color);
    xmlio_set_pattern_ref(attrs, pen->pattern);
    xfile_empty_element(xf, EStrFillSpec, attrs);
}

static void xmlio_write_line_spec(XFile *xf, Attributes *attrs,
    Pen *pen, double linew, int lines)
{
    attributes_reset(attrs);

    xmlio_set_color_ref(attrs, pen->color);
    xmlio_set_pattern_ref(attrs, pen->pattern);
    attributes_set_ival(attrs, AStrStyleId, lines);
    attributes_set_dval(attrs, AStrWidth, linew);
    xfile_empty_element(xf, EStrLineSpec, attrs);
}

static void xmlio_write_text(XFile *xf, char *text)
{
    if (is_empty_string(text)) {
        xfile_empty_element(xf, EStrText, NULL);
    } else {
        xfile_text_element(xf, EStrText, NULL, text, TRUE);
    }
}

static void xmlio_write_arrow(XFile *xf, Attributes *attrs, Arrow *arrow)
{
    attributes_reset(attrs);

    attributes_set_ival(attrs, AStrType, arrow->type); /* FIXME: textual */
    attributes_set_dval(attrs, AStrLength, arrow->length);
    attributes_set_dval(attrs, AStrDlFf, arrow->dL_ff);
    attributes_set_dval(attrs, AStrLlFf, arrow->lL_ff);
    xfile_empty_element(xf, EStrArrow, attrs);
}

int save_fontmap(XFile *xf, const Project *pr)
{
    int i;
    Attributes *attrs;
    
    attrs = attributes_new();
    
    if (attrs == NULL) {
        return RETURN_FAILURE;
    }

    xfile_begin_element(xf, EStrFontmap, NULL);
    for (i = 0; i < pr->nfonts; i++) {
        Fontdef *f = &pr->fontmap[i];
        attributes_reset(attrs);
        attributes_set_ival(attrs, AStrId, f->id);
        attributes_set_sval(attrs, AStrName, f->fontname);
        attributes_set_sval(attrs, AStrFallback, f->fallback);
        xfile_empty_element(xf, EStrFontDef, attrs);
    }
    xfile_end_element(xf, EStrFontmap);
    
    attributes_free(attrs);
    
    return RETURN_SUCCESS;
}

int save_colormap(XFile *xf, const Canvas *canvas)
{
    int i;
    Attributes *attrs;
    
    attrs = attributes_new();
    
    if (attrs == NULL) {
        return RETURN_FAILURE;
    }

    xfile_begin_element(xf, EStrColormap, NULL);
    for (i = 0; i < number_of_colors(canvas); i++) {
        Color *color;
        color = get_color_def(canvas, i);
        if (color != NULL && color->ctype == COLOR_MAIN) {
            char buf[16];
            attributes_reset(attrs);
            attributes_set_ival(attrs, AStrId, i);
            sprintf(buf, "#%02x%02x%02x",
                color->rgb.red, color->rgb.green, color->rgb.blue);
            attributes_set_sval(attrs, AStrRgb, buf);
            attributes_set_sval(attrs, AStrName, color->cname);
            xfile_empty_element(xf, EStrColorDef, attrs);
        }
    }
    xfile_end_element(xf, EStrColormap);

    attributes_free(attrs);
    
    return RETURN_SUCCESS;
}

int save_axis_properties(XFile *xf, tickmarks *t)
{
    Attributes *attrs;
    
    if (!t) {
        return RETURN_SUCCESS;
    }
    attrs = attributes_new();
    
    if (attrs == NULL) {
        return RETURN_FAILURE;
    }

    attributes_set_bval(attrs, AStrZero, t->zero);
    xmlio_set_offset(attrs, t->offsx, t->offsy);
    xfile_empty_element(xf, EStrPlacement, attrs);
    
    attributes_reset(attrs);
    xmlio_set_active(attrs, t->t_drawbar);
    xfile_begin_element(xf, EStrAxisbar, attrs);
    {
        Pen pen;
        pen.pattern = 1;
        pen.color = t->t_drawbarcolor;
        xmlio_write_line_spec(xf, attrs,
            &pen, t->t_drawbarlinew, t->t_drawbarlines);
    }
    xfile_end_element(xf, EStrAxisbar);

    attributes_reset(attrs);
    attributes_set_sval(attrs, AStrLayout,
        t->label_layout == LAYOUT_PERPENDICULAR ? VStrPerpendicular:VStrParallel);
    xmlio_set_offset_placement(attrs,
        t->label_place == TYPE_AUTO, t->label.offset.x, t->label.offset.y);
    xmlio_set_side_placement(grace->rt, attrs, t->label_op);
    xfile_begin_element(xf, EStrAxislabel, attrs);
    {
        xmlio_write_face_spec(xf, attrs,
            t->label.font, t->label.charsize, t->label.color);
        xmlio_write_text(xf, t->label.s);
    }
    xfile_end_element(xf, EStrAxislabel);

    attributes_reset(attrs);
    xmlio_set_world_value(attrs, AStrMajorStep, t->tmajor);
    attributes_set_ival(attrs, AStrMinorDivisions, t->nminor);
    attributes_set_ival(attrs, AStrAutoTicking, t->t_autonum);
    attributes_set_bval(attrs, AStrRoundedPosition, t->t_round);
    xfile_begin_element(xf, EStrTicks, attrs);
    {
        char *s;
        
        s = spec_tick_name(grace->rt, t->t_spec);
        
        attributes_reset(attrs);
        attributes_set_sval(attrs, AStrType, s);
        if (t->t_spec == TICKS_SPEC_NONE) {
            xfile_empty_element(xf, EStrUserticks, attrs);
        } else {
            xfile_begin_element(xf, EStrUserticks, attrs);
            {
                int i;
                for (i = 0; i < MIN2(t->nticks, MAX_TICKS); i++) {
                    attributes_reset(attrs);
                    attributes_set_sval(attrs, AStrType,
                        t->tloc[i].type == TICK_TYPE_MAJOR ? VStrMajor:VStrMinor);
                    xmlio_set_world_value(attrs,
                        AStrPosition, t->tloc[i].wtpos);
                    if (t->t_spec == TICKS_SPEC_BOTH) {
                        attributes_set_sval(attrs, AStrLabel, t->tloc[i].label);
                    }
                    xfile_empty_element(xf, EStrTick, attrs);
                }
            }
            xfile_end_element(xf, EStrUserticks);
        }

        attributes_reset(attrs);
        xmlio_set_active(attrs, t->t_flag);
        xmlio_set_side_placement(grace->rt, attrs, t->t_op);
        xmlio_set_inout_placement(grace->rt, attrs, t->t_inout);
        xfile_begin_element(xf, EStrTickmarks, attrs);
        {
            Pen pen;
            
            attributes_reset(attrs);
            attributes_set_dval(attrs, AStrSize, t->props.size);
            attributes_set_bval(attrs, AStrGridLines, t->props.gridflag);
            xfile_begin_element(xf, EStrMajor, attrs);
            {
                pen.color = t->props.color;
                pen.pattern = 1;
                xmlio_write_line_spec(xf, attrs,
                    &pen, t->props.linew, t->props.lines);
            }
            xfile_end_element(xf, EStrMajor);

            attributes_reset(attrs);
            attributes_set_dval(attrs, AStrSize, t->mprops.size);
            attributes_set_bval(attrs, AStrGridLines, t->mprops.gridflag);
            xfile_begin_element(xf, EStrMinor, attrs);
            {
                pen.color = t->mprops.color;
                pen.pattern = 1;
                xmlio_write_line_spec(xf, attrs,
                    &pen, t->mprops.linew, t->mprops.lines);
            }
            xfile_end_element(xf, EStrMinor);
        }
        xfile_end_element(xf, EStrTickmarks);

        attributes_reset(attrs);
        xmlio_set_active(attrs, t->tl_flag);
        xmlio_set_side_placement(grace->rt, attrs, t->tl_op);
        attributes_set_sval(attrs, AStrTransform, t->tl_formula);
        attributes_set_sval(attrs, AStrPrepend, t->tl_prestr);
        attributes_set_sval(attrs, AStrAppend, t->tl_appstr);
        xmlio_set_offset_placement(attrs,
            t->tl_gaptype == TYPE_AUTO, t->tl_gap.x, t->tl_gap.y);
        xmlio_set_angle(attrs, (double) t->tl_angle);
        attributes_set_ival(attrs, AStrSkip, t->tl_skip);
        attributes_set_ival(attrs, AStrStagger, t->tl_staggered);
        if (t->tl_starttype == TYPE_AUTO) {
            attributes_set_sval(attrs, AStrStart, VStrAuto);
        } else {
            xmlio_set_world_value(attrs, AStrStart, t->tl_start);
        }
        if (t->tl_stoptype == TYPE_AUTO) {
            attributes_set_sval(attrs, AStrStop, VStrAuto);
        } else {
            xmlio_set_world_value(attrs, AStrStop, t->tl_stop);
        }
        xfile_begin_element(xf, EStrTicklabels, attrs);
        {
            xmlio_write_face_spec(xf, attrs,
                t->tl_font, t->tl_charsize, t->tl_color);
            xmlio_write_format_spec(xf, attrs,
                EStrFormat, t->tl_format, t->tl_prec);
        }
        xfile_end_element(xf, EStrTicklabels);
    }
    xfile_end_element(xf, EStrTicks);

    attributes_free(attrs);
    
    return RETURN_SUCCESS;
}

int save_frame_properties(XFile *xf, frame *f)
{
    Attributes *attrs;
    if (!f) {
        return RETURN_FAILURE;
    }
    
    attrs = attributes_new();
    
    if (attrs == NULL) {
        return RETURN_FAILURE;
    }

    /* Frame */
    attributes_reset(attrs);
    xmlio_write_line_spec(xf, attrs,
        &f->outline.pen, f->outline.width, f->outline.style);
    xmlio_write_fill_spec(xf, attrs, &f->fillpen);
    
    /* Viewport */
    attributes_reset(attrs);
    attributes_set_dval(attrs, AStrXmin, f->v.xv1);
    attributes_set_dval(attrs, AStrXmax, f->v.xv2);
    attributes_set_dval(attrs, AStrYmin, f->v.yv1);
    attributes_set_dval(attrs, AStrYmax, f->v.yv2);
    xfile_empty_element(xf, EStrViewport, attrs);

    /* Legend */
    attributes_reset(attrs);
    xmlio_set_active(attrs, f->l.active);
    attributes_set_dval(attrs, AStrLength, f->l.len);
    attributes_set_dval(attrs, AStrVgap, f->l.vgap);
    attributes_set_dval(attrs, AStrHgap, f->l.hgap);
    attributes_set_bval(attrs, AStrSingleSymbol, f->l.singlesym);
    attributes_set_bval(attrs, AStrInvert, f->l.invert);
    xfile_begin_element(xf, EStrLegend, attrs);
    {
        xmlio_write_face_spec(xf, attrs,
            f->l.font, f->l.charsize, f->l.color);
        attributes_reset(attrs);
        attributes_set_ival(attrs, AStrAnchorCorner, f->l.acorner);
        xmlio_set_offset(attrs, f->l.offset.x, f->l.offset.y);
        xfile_begin_element(xf, EStrLegframe, attrs);
        {
            xmlio_write_line_spec(xf, attrs,
                &(f->l.boxline.pen), f->l.boxline.width, f->l.boxline.style);
            xmlio_write_fill_spec(xf, attrs, &(f->l.boxfillpen));
        }
        xfile_end_element(xf, EStrLegframe);
    }
    xfile_end_element(xf, EStrLegend);

    /* Title/subtitle */
    xfile_begin_element(xf, EStrTitle, NULL);
    {
        xmlio_write_face_spec(xf, attrs,
            f->labs.title.font, f->labs.title.charsize, f->labs.title.color);
        xmlio_write_text(xf, f->labs.title.s);
    }
    xfile_end_element(xf, EStrTitle);
    xfile_begin_element(xf, EStrSubtitle, NULL);
    {
        xmlio_write_face_spec(xf, attrs,
            f->labs.stitle.font, f->labs.stitle.charsize, f->labs.stitle.color);
        xmlio_write_text(xf, f->labs.stitle.s);
    }
    xfile_end_element(xf, EStrSubtitle);

    return RETURN_SUCCESS;
}

int save_graph_properties(XFile *xf, Quark *gr)
{
    Attributes *attrs;
    graph *g;
    
    if (!gr) {
        return RETURN_FAILURE;
    }
    
    g = graph_get_data(gr);
    
    attrs = attributes_new();
    
    if (attrs == NULL) {
        return RETURN_FAILURE;
    }
    
    attributes_reset(attrs);
    attributes_set_sval(attrs, AStrType, graph_types(grace->rt, g->type));
    attributes_set_bval(attrs, AStrStacked, g->stacked);
    attributes_set_dval(attrs, AStrBargap, g->bargap);
    xfile_empty_element(xf, EStrPresentationSpec, attrs);

    /* World coordinate scales */
    attributes_reset(attrs);
    xmlio_set_world_value(attrs, AStrMin, g->w.xg1);
    xmlio_set_world_value(attrs, AStrMax, g->w.xg2);
    attributes_set_sval(attrs, AStrType, scale_types(g->xscale));
    attributes_set_bval(attrs, AStrInvert, g->xinvert);
    xfile_empty_element(xf, EStrXscale, attrs);
    attributes_reset(attrs);
    xmlio_set_world_value(attrs, AStrMin, g->w.yg1);
    xmlio_set_world_value(attrs, AStrMax, g->w.yg2);
    attributes_set_sval(attrs, AStrType, scale_types(g->yscale));
    attributes_set_bval(attrs, AStrInvert, g->yinvert);
    xfile_empty_element(xf, EStrYscale, attrs);
    attributes_reset(attrs);
    attributes_set_dval(attrs, AStrNorm, g->znorm);
    xfile_empty_element(xf, EStrZscale, attrs);

    /* Locator */
    attributes_reset(attrs);
    attributes_set_ival(attrs, AStrType, g->locator.pt_type); /* FIXME: textual */
    xfile_begin_element(xf, EStrLocator, attrs);
    {
        attributes_reset(attrs);
        xmlio_set_active(attrs, g->locator.pointset);
        attributes_set_dval(attrs, AStrX, g->locator.dsx);
        attributes_set_dval(attrs, AStrY, g->locator.dsy);
        xfile_empty_element(xf, EStrFixedpoint, attrs);
        
        xmlio_write_format_spec(xf, attrs,
            EStrXformat, g->locator.fx, g->locator.px);
        xmlio_write_format_spec(xf, attrs,
            EStrYformat, g->locator.fy, g->locator.py);
    }
    xfile_end_element(xf, EStrLocator);

    /* FIXME: world stack */
    
    attributes_free(attrs);
    
    return RETURN_SUCCESS;
}

int save_set_properties(XFile *xf, Quark *pset)
{
    Attributes *attrs;
    set *p;
    
    if (!pset) {
        return RETURN_FAILURE;
    }
    
    p = (set *) pset->data;
    
    attrs = attributes_new();
    
    if (attrs == NULL) {
        return RETURN_FAILURE;
    }

    attributes_reset(attrs);
    attributes_set_ival(attrs, AStrType, p->sym.type); /* FIXME: textual */
    attributes_set_dval(attrs, AStrSize, p->sym.size);
    attributes_set_ival(attrs, AStrChar, (int) p->sym.symchar);
    xmlio_set_font_ref(attrs, p->sym.charfont);
    xfile_begin_element(xf, EStrSymbol, attrs);
    {
        xmlio_write_line_spec(xf, attrs,
            &(p->sym.line.pen), p->sym.line.width, p->sym.line.style);
        xmlio_write_fill_spec(xf, attrs, &(p->sym.fillpen));
    }
    xfile_end_element(xf, EStrSymbol);
    
    attributes_reset(attrs);
    attributes_set_ival(attrs, AStrType, p->line.type); /* FIXME: textual */
    attributes_set_ival(attrs, AStrFillType, p->line.filltype); /* FIXME: textual */
    attributes_set_sval(attrs, AStrFillRule,
        p->line.fillrule == FILLRULE_WINDING ? VStrWinding:VStrEvenodd);
    attributes_set_ival(attrs, AStrBaselineType, p->line.baseline_type); /* FIXME: textual */
    attributes_set_bval(attrs, AStrDrawBaseline, p->line.baseline);
    attributes_set_bval(attrs, AStrDrawDroplines, p->line.droplines);
    xfile_begin_element(xf, EStrLine, attrs);
    {
        xmlio_write_line_spec(xf, attrs,
            &(p->line.line.pen), p->line.line.width, p->line.line.style);
        xmlio_write_fill_spec(xf, attrs, &(p->line.fillpen));
    }
    xfile_end_element(xf, EStrLine);

    attributes_reset(attrs);
    xmlio_set_active(attrs, p->avalue.active);
    attributes_set_ival(attrs, AStrType, p->avalue.type); /* FIXME: textual */
    xmlio_set_angle(attrs, (double) p->avalue.angle);
    xmlio_set_offset(attrs, p->avalue.offset.x, p->avalue.offset.y);
    attributes_set_sval(attrs, AStrPrepend, p->avalue.prestr);
    attributes_set_sval(attrs, AStrAppend, p->avalue.appstr);
    xfile_begin_element(xf, EStrAnnotation, attrs);
    {
        xmlio_write_face_spec(xf, attrs,
            p->avalue.font, p->avalue.size, p->avalue.color);
        xmlio_write_format_spec(xf, attrs,
            EStrFormat, p->avalue.format, p->avalue.prec);
    }
    xfile_end_element(xf, EStrAnnotation);

    attributes_reset(attrs);
    xmlio_set_active(attrs, p->errbar.active);
    xmlio_set_side_placement(grace->rt, attrs, p->errbar.ptype);
    xfile_begin_element(xf, EStrErrorbar, attrs);
    {
        attributes_reset(attrs);
        attributes_set_dval(attrs, AStrSize, p->errbar.barsize);
        xfile_begin_element(xf, EStrBarline, attrs);
        {
            xmlio_write_line_spec(xf, attrs,
                &(p->errbar.pen), p->errbar.linew, p->errbar.lines);
        }
        xfile_end_element(xf, EStrBarline);
        
        attributes_reset(attrs);
        attributes_set_bval(attrs, AStrArrowClip, p->errbar.arrow_clip);
        attributes_set_dval(attrs, AStrClipLength, p->errbar.cliplen);
        xfile_begin_element(xf, EStrRiserline, attrs);
        {
            xmlio_write_line_spec(xf, attrs,
                &(p->errbar.pen), p->errbar.riser_linew, p->errbar.riser_lines);
        }
        xfile_end_element(xf, EStrRiserline);
    }
    xfile_end_element(xf, EStrErrorbar);

    xfile_begin_element(xf, EStrLegendEntry, NULL);
    xmlio_write_text(xf, p->legstr);
    xfile_end_element(xf, EStrLegendEntry);

    attributes_free(attrs);
    
    return RETURN_SUCCESS;
}

static int save_dataset(XFile *xf, Quark *pset)
{
    Attributes *attrs;
    int i, nc;
    Dataset *data;

    if (!pset) {
        return RETURN_FAILURE;
    }
    
    data = set_get_dataset(pset);
    
    attrs = attributes_new();
    
    if (attrs == NULL) {
        return RETURN_FAILURE;
    }

    attributes_set_ival(attrs, AStrCols, data->ncols);
    attributes_set_ival(attrs, AStrRows, data->len);
    attributes_set_sval(attrs, AStrComment, data->comment);
    if (data->hotlink) {
        attributes_set_sval(attrs, AStrHotfile, data->hotfile);
        /* FIXME: hotsrc */
    }
    xfile_begin_element(xf, EStrDataset, attrs);

    for (i = 0; i < data->len; i++) {
        attributes_reset(attrs);
        for (nc = 0; nc < data->ncols; nc++) {
            if (data->ex[nc]) {
                xmlio_set_world_value(attrs,
                    dataset_colname(nc), data->ex[nc][i]);
            }
        }
        if (data->s) {
            attributes_set_sval(attrs, "s", data->s[i]);
        }
        xfile_empty_element(xf, EStrRow, attrs);
    }
    
    xfile_end_element(xf, EStrDataset);

    attributes_free(attrs);

    return RETURN_SUCCESS;
}

int save_regions(XFile *xf)
{
    Attributes *attrs;

    if (!xf) {
        return RETURN_FAILURE;
    }
    
    attrs = attributes_new();
    
    if (attrs == NULL) {
        return RETURN_FAILURE;
    }

    /* FIXME: regions */

    attributes_free(attrs);

    return RETURN_SUCCESS;
}
    
int save_preferences(XFile *xf)
{
    Attributes *attrs;

    if (!xf) {
        return RETURN_FAILURE;
    }
    
    attrs = attributes_new();
    
    if (attrs == NULL) {
        return RETURN_FAILURE;
    }
    
    attributes_free(attrs);

    return RETURN_SUCCESS;
}

static int project_save_hook(Quark *q,
    void *udata, QTraverseClosure *closure)
{
    XFile *xf = (XFile *) udata;
    Project *pr;
    frame *f;
    set *p;
    tickmarks *t;
    DObject *o;
    Attributes *attrs;

    if (!q) {
        return RETURN_FAILURE;
    }

    attrs = attributes_new();
    if (attrs == NULL) {
        return RETURN_FAILURE;
    }
    
    switch (q->fid) {
    case QFlavorProject:
        pr = project_get_data(q);
        
        xfile_comment(xf, "Description");
        xfile_begin_element(xf, EStrDescription, NULL);
        {
            xmlio_write_text(xf, pr->description);
        }
        xfile_end_element(xf, EStrDescription);

        xfile_comment(xf, "Definitions");
        xfile_begin_element(xf, EStrDefinitions, NULL);
        {
            xfile_comment(xf, "Color map");
            save_colormap(xf, grace->rt->canvas);
            xfile_comment(xf, "Font map");
            save_fontmap(xf, pr);
        }
        xfile_end_element(xf, EStrDefinitions);

        xfile_comment(xf, "Page properties");
        attributes_reset(attrs);
        attributes_set_ival(attrs, AStrWidth, pr->page_wpp);
        attributes_set_ival(attrs, AStrHeight, pr->page_hpp);
        attributes_set_bval(attrs, AStrFill, pr->bgfill);
        xmlio_set_color_ref(attrs, pr->bgcolor);
        xfile_empty_element(xf, EStrPage, attrs);

        xfile_comment(xf, "Data formats");
        xfile_begin_element(xf, EStrDataFormats, NULL);
        {
            attributes_reset(attrs);
            xmlio_set_world_value(attrs, AStrReference, get_ref_date());
            attributes_set_bval(attrs, AStrWrap, two_digits_years_allowed());
            attributes_set_ival(attrs, AStrWrapYear, get_wrap_year());
            xfile_empty_element(xf, EStrDates, attrs);

            attributes_reset(attrs);
            attributes_set_sval(attrs, AStrFormat, pr->sformat);
            xfile_empty_element(xf, EStrWorld, attrs);
        }
        xfile_end_element(xf, EStrDataFormats);

        save_regions(xf);

        save_preferences(xf);
        break;
    case QFlavorFrame:
        if (!closure->pass2) {
            f = frame_get_data(q);

            attributes_set_ival(attrs, AStrType, f->type); /* FIXME: textual */
            xfile_begin_element(xf, EStrFrame, attrs);
            save_frame_properties(xf, f);
            
            closure->pass2 = TRUE;
        } else {
            xfile_end_element(xf, EStrFrame);
        }
        break;
    case QFlavorGraph:
        if (!closure->pass2) {
            attributes_set_sval(attrs, AStrId, QIDSTR(q));
            xmlio_set_active(attrs, !is_graph_hidden(q));

            xfile_begin_element(xf, EStrGraph, attrs);
            save_graph_properties(xf, q);
    
            closure->pass2 = TRUE;
        } else {
            xfile_end_element(xf, EStrGraph);
        }
        break;
    case QFlavorSet:
        p = set_get_data(q);
        
        attributes_set_sval(attrs, AStrId, QIDSTR(q));
        xmlio_set_active(attrs, !(p->hidden));
        attributes_set_sval(attrs, AStrType, set_types(grace->rt, p->type));
        attributes_set_ival(attrs, AStrSkip, p->symskip);
        xfile_begin_element(xf, EStrSet, attrs);
        {
            save_set_properties(xf, q);
            save_dataset(xf, q);
        }
        xfile_end_element(xf, EStrSet);
        
        break;
    case QFlavorAxis:
        t = axis_get_data(q);
        
        attributes_set_sval(attrs, AStrId, QIDSTR(q));
        
        attributes_set_sval(attrs, AStrType, axis_is_x(q) ? "x":"y");
        xmlio_set_active(attrs, t->active);
        xfile_begin_element(xf, EStrAxis, attrs);
        save_axis_properties(xf, t);
        xfile_end_element(xf, EStrAxis);
        
        break;
    case QFlavorDObject:
        o = object_get_data(q);

        xmlio_set_active(attrs, o->active);
        xmlio_set_angle(attrs, o->angle);
        xmlio_set_offset(attrs, o->offset.x, o->offset.y);
        xfile_begin_element(xf, EStrObject, attrs);
        {
            char buf[32];
            xmlio_write_location(xf, attrs, o->loctype, o->ap.x, o->ap.y);
            xmlio_write_line_spec(xf, attrs,
                &(o->line.pen), o->line.width, o->line.style);
            xmlio_write_fill_spec(xf, attrs, &(o->fillpen));
            attributes_reset(attrs);
            switch (o->type) {
            case DO_LINE:
                {
                    DOLineData *l = (DOLineData *) o->odata;
                    attributes_set_dval(attrs, AStrLength, l->length);
                    attributes_set_ival(attrs, AStrArrowsAt, l->arrow_end); /* FIXME: textual */
                }
                break;
            case DO_BOX:
                {
                    DOBoxData *b = (DOBoxData *) o->odata;
                    attributes_set_dval(attrs, AStrWidth, b->width);
                    attributes_set_dval(attrs, AStrHeight, b->height);
                }
                break;
            case DO_ARC:
                {
                    DOArcData *a = (DOArcData *) o->odata;
                    attributes_set_dval(attrs, AStrWidth, a->width);
                    attributes_set_dval(attrs, AStrHeight, a->height);
                    attributes_set_dval(attrs, AStrStartAngle, a->angle1);
                    attributes_set_dval(attrs, AStrExtentAngle, a->angle2);
                    attributes_set_ival(attrs, AStrFillMode, a->fillmode); /* FIXME: textual */
                }
                break;
            case DO_STRING:
                {
                    DOStringData *s = (DOStringData *) o->odata;
                    xmlio_set_font_ref(attrs, s->font);
                    attributes_set_dval(attrs, AStrCharSize, s->size);
                    attributes_set_ival(attrs, AStrJustification, s->just); /* FIXME: textual */
                }
                break;
            case DO_NONE:
                break;
            }
            sprintf(buf, "%s-data", object_types(o->type));
            if (o->type == DO_STRING) {
                xfile_begin_element(xf, buf, attrs);
                {
                    DOStringData *s = (DOStringData *) o->odata;
                    xmlio_write_text(xf, s->s);
                }
                xfile_end_element(xf, buf);
            } else
            if (o->type == DO_LINE) {
                xfile_begin_element(xf, buf, attrs);
                {
                    DOLineData *l = (DOLineData *) o->odata;
                    xmlio_write_arrow(xf, attrs, &l->arrow);
                }
                xfile_end_element(xf, buf);
            } else {
                xfile_empty_element(xf, buf, attrs);
            }
        }
        xfile_end_element(xf, EStrObject);

        break;
    }
    
    attributes_free(attrs);
        
    return TRUE;
}


int save_project(char *fn)
{
    XFile *xf;
    Attributes *attrs;
    Project *pr = (Project *) grace->project->data;
    
    xf = xfile_new(fn);
    attrs = attributes_new();
    
    if (xf == NULL || attrs == NULL) {
        return RETURN_FAILURE;
    }
    
    xfile_set_ns(xf, GRACE_NS_PREFIX, GRACE_NS_URI, FALSE);
    
    attributes_reset(attrs);
    attributes_set_ival(attrs, AStrVersion, bi_version_id());
    xfile_begin(xf, FALSE, NULL, "grace.dtd", EStrGrace, attrs);

    quark_traverse(grace->project, project_save_hook, xf);
        
    attributes_free(attrs);
    
    xfile_end(xf);
    xfile_free(xf);
    
    pr->docname = copy_string(pr->docname, fn);

    clear_dirtystate();
    
    return RETURN_SUCCESS;
}


/*
save_prefs()
{
    xfile_comment(xf, "Defaults");
    xfile_begin_element(xf, "defaults", NULL);
    {
        Pen pen;
        defaults grdefaults = grace->rt->grdefaults;
        pen.color = grdefaults.color;
        pen.pattern = grdefaults.pattern;
        xmlio_write_line_spec(xf, attrs,
            &pen, grdefaults.linew, grdefaults.lines);
        xmlio_write_fill_spec(xf, attrs, &pen);
        xmlio_write_face_spec(xf, attrs,
            grdefaults.font, grdefaults.charsize, grdefaults.color);
    }
    xfile_end_element(xf, "defaults");

    xfile_comment(xf, "Preferences");
    xfile_begin_element(xf, "preferences", attrs);
    {
        (int) rint(grace->rt->scrollper * 100);
        (int) rint(grace->rt->shexper * 100);
        grace->rt->scrolling_islinked ? "on" : "off";
    }
    xfile_end_element(xf, "preferences");

}
*/
