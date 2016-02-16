#include "sockswindow.hh"
#include "application.hh"
#include <QImage>
#include <QPushButton>
#include <QHBoxLayout>
#include <QCloseEvent>


SocksWindow::SocksWindow(Application &app, const NodeItem &node, QWidget *parent)
  : QWidget(parent), _application(app), _socks(app.dht(), node)
{
  _info = new QLabel(tr("Started SOCKS proxy service."));
  if (!_socks.isListening()) {
    _info->setText(tr("Error while starting SOCKS proxy service."));
  }
  _connectionCount = new QLabel(tr("Connections: %1").arg(_socks.connectionCount()));

  QLabel *icon = new QLabel();
  icon->setPixmap(
        QPixmap::fromImage(QImage("://icons/world.png")));
  QPushButton *stop = new QPushButton(QIcon("://icons/circle-x.png"), tr("stop"));

  QHBoxLayout *layout = new QHBoxLayout();
  layout->addWidget(icon);
  QVBoxLayout *infobox = new QVBoxLayout();
  infobox->addWidget(_info);
  infobox->addWidget(_connectionCount);
  layout->addLayout(infobox);
  layout->addWidget(stop);
  setLayout(layout);

  connect(stop, SIGNAL(clicked()), this, SLOT(close()));
  connect(&_socks, SIGNAL(connectionCountChanged(size_t)),
          this, SLOT(_onConnectionCountChanged(size_t)));
}

void
SocksWindow::closeEvent(QCloseEvent *evt) {
  evt->accept();
  this->deleteLater();
}

void
SocksWindow::_onConnectionCountChanged(size_t count) {
  _connectionCount->setText(tr("Connections: %1").arg(_socks.connectionCount()));
}
