#include "chatwindow.h"
#include <QTextCursor>
#include <QVBoxLayout>
#include <QTextBlockFormat>

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
  QTextBlockFormat fmt = cursor.blockFormat();
  fmt.setAlignment(Qt::AlignLeft);
  fmt.setBottomMargin(10.);
  cursor.setBlockFormat(fmt);
  cursor.beginEditBlock();
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
  QTextBlockFormat fmt = cursor.blockFormat();
  fmt.setAlignment(Qt::AlignRight);
  fmt.setBottomMargin(10.);
  cursor.setBlockFormat(fmt);
  cursor.beginEditBlock();
  cursor.insertText(msg);
  cursor.endEditBlock();
  _chat->sendMessage(msg);
}
