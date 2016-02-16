#include "callwindow.hh"
#include <QCloseEvent>
#include <QToolBar>
#include <QAction>
#include <QVBoxLayout>
#include <QLabel>
#include "application.hh"


CallWindow::CallWindow(Application &application, SecureCall *call, QWidget *parent)
  : QWidget(parent), _application(application), _call(call)
{
  setWindowTitle("Call");
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
  _mute = ctrl->addAction(QIcon("://icons/bullhorn.png"), tr("mute"), this, SLOT(onMute()));
  _mute->setCheckable(true); _mute->setChecked(true);
  _silent = ctrl->addAction(QIcon("://icons/microphone.png"), tr("pause"), this, SLOT(onPause()));
  _silent->setCheckable(true); _silent->setChecked(true);
  _startStop = ctrl->addAction(QIcon("://icons/circle-x.png"), tr("end call"), this, SLOT(onStartStop()));
  if ((SecureCall::INITIALIZED == _call->state()) && _call->isIncomming()) {
    _startStop->setIcon(QIcon("://icons/circle-check.png"));
    _startStop->setText(tr("accept"));
  }

  QVBoxLayout *layout = new QVBoxLayout();
  layout->setContentsMargins(0,0,0,0);
  layout->setSpacing(0);
  layout->addWidget(label);
  layout->addWidget(ctrl);
  setLayout(layout);

  QObject::connect(_call, SIGNAL(started()), this, SLOT(onCallStarted()));
  QObject::connect(_call, SIGNAL(ended()), this, SLOT(onCallEnd()));
}

CallWindow::~CallWindow() {
  _call->deleteLater();
}

void
CallWindow::closeEvent(QCloseEvent *evt) {
  evt->accept();
  _call->hangUp();
}

void
CallWindow::onMute() {
  logInfo() << "Not implemented yet.";
}

void
CallWindow::onPause() {
  logInfo() << "Not implemented yet.";
}

void
CallWindow::onStartStop() {
  if ((SecureCall::INITIALIZED == _call->state()) && _call->isIncomming()) {
    _call->accept();
  } else if (SecureCall::RUNNING == _call->state()) {
    _call->hangUp();
  }
}

void
CallWindow::onCallStarted() {
  _startStop->setIcon(QIcon("://icons/circle-x.png"));
  _startStop->setText(tr("end call"));
}

void
CallWindow::onCallEnd() {
  this->deleteLater();
}
