#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QSystemTrayIcon>
#include <QAction>
#include <QStringList>
#include <QTimer>

#include "lib/crypto.h"
#include "lib/dht.h"
#include "dhtstatus.h"
#include "buddylist.h"
#include "bootstrapnodelist.h"
#include "logwindow.h"


class Application : public QApplication, public ServiceHandler
{
  Q_OBJECT

public:
  explicit Application(int &argc, char *argv[]);
  virtual ~Application();

  // Implementation of StreamHandler interface
  SecureSocket *newSocket(uint16_t service);
  bool allowConnection(uint16_t service, const NodeItem &peer);
  void connectionStarted(SecureSocket *stream);
  void connectionFailed(SecureSocket *stream);

  /** Initializes a chat with the specified node. */
  void startChatWith(const Identifier &id);
  /** Initializes a voice call to the specified node. */
  void call(const Identifier &id);
  /** Initializes a file transfer. */
  void sendFile(const QString &path, size_t size, const Identifier &id);

  /** Returns a weak reference to the DHT instance. */
  DHT &dht();
  /** Returns a weak reference to the identity of this DHT node. */
  Identity &identity();
  /** Returns a weak reference to the buddy list. */
  BuddyList &buddies();

public slots:
  /** Shows the search dialog. */
  void search();

protected slots:
  /** Callback for the "show buddy list" action. */
  void onShowBuddies();
  /** Callback for the "bootstrap" action. */
  void onBootstrap();
  /** Callback for the "show DHT status" action. */
  void onShowStatus();
  /** Callback for the "show log window" action. */
  void onShowLogWindow();
  /** Callback for the "quit" action. */
  void onQuit();

  void onSearchWindowClosed();
  void onBuddyListClosed();
  void onStatusWindowClosed();
  void onLogWindowClosed();

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
  /** The identity of this DHT node. */
  Identity *_identity;
  /** This DHT node. */
  DHT *_dht;
  /** Status of the DHT node. */
  DHTStatus *_status;
  /** The buddy list. */
  BuddyList *_buddies;
  /** The list of bootstap servers. */
  BootstrapNodeList _bootstrapList;
  /** Receives log messages. */
  LogModel *_logModel;

  QAction *_showBuddies;
  QAction *_search;
  QAction *_bootstrap;
  QAction *_showStatus;
  QAction *_showLogWindow;
  QAction *_quit;

  QWidget *_searchWindow;
  QWidget *_buddyListWindow;
  QWidget *_statusWindow;
  QWidget *_logWindow;

  /** Table of pending streams. */
  QHash<Identifier, SecureSocket *> _pendingStreams;
  /** The system tray icon. */
  QSystemTrayIcon *_trayIcon;

  /** Once the connection to the network is lost, try to reconnect every minute. */
  QTimer _reconnectTimer;
};

#endif // APPLICATION_H
