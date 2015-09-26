#include "application.h"
#include "dhtstatusview.h"

#include <QMenu>
#include <QInputDialog>
#include <QHostAddress>
#include <QHostInfo>
#include <QMessageBox>
#include <QString>
#include <QDir>

Application::Application(int argc, char *argv[])
  : QApplication(argc, argv), _identity(0), _dht(0), _status(0)
{
  setQuitOnLastWindowClosed(false);
  // Try to load identity from file
  QDir vlfDir = QDir::home();
  if (! vlfDir.cd(".vlf")) { vlfDir.mkdir(".vlf"); vlfDir.cd(".vlf"); }
  QFile idFile(vlfDir.canonicalPath()+"/identity.pem");
  if (!idFile.exists()) {
    qDebug() << "No identity found -> create one.";
    _identity = Identity::newIdentity(idFile);
  } else {
    qDebug() << "Load identity from" << idFile.fileName();
    _identity = Identity::load(idFile);
  }

  if (_identity) {
    _dht = new DHT(_identity->id());
  } else {
    qDebug() << "Error while loading or creating my identity.";
  }

  _status = new DHTStatus(_dht);

  _showStatus = new QAction(QIcon("://settings.png"), tr("Show status ..."), this);
  _bootstrap  = new QAction(tr("Bootstrap..."), this);
  _quit = new QAction(QIcon("://quit.png"), tr("Quit"), this);

  QMenu *ctx = new QMenu();
  ctx->addAction(_bootstrap);
  ctx->addAction(_showStatus);
  ctx->addSeparator();
  ctx->addAction(_quit);

  _trayIcon = new QSystemTrayIcon();
  _trayIcon->setIcon(QIcon("://icon.png"));
  _trayIcon->setContextMenu(ctx);
  _trayIcon->show();

  QObject::connect(_bootstrap, SIGNAL(triggered()), this, SLOT(onBootstrap()));
  QObject::connect(_showStatus, SIGNAL(triggered()), this, SLOT(onShowStatus()));
  QObject::connect(_quit, SIGNAL(triggered()), this, SLOT(onQuit()));
}


void
Application::onBootstrap() {
  while (true) {
    QString host = QInputDialog::getText(0, tr("Bootstrap from..."), tr("Host and optional port:"));
    if (0 == host.size()) { return; }

    // Extract port
    uint16_t port = 7741;
    if (host.contains(':')) {
      QStringList parts = host.split(':');
      if (2 != parts.size()) {
        QMessageBox::critical(0, tr("Invalid hostname or port."),
                              tr("Invalid hostname or port format: {1}").arg(host));
        continue;
      }
      host = parts.front();
      port = parts.back().toUInt();
    }
    QHostInfo host_info = QHostInfo::fromName(host);
    if (QHostInfo::NoError != host_info.error()) {
      QMessageBox::critical(0, tr("Can not resolve host name."),
                            tr("Can not resolve host name: {1}").arg(host));
      continue;
    }

    _dht->ping(host_info.addresses().front(), port);
    return;
  }
}

void
Application::onShowStatus() {
  (new DHTStatusView(_status))->show();
}

void
Application::onQuit() {
  quit();
}

