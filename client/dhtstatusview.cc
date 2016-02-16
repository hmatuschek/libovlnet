#include "dhtstatusview.hh"
#include "logwindow.hh"

#include <QString>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QTabWidget>

#include "application.hh"


/* ******************************************************************************************** *
 * Implementation of DHTStatusWindow
 * ******************************************************************************************** */
DHTStatusWindow::DHTStatusWindow(Application &app, QWidget *parent)
  : QWidget(parent)
{
  setWindowTitle(tr("Overlay network status"));

  QTabWidget *tabs = new QTabWidget();
  tabs->addTab(new DHTStatusView(app), QIcon("://icons/dashboard.png"), tr("Status"));
  tabs->addTab(new LogWidget(app), QIcon("://icons/list.png"), tr("Log"));

  QVBoxLayout *layout = new QVBoxLayout();
  layout->addWidget(tabs);
  setLayout(layout);
}

void
DHTStatusWindow::closeEvent(QCloseEvent *evt) {
  evt->accept();
  deleteLater();
}


/* ******************************************************************************************** *
 * Implementation of DHTStatusView
 * ******************************************************************************************** */
DHTStatusView::DHTStatusView(Application &app, QWidget *parent) :
  QWidget(parent), _status(&app.status()), _updateTimer()
{
  _updateTimer.setInterval(5000);
  _updateTimer.setSingleShot(false);

  _numPeers = new QLabel(QString::number(_status->numNeighbors()));
  _numStreams = new QLabel(QString::number(_status->numStreams()));


  _bytesReceived = new QLabel(_formatBytes(_status->bytesReceived()));
  _bytesSend = new QLabel(_formatBytes(_status->bytesSend()));
  _inRate = new QLabel(_formatRate(_status->inRate()));
  _outRate = new QLabel(_formatRate(_status->outRate()));

  _dhtNet   = new DHTNetGraph();
  QList<QPair<double, bool> > nodes; _status->neighbors(nodes);
  _dhtNet->update(nodes);

  QVBoxLayout *layout= new QVBoxLayout();
  layout->addWidget(new QLabel(tr("Identifier: <b>%1</b>").arg(_status->identifier())));
  QHBoxLayout *row = new QHBoxLayout();
  QFormLayout *form = new QFormLayout();
  form->addRow(tr("Peers:"), _numPeers);
  form->addRow(tr("Active streams:"), _numStreams);
  row->addLayout(form);
  form = new QFormLayout();
  form->addRow(tr("Received:"), _bytesReceived);
  form->addRow(tr("Send:"), _bytesSend);
  form->addRow(tr("In rate:"), _inRate);
  form->addRow(tr("Out rate:"), _outRate);
  row->addLayout(form);
  layout->addLayout(row);
  layout->addWidget(_dhtNet);
  setLayout(layout);

  QObject::connect(&_updateTimer, SIGNAL(timeout()), this, SLOT(_onUpdate()));

  _updateTimer.start();
}

void
DHTStatusView::_onUpdate() {
  _numPeers->setText(QString::number(_status->numNeighbors()));
  _numStreams->setText(QString::number(_status->numStreams()));
  _bytesReceived->setText(_formatBytes(_status->bytesReceived()));
  _bytesSend->setText(_formatBytes(_status->bytesSend()));
  _inRate->setText(_formatRate(_status->inRate()));
  _outRate->setText(_formatRate(_status->outRate()));
  QList<QPair<double, bool> > nodes; _status->neighbors(nodes);
  _dhtNet->update(nodes);
}

QString
DHTStatusView::_formatBytes(size_t bytes) {
  if (bytes < 2000UL) {
    return QString("%1b").arg(QString::number(bytes));
  }
  if (bytes < 2000000UL) {
    return QString("%1kb").arg(QString::number(bytes/1000UL));
  }
  return QString("%1Mb").arg(QString::number(bytes/1000000UL));
}


QString
DHTStatusView::_formatRate(double rate) {
  if (rate < 2000.0) {
    return QString("%1b/s").arg(QString::number(rate, 'f', 1));
  }
  if (rate < 2e6) {
    return QString("%1kb/s").arg(QString::number(rate/1000., 'f', 1));
  }
  return QString("%1Mb/s").arg(QString::number(rate/1e6, 'f', 1));
}

