#ifndef SOCKSWINDOW_H
#define SOCKSWINDOW_H

#include <QWidget>
#include <QLabel>
#include "socksservice.h"

class Application;

class SocksWindow : public QWidget
{
  Q_OBJECT

public:
  explicit SocksWindow(Application &app, const NodeItem &node, QWidget *parent = 0);

protected slots:
  void _onConnectionCountChanged(size_t count);

protected:
  void closeEvent(QCloseEvent *evt);

protected:
  Application &_application;
  SocksService _socks;

  QLabel *_info;
  QLabel *_connectionCount;
};

#endif // SOCKSWINDOW_H
