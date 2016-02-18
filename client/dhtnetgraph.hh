#ifndef DHTNETGRAPH_H
#define DHTNETGRAPH_H

#include <QWidget>
#include "lib/node.hh"


class DHTNetGraph : public QWidget
{
  Q_OBJECT
public:
  explicit DHTNetGraph(QWidget *parent = 0);

  void update(const QList< QPair<double, bool> > &nodes);

protected:
  virtual void paintEvent(QPaintEvent *evt);

protected:
  QList< QPair<double, bool> > _nodes;
};

#endif // DHTNETGRAPH_H
