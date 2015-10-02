#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QSystemTrayIcon>
#include <QAction>
#include <QStringList>

#include "lib/crypto.h"
#include "lib/dht.h"
#include "dhtstatus.h"
#include "buddylist.h"
#include "securechat.h"
#include "bootstrapnodelist.h"


class Application : public QApplication, public StreamHandler
{
  Q_OBJECT

public:
  explicit Application(int &argc, char *argv[]);
  virtual ~Application();

  // Implementation of StreamHandler interface
  SecureStream *newStream(uint16_t service);
  bool allowStream(uint16_t service, const NodeItem &peer);
  void streamStarted(SecureStream *stream);
  void streamFailed(SecureStream *stream);

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

protected slots:
  /** Callback for the "search" action. */
  void onSearch();
  /** Callback for the "show buddy list" action. */
  void onShowBuddies();
  /** Callback for the "bootstrap" action. */
  void onBootstrap();
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

  QAction *_showBuddies;
  QAction *_search;
  QAction *_bootstrap;
  QAction *_showStatus;
  QAction *_quit;

  QWidget *_searchWindow;
  QWidget *_buddyListWindow;
  QWidget *_statusWindow;

  /** Table of pending streams. */
  QHash<Identifier, SecureStream *> _pendingStreams;
  /** The system tray icon. */
  QSystemTrayIcon *_trayIcon;
};

#endif // APPLICATION_H
