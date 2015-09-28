#include "callwindow.h"
#include <QCloseEvent>
#include <QToolBar>
#include <QAction>
#include <QVBoxLayout>


CallWindow::CallWindow(SecureCall *call, QWidget *parent)
  : QWidget(parent), _call(call)
{
  QToolBar *ctrl = new QToolBar();
  QAction *mute = ctrl->addAction(tr("mute"), this, SLOT(onMute()));
  mute->setCheckable(true); mute->setChecked(false);
  QAction *pause = ctrl->addAction(tr("pause"), this, SLOT(onPause()));
  pause->setCheckable(true); pause->setChecked(false);
  ctrl->addAction(tr("stop"), this, SLOT(onStop()));

  QVBoxLayout *layout = new QVBoxLayout();
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
  qDebug() << "Not Implemented yet.";
}
