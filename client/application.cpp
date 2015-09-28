#include "application.h"
#include "dhtstatusview.h"
#include "searchdialog.h"
#include "buddylistview.h"
#include "chatwindow.h"

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
  QString idFile(vlfDir.canonicalPath()+"/identity.pem");
  if (!QFile::exists(idFile)) {
    qDebug() << "No identity found -> create one.";
    _identity = Identity::newIdentity(idFile);
  } else {
    qDebug() << "Load identity from" << idFile;
    _identity = Identity::load(idFile);
  }

  if (_identity) {
    _dht = new DHT(_identity->id(), this);
  } else {
    qDebug() << "Error while loading or creating my identity.";
  }

  _status = new DHTStatus(_dht);
  _buddies = new BuddyList(vlfDir.canonicalPath()+"/buddies.json");

  _search      = new QAction(QIcon("://search.png"),    tr("Search..."), this);
  _showBuddies = new QAction(QIcon("://people.png"),    tr("Contacts..."), this);
  _bootstrap   = new QAction(QIcon("://bootstrap.png"), tr("Bootstrap..."), this);
  _showStatus  = new QAction(QIcon("://settings.png"),  tr("Show status ..."), this);
  _quit        = new QAction(QIcon("://quit.png"),      tr("Quit"), this);

  QMenu *ctx = new QMenu();
  ctx->addAction(_search);
  ctx->addAction(_showBuddies);
  ctx->addSeparator();
  ctx->addAction(_bootstrap);
  ctx->addAction(_showStatus);
  ctx->addSeparator();
  ctx->addAction(_quit);

  _trayIcon = new QSystemTrayIcon();
  _trayIcon->setIcon(QIcon("://icon.png"));
  _trayIcon->setContextMenu(ctx);
  _trayIcon->show();

  QObject::connect(_dht, SIGNAL(nodeFound(NodeItem)), this, SLOT(onNodeFound(NodeItem)));
  QObject::connect(_dht, SIGNAL(nodeNotFound(Identifier,QList<NodeItem>)),
                   this, SLOT(onNodeNotFound(Identifier,QList<NodeItem>)));

  QObject::connect(_bootstrap, SIGNAL(triggered()), this, SLOT(onBootstrap()));
  QObject::connect(_showBuddies, SIGNAL(triggered()), this, SLOT(onShowBuddies()));
  QObject::connect(_search, SIGNAL(triggered()), this, SLOT(onSearch()));
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
Application::onSearch() {
  (new SearchDialog(_dht, _buddies))->show();
}

void
Application::onShowBuddies() {
  (new BuddyListView(*this,  _buddies))->show();
}

void
Application::onShowStatus() {
  (new DHTStatusView(_status))->show();
}

void
Application::onQuit() {
  quit();
}

SecureStream *
Application::newStream(uint16_t service) {
  if (0 == service) {
    // Chat service
    return new SecureChat(*this);
  }
  return 0;
}

bool
Application::allowStream(uint16_t service, const NodeItem &peer) {
  if (0 == service) {
    // Check if peer is buddy list
    return _buddies->hasNode(peer.id());
  }
  return false;
}

void
Application::streamStarted(SecureStream *stream) {
  SecureChat *chat = 0;
  if (0 != (chat = dynamic_cast<SecureChat *>(stream))) {
    _pendingChats.remove(stream->peerId());
    (new ChatWindow(chat))->show();
  } else {
    _dht->closeStream(stream->id()); delete stream;
  }
}

void
Application::startChatWith(const Identifier &id) {
  // Add id to list of pending chats
  _pendingChats.insert(id);
  // First search node
  _dht->findNode(id);
}

DHT &
Application::dht() {
  return *_dht;
}

Identity &
Application::identity() {
  return *_identity;
}

BuddyList &
Application::buddies() {
  return *_buddies;
}

void
Application::onNodeFound(const NodeItem &node) {
  if (_pendingChats.contains(node.id())) {
    qDebug() << "Node" << node.id() << "found: Start chat...";
    _dht->startStream(0, node);
  }
}

void
Application::onNodeNotFound(const Identifier &id, const QList<NodeItem> &best) {
  if (_pendingChats.contains(id)) {
    QMessageBox::critical(
          0, tr("Can not initialize chat"),
          tr("Can not initialize chat with node %1: not reachable.").arg(QString(id.toHex())));
    _pendingChats.remove(id);
  }
}
