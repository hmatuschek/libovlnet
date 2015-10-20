#ifndef CALLWINDOW_H
#define CALLWINDOW_H

#include <QWidget>
#include "lib/securecall.h"

class Application;

class CallWindow : public QWidget
{
  Q_OBJECT

public:
  explicit CallWindow(Application &application, SecureCall *call, QWidget *parent = 0);
  virtual ~CallWindow();

protected slots:
  void onMute();
  void onPause();
  void onStartStop();

  void onCallStarted();
  void onCallEnd();

protected:
  void closeEvent(QCloseEvent *evt);

protected:
  Application &_application;
  SecureCall *_call;

  QAction *_silent;
  QAction *_mute;
  QAction *_startStop;
};

#endif // CALLWINDOW_H
