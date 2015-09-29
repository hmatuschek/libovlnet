#include "callwindow.h"
#include <QCloseEvent>
#include <QToolBar>
#include <QAction>
#include <QVBoxLayout>
#include <QLabel>
#include "application.h"


CallWindow::CallWindow(Application &application, SecureCall *call, QWidget *parent)
  : QWidget(parent), _application(application), _call(call)
{
  QLabel *label = new QLabel();
  if (_application.buddies().hasNode(_call->peerId())) {
    label->setText(
          tr("<h3>Call with %1</h3>").arg(_application.buddies().buddyName(_call->peerId())));
  } else {
    label->setText(
          tr("<h3>Call with node %1</h3>").arg(QString(_call->peerId().toHex())));
  }
  label->setAlignment(Qt::AlignCenter);
  label->setMargin(8);

  QToolBar *ctrl = new QToolBar();
  QAction *mute = ctrl->addAction(QIcon("://icons/bullhorn.png"), tr("mute"), this, SLOT(onMute()));
  mute->setCheckable(true); mute->setChecked(true);
  QAction *pause = ctrl->addAction(QIcon("://icons/microphone.png"), tr("pause"), this, SLOT(onPause()));
  pause->setCheckable(true); pause->setChecked(true);
  ctrl->addAction(QIcon("://icons/circle-x.png"), tr("stop"), this, SLOT(onStop()));

  QVBoxLayout *layout = new QVBoxLayout();
  layout->setContentsMargins(0,0,0,0);
  layout->setSpacing(0);
  layout->addWidget(label);
  layout->addWidget(ctrl);
  setLayout(layout);
}

CallWindow::~CallWindow() {
  _call->deleteLater();
}

void
CallWindow::closeEvent(QCloseEvent *evt) {
  evt->accept();
  this->deleteLater();
}

void
CallWindow::onMute() {
  qDebug() << "Not implemented yet.";
}

void
CallWindow::onPause() {
  qDebug() << "Not implemented yet.";
}

void
CallWindow::onStop() {
  this->close();
}
