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
  explicit Application(int argc, char *argv[]);

  SecureStream *newStream(uint16_t service);
  bool allowStream(uint16_t service, const NodeItem &peer);
  void streamStarted(SecureStream *stream);

  void startChatWith(const Identifier &id);

signals:
  void chatStarted(SecureChat *chat);

protected slots:
  void onSearch();
  void onShowBuddies();
  void onBootstrap();
  void onShowStatus();
  void onNodeFound(const NodeItem &node);
  void onNodeNotFound(const Identifier &id, const QList<NodeItem> &best);
  void onQuit();

protected:
  Identity *_identity;
  DHT *_dht;
  DHTStatus *_status;
  BuddyList *_buddies;

  QAction *_showBuddies;
  QAction *_search;
  QAction *_bootstrap;
  QAction *_showStatus;
  QAction *_quit;

  QSet<Identifier> _pendingChats;

  QSystemTrayIcon *_trayIcon;
};

#endif // APPLICATION_H
