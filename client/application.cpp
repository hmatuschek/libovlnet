#include "application.h"
#include "dhtstatusview.h"
#include "searchdialog.h"
#include "buddylistview.h"
#include "chatwindow.h"
#include "callwindow.h"

#include <portaudio.h>

#include <QMenu>
#include <QInputDialog>
#include <QHostAddress>
#include <QHostInfo>
#include <QMessageBox>
#include <QString>
#include <QDir>


Application::Application(int &argc, char *argv[])
  : QApplication(argc, argv), _identity(0), _dht(0), _status(0)
{
  // Init PortAudio
  Pa_Initialize();

  // Do not quit application if the last window is closed.
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

  if (0 == _identity) {
    qDebug() << "Error while loading or creating my identity.";
    return;
  }

  _dht = new DHT(_identity->id(), this, QHostAddress::Any, 7742);

  _status = new DHTStatus(_dht);
  _buddies = new BuddyList(*this, vlfDir.canonicalPath()+"/buddies.json");

  _search      = new QAction(QIcon("://icons/search.png"),    tr("Search..."), this);
  _searchWindow = 0;
  _showBuddies = new QAction(QIcon("://icons/people.png"),    tr("Contacts..."), this);
  _buddyListWindow = 0;
  _bootstrap   = new QAction(QIcon("://icons/bootstrap.png"), tr("Bootstrap..."), this);
  _showStatus  = new QAction(QIcon("://icons/settings.png"),  tr("Show status ..."), this);
  _statusWindow = 0;
  _quit        = new QAction(QIcon("://icons/quit.png"),      tr("Quit"), this);

  QMenu *ctx = new QMenu();
  ctx->addAction(_search);
  ctx->addAction(_showBuddies);
  ctx->addSeparator();
  ctx->addAction(_bootstrap);
  ctx->addAction(_showStatus);
  ctx->addSeparator();
  ctx->addAction(_quit);

  _trayIcon = new QSystemTrayIcon();
  _trayIcon->setIcon(QIcon("://icons/fork.png"));
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

Application::~Application() {
  Pa_Terminate();
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
  if (_searchWindow) {
    _searchWindow->activateWindow();
  } else {
    _searchWindow = new SearchDialog(_dht, _buddies);
    _searchWindow->show();
    QObject::connect(_searchWindow, SIGNAL(destroyed()), this, SLOT(onSearchWindowClosed()));
  }
}

void
Application::onSearchWindowClosed() {
  _searchWindow = 0;
}

void
Application::onShowBuddies() {
  if (_buddyListWindow) {
    _buddyListWindow->activateWindow();
  } else {
    _buddyListWindow = new BuddyListView(*this,  _buddies);
    _buddyListWindow->show();
    QObject::connect(_buddyListWindow, SIGNAL(destroyed()),
                     this, SLOT(onBuddyListClosed()));
  }
}

void
Application::onBuddyListClosed() {
  _buddyListWindow = 0;
}

void
Application::onShowStatus() {
  if (_statusWindow) {
    _statusWindow->activateWindow();
  } else {
    _statusWindow = new DHTStatusView(_status);
    _statusWindow->show();
    QObject::connect(_statusWindow, SIGNAL(destroyed()),
                     this, SLOT(onStatusWindowClosed()));
  }
}

void
Application::onStatusWindowClosed() {
  _statusWindow = 0;
}

void
Application::onQuit() {
  quit();
}

SecureStream *
Application::newStream(bool incomming, uint16_t service) {
  if (1 == service) {
    qDebug() << "Create new SecureCall instance.";
    // VoIP service
    return new SecureCall(incomming, *this);
  } else if (2 == service) {
    // Chat service
    return new SecureChat(incomming, *this);
  }
  return 0;
}

bool
Application::allowStream(uint16_t service, const NodeItem &peer) {
  if ((1 == service) || (2 == service)) {
    // VoIP or Chat services: check if peer is buddy list
    return _buddies->hasNode(peer.id());
  }
  return false;
}

void
Application::streamStarted(SecureStream *stream) {
  SecureChat *chat = 0;
  SecureCall *call = 0;
  if (0 != (chat = dynamic_cast<SecureChat *>(stream))) {
    // Remove stream ID from pending chats
    _pendingChats.remove(stream->peerId());
    // start keep alive timer
    chat->keepAlive();
    (new ChatWindow(chat))->show();
  } if (0 != (call = dynamic_cast<SecureCall *>(stream))) {
    // Remove stream ID from pending chats
    _pendingCalls.remove(stream->peerId());
    // start streaming
    call->initialized();
    (new CallWindow(*this, call))->show();
  } else {
    _dht->closeStream(stream->id());
    delete stream;
  }
}

void
Application::startChatWith(const Identifier &id) {
  // Add id to list of pending chats
  _pendingChats.insert(id);
  // First search node
  _dht->findNode(id);
}

void
Application::call(const Identifier &id) {
  // Add id to list of pending calls
  _pendingCalls.insert(id);
  // First, search node
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
    _dht->startStream(2, node);
  } else if (_pendingCalls.contains(node.id())) {
    qDebug() << "Node" << node.id() << "found: Start call...";
    _dht->startStream(1, node);
  }
}

void
Application::onNodeNotFound(const Identifier &id, const QList<NodeItem> &best) {
  if (_pendingChats.contains(id)) {
    QMessageBox::critical(
          0, tr("Can not initialize chat"),
          tr("Can not initialize chat with node %1: not reachable.").arg(QString(id.toHex())));
    _pendingChats.remove(id);
  } else if (_pendingCalls.contains(id)) {
    QMessageBox::critical(
          0, tr("Can not initialize call"),
          tr("Can not initialize call to node %1: not reachable.").arg(QString(id.toHex())));
    _pendingCalls.remove(id);
  }
}
