#include "chatwindow.h"
#include <QTextCursor>

ChatWindow::ChatWindow(SecureChat *chat, QWidget *parent)
  : QWidget(parent), _chat(chat)
{
  _view = new QTextBrowser();
  _text = new QLineEdit();

  QObject::connect(_chat, SIGNAL(messageReceived(QString)), this, SLOT(_onMessageReceived(QString)));
  QObject::connect(_text, SIGNAL(returnPressed()), this, SLOT(_onMessageSend()));
}

void
ChatWindow::_onMessageReceived(const QString &msg) {
  QTextCursor cursor = _view->textCursor();
  cursor.movePosition(QTextCursor::End);
  cursor.insertBlock();
  cursor.beginEditBlock();
  cursor.insertText(msg);
  cursor.endEditBlock();
}

void
ChatWindow::_onMessageSend() {
  QString msg = _text->text(); _text->clear();
  QTextCursor cursor = _view->textCursor();
  cursor.movePosition(QTextCursor::End);
  cursor.insertBlock();
  cursor.beginEditBlock();
  cursor.insertText(msg);
  cursor.endEditBlock();
  _chat->sendMessage(msg);
}


