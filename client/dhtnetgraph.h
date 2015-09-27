#ifndef DHTNETGRAPH_H
#define DHTNETGRAPH_H

#include <QWidget>
#include "lib/dht.h"


class DHTNetGraph : public QWidget
{
  Q_OBJECT
public:
  explicit DHTNetGraph(QWidget *parent = 0);

  void update(const QList< QPair<double, NodeItem> > &nodes);

protected:
  virtual void paintEvent(QPaintEvent *evt);

protected:
  QList< QPair<double, NodeItem> > _nodes;
};

#endif // DHTNETGRAPH_H