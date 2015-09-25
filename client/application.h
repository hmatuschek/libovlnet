#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QSystemTrayIcon>
#include <QAction>

#include "lib/dht.h"
#include "dhtstatus.h"


class Application : public QApplication
{
  Q_OBJECT

public:
  explicit Application(int argc, char *argv[]);

protected slots:
  void onShowStatus();
  void onQuit();

protected:
  DHT *_dht;
  DHTStatus *_status;

  QAction *_showStatus;
  QAction *_quit;

  QSystemTrayIcon *_trayIcon;
};

#endif // APPLICATION_H
