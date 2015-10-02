#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QWidget>
#include <QTextBrowser>
#include <QLineEdit>
#include "securechat.h"


// Forward declarations
class Application;

class ChatWindow : public QWidget
{
  Q_OBJECT

public:
  explicit ChatWindow(Application &app, SecureChat *chat, QWidget *parent=0);
  virtual ~ChatWindow();

protected slots:
  void _onMessageReceived(const QString &msg);
  void _onMessageSend();

protected:
  void closeEvent(QCloseEvent *evt);

protected:
  Application &_application;
  SecureChat *_chat;
  QString _peer;
  QTextBrowser *_view;
  QLineEdit *_text;
};

#endif // CHATWINDOW_H
