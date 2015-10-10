#include "sockswindow.h"
#include <QImage>
#include <QPushButton>
#include <QHBoxLayout>
#include <QCloseEvent>


SocksWindow::SocksWindow(Application &app, const NodeItem &node, QWidget *parent)
  : QWidget(parent), _application(app), _socks(app, node)
{
  _info = new QLabel(tr("Started socks service."));
  QLabel *icon = new QLabel();
  icon->setPixmap(
        QPixmap::fromImage(QImage("://icons/world.png")));
  QPushButton *stop = new QPushButton(QIcon("://icons/circle-x.png"), tr("stop"));

  QHBoxLayout *layout = new QHBoxLayout();
  layout->addWidget(icon);
  layout->addWidget(_info);
  layout->addWidget(stop);
  setLayout(layout);

  connect(stop, SIGNAL(clicked()), this, SLOT(close()));
}

void
SocksWindow::closeEvent(QCloseEvent *evt) {
  evt->accept();
  this->deleteLater();
}
