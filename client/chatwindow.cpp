#include "chatwindow.h"
#include "application.h"
#include <QTextCursor>
#include <QVBoxLayout>
#include <QTextBlockFormat>
#include <QCloseEvent>


ChatWindow::ChatWindow(Application &app, SecureChat *chat, QWidget *parent)
  : QWidget(parent), _application(app), _chat(chat)
{
  _peer = QString(chat->peerId().toHex());
  if (_application.buddies().hasNode(chat->peerId())) {
    _peer = _application.buddies().buddyName(chat->peerId());
  }
  setWindowTitle(tr("Chat with %1").arg(_peer));
  setMinimumWidth(200);

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
  cursor.insertText(_peer);
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
