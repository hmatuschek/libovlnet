#include "chatwindow.hh"
#include "application.hh"
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
  setMinimumWidth(400);
  setMinimumHeight(300);

  _view = new QTextBrowser();
  _text = new QLineEdit();

  QVBoxLayout *layout = new QVBoxLayout();
  layout->setSpacing(0);
  layout->setContentsMargins(0,0,0,0);
  layout->addWidget(_view);
  layout->addWidget(_text);
  setLayout(layout);

  connect(_chat, SIGNAL(messageReceived(QString)), this, SLOT(_onMessageReceived(QString)));
  connect(_chat, SIGNAL(closed()), this, SLOT(_onConnectionLost()));
  connect(_text, SIGNAL(returnPressed()), this, SLOT(_onMessageSend()));
}

ChatWindow::~ChatWindow() {
  _chat->deleteLater();
}

void
ChatWindow::_onMessageReceived(const QString &msg) {
  QTextCursor cursor = _view->textCursor();
  cursor.movePosition(QTextCursor::End);
  if (! cursor.atStart())
    cursor.insertBlock();
  cursor.beginEditBlock();
  QTextBlockFormat fmt;
  fmt.setAlignment(Qt::AlignLeft);
  cursor.setBlockFormat(fmt);
  cursor.insertText("(");
  cursor.insertText(QTime::currentTime().toString());
  cursor.insertText(") ");
  cursor.insertText(_peer);
  cursor.insertText(":\n");
  cursor.insertText(msg);
  cursor.endEditBlock();
  _view->ensureCursorVisible();
}

void
ChatWindow::_onMessageSend() {
  QString msg = _text->text(); _text->clear();
  QTextCursor cursor = _view->textCursor();
  cursor.movePosition(QTextCursor::End);
  if (!cursor.atStart())
    cursor.insertBlock();
  cursor.beginEditBlock();
  QTextBlockFormat fmt;
  fmt.setAlignment(Qt::AlignRight);
  cursor.setBlockFormat(fmt);
  cursor.insertText("(");
  cursor.insertText(QTime::currentTime().toString());
  cursor.insertText(") you:\n");
  cursor.insertText(msg);
  cursor.endEditBlock();
  _chat->sendMessage(msg);
  _view->ensureCursorVisible();
}

void
ChatWindow::_onConnectionLost() {
  QTextCursor cursor = _view->textCursor();
  cursor.movePosition(QTextCursor::End);
  if (!cursor.atStart())
    cursor.insertBlock();
  cursor.beginEditBlock();
  cursor.insertText("[connection lost]");
  cursor.endEditBlock();

  _view->setEnabled(false);
  _text->setEnabled(false);
  disconnect(_chat, SIGNAL(messageReceived(QString)), this, SLOT(_onMessageReceived(QString)));
}

void
ChatWindow::closeEvent(QCloseEvent *evt) {
  evt->accept();
  this->deleteLater();
}
