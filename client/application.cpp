#include "application.h"
#include "dhtstatusview.h"
#include "searchdialog.h"
#include "buddylistview.h"
#include "chatwindow.h"
#include "callwindow.h"
#include "filetransferdialog.h"
#include "settingsdialog.h"

#include "lib/socks.h"
#include "lib/logger.h"

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
  : QApplication(argc, argv), _identity(0), _dht(0), _status(0), _settings(0),
    _buddies(0), _bootstrapList(), _reconnectTimer()
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
  QDir nodeDir = QStandardPaths::writableLocation(
        QStandardPaths::DataLocation);
  // check if VLF directory exists
  if (! nodeDir.exists()) {
    nodeDir.mkpath(nodeDir.absolutePath());
  }
  // Load or create identity
  QString idFile(nodeDir.canonicalPath()+"/identity.pem");
  if (!QFile::exists(idFile)) {
    logInfo() << "No identity found -> create new identity.";
    _identity = Identity::newIdentity();
    if (_identity) { _identity->save(idFile); }
  } else {
    logDebug() << "Load identity from" << idFile;
    _identity = Identity::load(idFile);
  }

  if (0 == _identity) {
    logError() << "Error while loading or creating identity.";
    return;
  }

  // Create log model
  _logModel = new LogModel();
  Logger::addHandler(_logModel);

  // Create DHT instance
  _dht = new DHT(*_identity, this, QHostAddress::Any, 7742);

  // Load settings
  _settings = new Settings(nodeDir.canonicalPath()+"/settings.json");

  // load a list of bootstrap servers.
  _bootstrapList = BootstrapNodeList(nodeDir.canonicalPath()+"/bootstrap.json");
  QPair<QString, uint16_t> hostport;
  foreach (hostport, _bootstrapList) {
    _dht->ping(hostport.first, hostport.second);
  }

  // Create DHT status object
  _status = new DHTStatus(*this);
  // Create buddy list model
  _buddies = new BuddyList(*this, nodeDir.canonicalPath()+"/buddies.json");

  // Actions
  _search      = new QAction(QIcon("://icons/search.png"),    tr("Search..."), this);
  _searchWindow = 0;
  _showBuddies = new QAction(QIcon("://icons/people.png"),    tr("Contacts..."), this);
  _buddyListWindow = 0;
  _bootstrap   = new QAction(QIcon("://icons/bootstrap.png"), tr("Bootstrap..."), this);
  _showSettings = new QAction(QIcon("://icons/wrench.png"), tr("Settings..."), this);
  _showStatus  = new QAction(QIcon("://icons/dashboard.png"),  tr("Show status ..."), this);
  _statusWindow = 0;
  _showLogWindow = new QAction(QIcon("://icons/list.png"), tr("Show log..."), this);
  _logWindow = 0;
  _quit        = new QAction(QIcon("://icons/quit.png"),      tr("Quit"), this);

  QMenu *ctx = new QMenu();
  ctx->addAction(_search);
  ctx->addAction(_showBuddies);
  ctx->addSeparator();
  ctx->addAction(_bootstrap);
  ctx->addAction(_showSettings);
  ctx->addAction(_showStatus);
  ctx->addAction(_showLogWindow);
  ctx->addSeparator();
  ctx->addAction(_quit);

  _trayIcon = new QSystemTrayIcon();
  if (_dht->numNodes()) {
    _trayIcon->setIcon(QIcon("://icons/fork.png"));
  } else {
    _trayIcon->setIcon(QIcon("://icons/fork_gray.png"));
  }
  _trayIcon->setContextMenu(ctx);
  _trayIcon->show();

  // setup reconnect timer
  _reconnectTimer.setInterval(1000*60);
  _reconnectTimer.setSingleShot(false);
  if (0 == _dht->numNodes()) {
    _reconnectTimer.start();
  }

  connect(_dht, SIGNAL(nodeFound(NodeItem)), this, SLOT(onNodeFound(NodeItem)));
  connect(_dht, SIGNAL(nodeNotFound(Identifier,QList<NodeItem>)),
          this, SLOT(onNodeNotFound(Identifier,QList<NodeItem>)));
  QObject::connect(_dht, SIGNAL(connected()), this, SLOT(onDHTConnected()));
  QObject::connect(_dht, SIGNAL(disconnected()), this, SLOT(onDHTDisconnected()));

  QObject::connect(_search, SIGNAL(triggered()), this, SLOT(search()));
  QObject::connect(_showBuddies, SIGNAL(triggered()), this, SLOT(onShowBuddies()));
  QObject::connect(_bootstrap, SIGNAL(triggered()), this, SLOT(onBootstrap()));
  connect(_showSettings, SIGNAL(triggered()), this, SLOT(onShowSettings()));
  QObject::connect(_showStatus, SIGNAL(triggered()), this, SLOT(onShowStatus()));
  QObject::connect(_showLogWindow, SIGNAL(triggered()), this, SLOT(onShowLogWindow()));
  QObject::connect(_quit, SIGNAL(triggered()), this, SLOT(onQuit()));

  QObject::connect(&_reconnectTimer, SIGNAL(timeout()), this, SLOT(onReconnect()));
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
Application::search() {
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
    _buddyListWindow->raise();
  } else {
    _buddyListWindow = new BuddyListView(*this,  _buddies);
    _buddyListWindow->show();
    _buddyListWindow->raise();
    QObject::connect(_buddyListWindow, SIGNAL(destroyed()),
                     this, SLOT(onBuddyListClosed()));
  }
}

void
Application::onBuddyListClosed() {
  _buddyListWindow = 0;
}

void
Application::onShowSettings() {
  SettingsDialog dialog(settings());
  if (QDialog::Accepted == dialog.exec()) {
    dialog.apply();
  }
}

void
Application::onShowStatus() {
  if (_statusWindow) {
    _statusWindow->activateWindow();
    _statusWindow->raise();
  } else {
    _statusWindow = new DHTStatusView(_status);
    _statusWindow->show();
    _statusWindow->raise();
    QObject::connect(_statusWindow, SIGNAL(destroyed()),
                     this, SLOT(onStatusWindowClosed()));
  }
}

void
Application::onStatusWindowClosed() {
  _statusWindow = 0;
}

void
Application::onShowLogWindow() {
  if (_logWindow) {
    _logWindow->activateWindow();
    _logWindow->raise();
  } else {
    _logWindow = new LogWindow(_logModel);
    _logWindow->show();
    _logWindow->raise();
    QObject::connect(_logWindow, SIGNAL(destroyed()),
                     this, SLOT(onLogWindowClosed()));
  }
}

void
Application::onLogWindowClosed() {
  _logWindow = 0;
}

void
Application::onQuit() {
  quit();
}

SecureSocket *
Application::newSocket(uint16_t service) {
  if (1 == service) {
    // VoIP service
    logDebug() << "Create new SecureCall instance.";
    return new SecureCall(true, dht());
  } else if (2 == service) {
    // Chat service
    logDebug() << "Create new SecureChat instance.";
    return new SecureChat(dht());
  } else if (4 == service) {
    // File download
    logDebug() << "Create new Download instance.";
    return new FileDownload(dht());
  } else if (5 == service) {
    // If socks service is disabled -> return 0
    if (! _settings->socksServiceSettings().enabled()) {
      return 0;
    }
    // otherwise create SOCKS proxy stream
    return new SocksOutStream(dht());
  } else {
    logWarning() << "Unknown service number " << service;
  }
  return 0;
}

bool
Application::allowConnection(uint16_t service, const NodeItem &peer) {
  if ((1 == service) || (2 == service) || (4 == service)) {
    // File upload, VoIP or Chat services:
    //  check if peer is buddy list
    return _buddies->hasNode(peer.id());
  } else if (5 == service) {
    // If buddies are allowed to use the SOCKS service or
    //  if whitelist is enabled and node is listed there
    return ( (_settings->socksServiceSettings().allowBuddies() && _buddies->hasNode(peer.id())) ||
             (_settings->socksServiceSettings().allowWhiteListed() &&
              _settings->socksServiceSettings().whitelist().contains(peer.id())) );
  }
  return false;
}

void
Application::connectionStarted(SecureSocket *stream) {
  // Dispatch by stream type
  if (SecureChat *chat = dynamic_cast<SecureChat *>(stream)) {
    // Start keep alive timer
    chat->started();
    (new ChatWindow(*this, chat))->show();
  } else if (SecureCall *call = dynamic_cast<SecureCall *>(stream)) {
    // Start streaming
    call->initialized();
    (new CallWindow(*this, call))->show();
  } else if (FileUpload *upload = dynamic_cast<FileUpload *>(stream)) {
    // Send request
    upload->sendRequest();
    // Show upload dialog
    (new FileUploadDialog(upload, *this))->show();
  } else if (FileDownload *download = dynamic_cast<FileDownload *>(stream)) {
    // Show download dialog
    (new FileDownloadDialog(download, *this))->show();
  } else if (LocalSocksStream *socksin = dynamic_cast<LocalSocksStream *>(stream)) {
    // Simply open the stream
    socksin->open(QIODevice::ReadWrite);
  } else if (SocksOutStream *socksout = dynamic_cast<SocksOutStream *>(stream)) {
    // start SOCKS service.
    socksout->open(QIODevice::ReadWrite);
  } else {
    logWarning() << "Unknown service type. Close connection " << stream->id();
    _dht->socketClosed(stream->id());
    delete stream;
  }
}

void
Application::connectionFailed(SecureSocket *stream) {
  /// @todo Handle stream errors;
}

void
Application::startChatWith(const Identifier &id) {
  // Add id to list of pending chats
  _pendingStreams.insert(id, new SecureChat(dht()));
  // First search node
  _dht->findNode(id);
}

void
Application::call(const Identifier &id) {
  // Add id to list of pending calls
  _pendingStreams.insert(id, new SecureCall(false, dht()));
  // First, search node
  _dht->findNode(id);
}

void
Application::sendFile(const QString &path, size_t size, const Identifier &id) {
  // Add id to list of pending file transfers
  _pendingStreams.insert(id, new FileUpload(dht(), path, size));
  // First, search for the node id
  _dht->findNode(id);
}

DHT &
Application::dht() {
  return *_dht;
}

Settings &
Application::settings() {
  return *_settings;
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
    logInfo() << "Node " << node.id() << " found: Start chat.";
    _dht->startStream(2, node, stream);
  } else if (0 != (call = dynamic_cast<SecureCall *>(stream))) {
    logInfo() << "Node " << node.id() << " found: Start call.";
    _dht->startStream(1, node, stream);
  } else if (0 != (upload = dynamic_cast<FileUpload *>(stream))) {
    logInfo() << "Node " << node.id() << "found: Start upload of file " << upload->fileName();
    _dht->startStream(4, node, stream);
  }
}

void
Application::onNodeNotFound(const Identifier &id, const QList<NodeItem> &best) {
  if (!_pendingStreams.contains(id)) { return; }
  QMessageBox::critical(
        0, tr("Can not initialize connection"),
        tr("Can not initialize a secure connection to %1: not reachable.").arg(QString(id.toHex())));
  // Free stream
  delete _pendingStreams[id];
  _pendingStreams.remove(id);
}

void
Application::onDHTConnected() {
  logInfo() << "Connected to overlay network.";
  _trayIcon->setIcon(QIcon("://icons/fork.png"));
  _reconnectTimer.stop();
}

void
Application::onDHTDisconnected() {
  logInfo() << "Lost connection to overlay network.";
  _trayIcon->setIcon(QIcon("://icons/fork_gray.png"));
  _reconnectTimer.start();
}

void
Application::onReconnect() {
  if (_dht->numNodes()) { onDHTConnected(); return;   logInfo() << "Connected to overlay network.";
    _trayIcon->setIcon(QIcon("://icons/fork.png"));
    _reconnectTimer.stop();
  }
  logInfo() << "Connect to overlay network...";
  QPair<QString, uint16_t> hostport;
  foreach (hostport, _bootstrapList) {
    _dht->ping(hostport.first, hostport.second);
  }
}
