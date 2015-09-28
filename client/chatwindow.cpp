#include "chatwindow.h"
#include <QTextCursor>
#include <QVBoxLayout>
#include <QTextBlockFormat>
#include <QCloseEvent>


ChatWindow::ChatWindow(SecureChat *chat, QWidget *parent)
  : QWidget(parent), _chat(chat)
{
  _view = new QTextBrowser();
  _text = new QLineEdit();

  QVBoxLayout *layout = new QVBoxLayout();
  layout->setSpacing(0);
  layout->setContentsMargins(0,0,0,0);
  layout->addWidget(_view);
  layout->addWidget(_text);
  setLayout(layout);

  QObject::connect(_chat, SIGNAL(messageReceived(QString)), this, SLOT(_onMessageReceived(QString)));
  QObject::connect(_text, SIGNAL(returnPressed()), this, SLOT(_onMessageSend()));
}

ChatWindow::~ChatWindow() {
  _chat->deleteLater();
}

void
ChatWindow::_onMessageReceived(const QString &msg) {
  QTextCursor cursor = _view->textCursor();
  cursor.movePosition(QTextCursor::End);
  if (!cursor.atStart())
    cursor.insertBlock();
  cursor.beginEditBlock();
  cursor.insertText("(");
  cursor.insertText(QTime::currentTime().toString());
  cursor.insertText(") ");
  cursor.insertText(_chat->peerId().toHex());
  cursor.insertText(":\n");
  cursor.insertText(msg);
  cursor.endEditBlock();
}

void
ChatWindow::_onMessageSend() {
  QString msg = _text->text(); _text->clear();
  QTextCursor cursor = _view->textCursor();
  cursor.movePosition(QTextCursor::End);
  if (!cursor.atStart())
    cursor.insertBlock();
  cursor.beginEditBlock();
  cursor.insertText("(");
  cursor.insertText(QTime::currentTime().toString());
  cursor.insertText(") you:\n");
  cursor.insertText(msg);
  cursor.endEditBlock();
  _chat->sendMessage(msg);
}

void
ChatWindow::closeEvent(QCloseEvent *evt) {
  evt->accept();
  this->deleteLater();
}
