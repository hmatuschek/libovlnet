#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QSystemTrayIcon>
#include <QAction>

#include "lib/crypto.h"
#include "lib/dht.h"
#include "dhtstatus.h"
#include "buddylist.h"
#include "securechat.h"


class Application : public QApplication, public StreamHandler
{
  Q_OBJECT

public:
  explicit Application(int &argc, char *argv[]);

  // Implementation of StreamHandler interface
  SecureStream *newStream(uint16_t service);
  bool allowStream(uint16_t service, const NodeItem &peer);
  void streamStarted(SecureStream *stream);

  /** Initializes a chat with the specified node. */
  void startChatWith(const Identifier &id);

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

  QAction *_showBuddies;
  QAction *_search;
  QAction *_bootstrap;
  QAction *_showStatus;
  QAction *_quit;

  /** Chat streams bing initialized. */
  QSet<Identifier> _pendingChats;
  /** The system tray icon. */
  QSystemTrayIcon *_trayIcon;
};

#endif // APPLICATION_H
