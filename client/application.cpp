#include "application.h"
#include "dhtstatusview.h"
#include "searchdialog.h"
#include "buddylistview.h"
#include "chatwindow.h"
#include "callwindow.h"
#include "filetransferdialog.h"

#include <portaudio.h>

#include <QMenu>
#include <QInputDialog>
#include <QHostAddress>
#include <QHostInfo>
#include <QMessageBox>
#include <QString>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>

Application::Application(int &argc, char *argv[])
  : QApplication(argc, argv), _identity(0), _dht(0), _status(0), _buddies(0),
    _bootstrapList()
{
  // Init PortAudio
  Pa_Initialize();

  // Do not quit application if the last window is closed.
  setQuitOnLastWindowClosed(false);

  // Set application name
  setApplicationName("vlf");
  setOrganizationName("com.github.hmatuschek");
  setOrganizationDomain("com.github.hmatuschek");

  // Try to load identity from file
  QDir vlfDir = QStandardPaths::writableLocation(
        QStandardPaths::DataLocation);
  // check if VLF directory exists
  if (! vlfDir.exists()) {
    vlfDir.mkpath(vlfDir.absolutePath());
  }
  // Load or create identity
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

  // load a list of bootstrap servers.
  _bootstrapList = BootstrapNodeList(vlfDir.canonicalPath()+"/bootstrap.json");
  QPair<QString, uint16_t> hostport;
  foreach (hostport, _bootstrapList) {
    _dht->ping(hostport.first, hostport.second);
  }

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
    _dht->ping(host, port);
    _bootstrapList.insert(host, port);
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

SecureSocket *
Application::newStream(uint16_t service) {
  if (1 == service) {
    qDebug() << "Create new SecureCall instance.";
    // VoIP service
    return new SecureCall(true, *this);
  } else if (2 == service) {
    // Chat service
    return new SecureChat(*this);
  } else if (4 == service) {
    // File download
    return new FileDownload(*this);
  }
  return 0;
}

bool
Application::allowStream(uint16_t service, const NodeItem &peer) {
  if ((1 == service) || (2 == service) || (4 == service)) {
    // VoIP or Chat services: check if peer is buddy list
    return _buddies->hasNode(peer.id());
  }
  return false;
}

void
Application::streamStarted(SecureSocket *stream) {
  SecureChat *chat = 0;
  SecureCall *call = 0;
  FileUpload *upload = 0;
  FileDownload *download = 0;

  if (0 != (chat = dynamic_cast<SecureChat *>(stream))) {
    // start keep alive timer
    chat->keepAlive();
    (new ChatWindow(*this, chat))->show();
  } else if (0 != (call = dynamic_cast<SecureCall *>(stream))) {
    // start streaming
    call->initialized();
    (new CallWindow(*this, call))->show();
  } else if (0 != (upload = dynamic_cast<FileUpload *>(stream))) {
    // Send request
    upload->sendRequest();
    // show upload dialog
    (new FileUploadDialog(upload, *this))->show();
  } else if (0 != (download = dynamic_cast<FileDownload *>(stream))) {
    // show download dialog
    (new FileDownloadDialog(download, *this))->show();
  } else {
    _dht->closeStream(stream->id());
    delete stream;
  }
}

void
Application::streamFailed(SecureSocket *stream) {
  /// @todo Handle stream errors;
}

void
Application::startChatWith(const Identifier &id) {
  // Add id to list of pending chats
  _pendingStreams.insert(id, new SecureChat(*this));
  // First search node
  _dht->findNode(id);
}

void
Application::call(const Identifier &id) {
  // Add id to list of pending calls
  _pendingStreams.insert(id, new SecureCall(false, *this));
  // First, search node
  _dht->findNode(id);
}

void
Application::sendFile(const QString &path, size_t size, const Identifier &id) {
  // Add id to list of pending file transfers
  _pendingStreams.insert(id, new FileUpload(*this, path, size));
  // First, search for the node id
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
  if (! _pendingStreams.contains(node.id())) { return; }
  SecureSocket *stream = _pendingStreams[node.id()];
  _pendingStreams.remove(node.id());

  SecureChat *chat = 0;
  SecureCall *call = 0;
  FileUpload *upload = 0;

  // Dispatch by type
  if (0 != (chat = dynamic_cast<SecureChat *>(stream))) {
    qDebug() << "Node" << node.id() << "found: Start chat...";
    _dht->startStream(2, node, stream);
  } else if (0 != (call = dynamic_cast<SecureCall *>(stream))) {
    qDebug() << "Node" << node.id() << "found: Start call...";
    _dht->startStream(1, node, stream);
  } else if (0 != (upload = dynamic_cast<FileUpload *>(stream))) {
    qDebug() << "Node" << node.id() << "found: Start uploaing" << upload->fileName();
    _dht->startStream(4, node, stream);
  }
}

void
Application::onNodeNotFound(const Identifier &id, const QList<NodeItem> &best) {
  if (!_pendingStreams.contains(id)) { return; }
  QMessageBox::critical(
        0, tr("Can not initialize stream"),
        tr("Can not initialize a secure connection to %1: not reachable.").arg(QString(id.toHex())));
  // Free stream
  delete _pendingStreams[id];
  _pendingStreams.remove(id);
}
