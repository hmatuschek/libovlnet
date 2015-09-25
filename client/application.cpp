#include "application.h"
#include "dhtstatusview.h"

#include <QMenu>

Application::Application(int argc, char *argv[])
  : QApplication(argc, argv), _dht(0), _status(0)
{
  _dht    = new DHT(Identifier());
  _status = new DHTStatus(_dht);

  _showStatus = new QAction(tr("Show status ..."), this);
  _quit = new QAction(tr("Quit"), this);

  QMenu *ctx = new QMenu();
  ctx->addAction(_showStatus);
  ctx->addSeparator();
  ctx->addAction(_quit);

  _trayIcon = new QSystemTrayIcon();
  _trayIcon->setContextMenu(ctx);
  _trayIcon->show();

  QObject::connect(_showStatus, SIGNAL(triggered()), this, SLOT(onShowStatus()));
  QObject::connect(_quit, SIGNAL(triggered()), this, SLOT(onQuit()));
}


void
Application::onShowStatus() {
  (new DHTStatusView(_status))->show();
}

void
Application::onQuit() {
  quit();
}

