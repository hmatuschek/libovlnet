#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QSystemTrayIcon>
#include <QAction>
#include <QStringList>
#include <QTimer>

#include "lib/crypto.hh"
#include "lib/dht.hh"
#include "dhtstatus.hh"
#include "buddylist.hh"
#include "bootstrapnodelist.hh"
#include "logwindow.hh"
#include "settings.hh"


class Application : public QApplication
{
  Q_OBJECT

public:
  explicit Application(int &argc, char *argv[]);
  virtual ~Application();

  /** Initializes a chat with the specified node. */
  void startChatWith(const Identifier &id);
  /** Initializes a voice call to the specified node. */
  void call(const Identifier &id);
  /** Initializes a file transfer. */
  void sendFile(const QString &path, size_t size, const Identifier &id);

  /** Returns a weak reference to the DHT instance. */
  DHT &dht();
  /** Returns the settings instance. */
  Settings &settings();
  /** Returns a weak reference to the identity of this DHT node. */
  Identity &identity();
  /** Returns a weak reference to the buddy list. */
  BuddyList &buddies();
  /** Returns the log model. */
  LogModel &log();
  /** Returns the status model of the application. */
  DHTStatus &status();

  /** Returns @c true if the OvlNet node was started successfully. */
  bool started() const;

public slots:
  /** Shows the search dialog. */
  void search();

protected slots:
  /** Callback for the "show buddy list" action. */
  void onShowBuddies();
  /** Callback for the "bootstrap" action. */
  void onBootstrap();
  /** Callback for the "show settings" action. */
  void onShowSettings();
  /** Callback for the "show DHT status" action. */
  void onShowStatus();
  /** Callback for the "quit" action. */
  void onQuit();

  void onSearchWindowClosed();
  void onBuddyListClosed();
  void onStatusWindowClosed();

  /** Get notified if a node search was successful. */
  void onNodeFound(const NodeItem &node);
  /** Get notified if a node cannot be found. */
  void onNodeNotFound(const Identifier &id, const QList<NodeItem> &best);
  /** Get notified if the DHT connected to the network. */
  void onDHTConnected();
  /** Get notified if the DHT lost the connection to the network. */
  void onDHTDisconnected();
  /** Gets called periodically on connection loss to bootstrap a connection to the network. */
  void onReconnect();

protected:
  class ChatService: public AbstractService
  {
  public:
    ChatService(Application &app);
    SecureSocket *newSocket();
    bool allowConnection(const NodeItem &peer);
    void connectionStarted(SecureSocket *socket);
    void connectionFailed(SecureSocket *socket);
  protected:
    Application &_application;
  };

protected:
  /** The identity of this DHT node. */
  Identity *_identity;
  /** This DHT node. */
  DHT *_dht;
  /** Status of the DHT node. */
  DHTStatus *_status;
  /** The persistent settings instance. */
  Settings *_settings;
  /** The buddy list. */
  BuddyList *_buddies;
  /** The list of bootstap servers. */
  BootstrapNodeList _bootstrapList;
  /** Receives log messages. */
  LogModel *_logModel;

  QAction *_showBuddies;
  QAction *_search;
  QAction *_bootstrap;
  QAction *_showSettings;
  QAction *_showStatus;
  QAction *_quit;

  QWidget *_searchWindow;
  QWidget *_buddyListWindow;
  QWidget *_statusWindow;

  /** Table of pending streams. */
  QHash<Identifier, SecureSocket *> _pendingStreams;
  /** The system tray icon. */
  QSystemTrayIcon *_trayIcon;

  /** Once the connection to the network is lost, try to reconnect every minute. */
  QTimer _reconnectTimer;
};

#endif // APPLICATION_H
