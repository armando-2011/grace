#include <QPainter>
#include <QFont>

#include "canvaswidget.h"
#include <stdio.h>
#include <grace/grace.h> 

CanvasWidget::CanvasWidget(QWidget *parent) : QWidget(parent)
{
/*  static const QPointF points[3] = {
    QPointF(10.0, 80.0),
    QPointF(20.0, 10.0),
    QPointF(80.0, 30.0),
  };

  QPainter painter(&qtstream);
  painter.drawPolyline(points, 3);*/

  printf("Hello world\n");
//  canvas_set_prstream(grace_get_canvas(m_gapp->grace), &qtstream);
  //select_device(grace_get_canvas(m_gapp->grace), m_gapp->rt->tdevice);
  //gproject_render(gp);
  
  printf("Hello world2\n");
}

void CanvasWidget::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  qtstream.play(&painter);
}

