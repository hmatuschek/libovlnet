#include "dhtnetgraph.hh"
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QColor>

DHTNetGraph::DHTNetGraph(QWidget *parent)
  : QWidget(parent)
{
  setMinimumWidth(300);
  setMinimumHeight(75);

  update(QList< QPair<double, bool> >());
}

void
DHTNetGraph::update(const QList<QPair<double, bool> > &nodes) {
  _nodes = nodes;
  QWidget::update();
}

void
DHTNetGraph::paintEvent(QPaintEvent *evt) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(rect(), "white");

  int margin = 10;

  // Draw axis
  painter.setPen(QPen(Qt::black, 3));
  painter.drawLine(margin, height()/2, width()-2*margin, height()/2);
  painter.setBrush(QBrush(Qt::black));
  painter.drawEllipse(QPoint(margin,height()/2), 5,5);

  // Draw nodes
  painter.setPen(QPen(Qt::black, 2));
  QList< QPair<double, bool> >::iterator item = _nodes.begin();
  for (; item != _nodes.end(); item++) {
    if (item->second) {
      painter.setBrush(QBrush(Qt::blue));
    } else {
      painter.setBrush(QBrush(Qt::gray));
    }
    int x = margin+(width()-2*margin)*item->first;
    painter.drawEllipse(QPoint(x,height()/2), 5,5);
  }
}
