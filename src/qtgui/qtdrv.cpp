/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 *
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 *
 * Copyright (c) 1996-2008 Grace Development Team
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
 * Qt driver
 */

#include <math.h>

#define MIN2(a, b) (((a) < (b)) ? a : b)
#define MAX2(a, b) (((a) > (b)) ? a : b)

#define CANVAS_BACKEND_API
extern "C" {
  #include <grace/canvas.h>
}

#include <QPainter>
#include <QBitmap>

typedef struct {
    double x;
    double y;
} XPoint;

typedef struct {
    QPainter *painter;
    QImage *pixmap;

    unsigned int height, width, page_scale;

    int color;
    int bgcolor;
    int patno;
    int linewidth;
    int linestyle;
    int fillrule;
    int arcfillmode;
    int linecap;
    int linejoin;
} Qt_data;

static Qt_data *qt_data_new(void)
{
    Qt_data *data = new Qt_data;

    if (data) {
        memset(data, 0, sizeof(Qt_data));
    }

    return data;
}

static void qt_data_free(void *data)
{
    Qt_data *qtdata = (Qt_data *) data;

    if (qtdata) {
        delete qtdata;
    }
}

static void VPoint2XPoint(const Qt_data *qtdata, const VPoint *vp, XPoint *xp)
{
    xp->x = qtdata->page_scale*vp->x;
    xp->y = qtdata->height - qtdata->page_scale*vp->y;
}

static QColor Color2QColor(const Canvas *canvas, const int color)
{
    RGB rgb;
    get_rgb(canvas, color, &rgb);
    return QColor(rgb.red, rgb.green, rgb.blue);
}

// TODO: folowing is defined in xprotos.h
typedef void *Pixmap;
typedef struct {
    Pixmap pixmap;
} X11stream;

static int qt_initgraphics(const Canvas *canvas, void *data,
    const CanvasStats *cstats)
{
    Qt_data *qtdata = (Qt_data *) data;

    Page_geometry *pg = get_page_geometry(canvas);

    qtdata->width = pg->width;
    qtdata->height = pg->height;
    qtdata->page_scale = MIN2(pg->width, pg->height);

    X11stream *xstream = (X11stream *) canvas_get_prstream(canvas);
    qtdata->pixmap = (QImage *) xstream->pixmap;
    qtdata->painter = new QPainter(qtdata->pixmap);

    /* init settings specific to Qt driver */
    qtdata->color       = BAD_COLOR;
    qtdata->bgcolor     = BAD_COLOR;
    qtdata->patno       = -1;
    qtdata->linewidth   = -1;
    qtdata->linestyle   = -1;
    qtdata->fillrule    = -1;
    qtdata->arcfillmode = -1;
    qtdata->linecap     = -1;
    qtdata->linejoin    = -1;

    return RETURN_SUCCESS;
}

static void qt_setpen(const Canvas *canvas, Qt_data *qtdata)
{
    int fg;
    Pen pen;

    getpen(canvas, &pen);
    fg = pen.color;

    QPen qpen(Color2QColor(canvas, fg));
    qpen.setStyle(Qt::SolidLine);
    qtdata->painter->setPen(qpen);
}

static void qt_setdrawbrush(const Canvas *canvas, Qt_data *qtdata)
{
    unsigned int iw;
    int style;
    int lc, lj;
    int i, darr_len;

    qt_setpen(canvas, qtdata);

    iw = (unsigned int) rint(getlinewidth(canvas) * qtdata->page_scale);
    if (iw == 1) {
        iw = 0;
    }
    style = getlinestyle(canvas);
    lc = getlinecap(canvas);
    lj = getlinejoin(canvas);

    switch (lc) {
    case LINECAP_BUTT:
        lc = (int) Qt::FlatCap;
        break;
    case LINECAP_ROUND:
        lc = (int) Qt::RoundCap;
        break;
    case LINECAP_PROJ:
        lc = (int) Qt::SquareCap;
        break;
    }

    switch (lj) {
    case LINEJOIN_MITER:
        lj = (int) Qt::MiterJoin;
        break;
    case LINEJOIN_ROUND:
        lj = (int) Qt::RoundJoin;
        break;
    case LINEJOIN_BEVEL:
        lj = (int) Qt::BevelJoin;
        break;
    }

    //    if (iw != qtdata->linewidth || style != qtdata->linestyle ||
    //        lc != qtdata->linecap   || lj    != qtdata->linejoin) {
    if (style > 1) {
        LineStyle *linestyle = canvas_get_linestyle(canvas, style);
        darr_len = linestyle->length;
        QVector<qreal> dashes(darr_len);
        for (i = 0; i < darr_len; i++) {
            dashes[i] = linestyle->array[i];
        }
        QPen pen = qtdata->painter->pen();

        pen.setWidth(iw);
        pen.setCapStyle((Qt::PenCapStyle) lc);
        pen.setJoinStyle((Qt::PenJoinStyle) lj);
        pen.setDashPattern(dashes);

        qtdata->painter->setPen(pen);
    } else if (style == 1) {
        QPen pen = qtdata->painter->pen();

        pen.setWidth(iw);
        pen.setStyle(Qt::SolidLine);
        pen.setCapStyle((Qt::PenCapStyle) lc);
        pen.setJoinStyle((Qt::PenJoinStyle) lj);

        qtdata->painter->setPen(pen);
    }

    //        qtdata->linestyle = style;
    //        qtdata->linewidth = iw;
    //        qtdata->linecap   = lc;
    //        qtdata->linejoin  = lj;
    //    }
    return;
}

static void qt_drawpixel(const Canvas *canvas, void *data,
    const VPoint *vp)
{
    Qt_data *qtdata = (Qt_data *) data;
    XPoint xp;

    VPoint2XPoint(qtdata, vp, &xp);
    qt_setpen(canvas, qtdata);
    qtdata->painter->drawPoint(QPointF(xp.x, xp.y));
}

static void qt_drawpolyline(const Canvas *canvas, void *data,
    const VPoint *vps, int n, int mode)
{
    Qt_data *qtdata = (Qt_data *) data;
    int i, xn = n;
    XPoint *p;

    if (mode == POLYLINE_CLOSED) {
        xn++;
    }

    p = (XPoint *) xmalloc(xn * sizeof(XPoint));
    if (p == NULL) {
        return;
    }

    QPointF points[xn];
    for (i = 0; i < n; i++) {
        VPoint2XPoint(qtdata, &vps[i], &p[i]);
        points[i] = QPointF(p[i].x, p[i].y);
    }
    if (mode == POLYLINE_CLOSED) {
        points[n] = QPointF(p[0].x, p[0].y);
    }

    qt_setdrawbrush(canvas, qtdata);

    qtdata->painter->drawPolyline(points, xn);

    xfree(p);
}

static void qt_setfillpen(const Canvas *canvas, Qt_data *qtdata)
{
    int fg, p;
    Pen pen;

    getpen(canvas, &pen);
    fg = pen.color;
    p = pen.pattern;

    if (p == 0) { /* TODO: transparency !!!*/
        return;
    } else if (p == 1) {
        qtdata->painter->setPen(Qt::NoPen);
        qtdata->painter->setBrush(QBrush(Color2QColor(canvas, fg)));
    } else {
        qtdata->painter->setPen(Qt::NoPen);

        Pattern *pat = canvas_get_pattern(canvas, p);
        QBitmap bitmap = QBitmap::fromData(QSize(pat->width, pat->height),
                pat->bits, QImage::Format_MonoLSB);
        QBrush brush(Color2QColor(canvas, fg));
        brush.setTexture(bitmap);
        qtdata->painter->setBrush(brush);
    }
}

static void qt_fillpolygon(const Canvas *canvas, void *data,
    const VPoint *vps, int nc)
{
    Qt_data *qtdata = (Qt_data *) data;
    int i;
    XPoint *p;

    p = (XPoint *) xmalloc(nc * sizeof(XPoint));
    if (p == NULL) {
        return;
    }

    QPointF points[nc];
    for (i = 0; i < nc; i++) {
        VPoint2XPoint(qtdata, &vps[i], &p[i]);
        points[i] = QPointF(p[i].x, p[i].y);
    }

    qt_setfillpen(canvas, qtdata);

    Qt::FillRule rule;
    if (getfillrule(canvas) == FILLRULE_WINDING) {
        rule = Qt::WindingFill;
    } else {
        rule = Qt::OddEvenFill;
    }

    qtdata->painter->drawPolygon(points, nc, rule);

    xfree(p);
}

static void qt_drawarc(const Canvas *canvas, void *data,
    const VPoint *vp1, const VPoint *vp2, double a1, double a2)
{
    Qt_data *qtdata = (Qt_data *) data;
    XPoint xp1, xp2;

    VPoint2XPoint(qtdata, vp1, &xp1);
    VPoint2XPoint(qtdata, vp2, &xp2);

    qt_setdrawbrush(canvas, qtdata);

    if (xp1.x != xp2.x || xp1.y != xp2.y) {
        double x = MIN2(xp1.x, xp2.x);
        double y = MIN2(xp1.y, xp2.y);
        double width = fabs(xp2.x - xp1.x);
        double height = fabs(xp2.y - xp1.y);
        int angle1 = (int) rint(16 * a1);
        int angle2 = (int) rint(16 * a2);

        qtdata->painter->drawArc(QRectF(x, y, width, height), angle1, angle2);
    } else { /* zero radius */
        qtdata->painter->drawPoint(QPointF(xp1.x, xp1.y));
    }
}

static void qt_fillarc(const Canvas *canvas, void *data,
    const VPoint *vp1, const VPoint *vp2, double a1, double a2, int mode)
{
    Qt_data *qtdata = (Qt_data *) data;
    XPoint xp1, xp2;

    VPoint2XPoint(qtdata, vp1, &xp1);
    VPoint2XPoint(qtdata, vp2, &xp2);

    qt_setfillpen(canvas, qtdata);

    if (xp1.x != xp2.x || xp1.y != xp2.y) {
        double x = MIN2(xp1.x, xp2.x);
        double y = MIN2(xp1.y, xp2.y);
        double width = fabs(xp2.x - xp1.x);
        double height = fabs(xp2.y - xp1.y);
        int angle1 = (int) rint(16 * a1);
        int angle2 = (int) rint(16 * a2);

        if (mode == ARCCLOSURE_CHORD) {
            qtdata->painter->drawChord(QRectF(x, y, width, height), angle1, angle2);
        } else {
            qtdata->painter->drawPie(QRectF(x, y, width, height), angle1, angle2);
        }
    } else { /* zero radius */
        qtdata->painter->drawPoint(QPointF(xp1.x, xp1.y));
    }
}

void qt_putpixmap(const Canvas *canvas, void *data,
    const VPoint *vp, const CPixmap *pm)
{
    Qt_data *qtdata = (Qt_data *) data;
    int cindex, bg;
    int color;

    int i, k, j;
    long paddedW;

    XPoint xp;
    double x, y;

    bg = getbgcolor(canvas);

    VPoint2XPoint(qtdata, vp, &xp);

    y = xp.y;
    if (pm->bpp == 1) {
        color = getcolor(canvas);
        paddedW = PADBITS(pm->width, pm->pad);
        for (k = 0; k < pm->height; k++) {
            x = xp.x;
            y++;
            for (j = 0; j < paddedW / pm->pad; j++) {
                for (i = 0; i < pm->pad && j * pm->pad + i < pm->width; i++) {
                    x++;
                    if (bin_dump(&(pm->bits)[k * paddedW / pm->pad + j], i, pm->pad)) {
                        QPen qpen(Color2QColor(canvas, color));
                        qtdata->painter->setPen(qpen);
                        qtdata->painter->drawPoint(QPointF(x, y));
                    } else {
                        if (pm->type == PIXMAP_OPAQUE) {
                            QPen qpen(Color2QColor(canvas, bg));
                            qtdata->painter->setPen(qpen);
                            qtdata->painter->drawPoint(QPointF(x, y));
                        }
                    }
                }
            }
        }
    } else {
        unsigned int *cptr = (unsigned int *) pm->bits;
        for (k = 0; k < pm->height; k++) {
            x = xp.x;
            y++;
            for (j = 0; j < pm->width; j++) {
                x++;
                cindex = cptr[k * pm->width + j];
                if (cindex != bg || pm->type == PIXMAP_OPAQUE) {
                    color = cindex;
                    QPen qpen(Color2QColor(canvas, color));
                    qtdata->painter->setPen(qpen);
                    qtdata->painter->drawPoint(QPointF(x, y));
                }
            }
        }
    }
}

static void qt_leavegraphics(const Canvas *canvas, void *data,
    const CanvasStats *cstats)
{
    Qt_data *qtdata = (Qt_data *) data;
    delete qtdata->painter;
}

int register_qt_drv(Canvas *canvas)
{
    Device_entry *d;
    Qt_data *data;

    data = qt_data_new();
    if (!data) {
        return -1;
    }

    d = device_new("Qt", DEVICE_TERM, FALSE, data, qt_data_free);
    if (!d) {
        return -1;
    }

    device_set_procs(d,
        qt_initgraphics,
        qt_leavegraphics,
        NULL,
        NULL,
        qt_drawpixel,
        qt_drawpolyline,
        qt_fillpolygon,
        qt_drawarc,
        qt_fillarc,
        qt_putpixmap,
        NULL);

    return register_device(canvas, d);
}
