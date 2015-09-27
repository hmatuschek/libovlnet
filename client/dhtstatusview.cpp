#include "dhtstatusview.h"
#include <QString>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>


DHTStatusView::DHTStatusView(DHTStatus *status, QWidget *parent) :
  QWidget(parent), _status(status), _updateTimer()
{
  _updateTimer.setInterval(5000);
  _updateTimer.setSingleShot(false);

  _numPeers = new QLabel(QString::number(_status->numNeighbors()));
  _numKeys  = new QLabel(QString::number(_status->numMappings()));
  _numData  = new QLabel(QString::number(_status->numDataItems()));
  _numStreams = new QLabel(QString::number(_status->numStreams()));


  _bytesReceived = new QLabel(_formatBytes(_status->bytesReceived()));
  _bytesSend = new QLabel(_formatBytes(_status->bytesSend()));
  _inRate = new QLabel(_formatRate(_status->inRate()));
  _outRate = new QLabel(_formatRate(_status->outRate()));

  _dhtNet   = new DHTNetGraph();
  QList<QPair<double, NodeItem> > nodes; _status->neighbors(nodes);
  _dhtNet->update(nodes);

  QVBoxLayout *layout= new QVBoxLayout();
  QHBoxLayout *row = new QHBoxLayout();
  QFormLayout *form = new QFormLayout();
  form->addRow(tr("Peers:"), _numPeers);
  form->addRow(tr("Keys:"), _numKeys);
  form->addRow(tr("Values:"), _numData);
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
  _numKeys->setText(QString::number(_status->numMappings()));
  _numData->setText(QString::number(_status->numDataItems()));
  _numStreams->setText(QString::number(_status->numStreams()));
  _bytesReceived->setText(_formatBytes(_status->bytesReceived()));
  _bytesSend->setText(_formatBytes(_status->bytesSend()));
  _inRate->setText(_formatRate(_status->inRate()));
  _outRate->setText(_formatRate(_status->outRate()));
  QList<QPair<double, NodeItem> > nodes; _status->neighbors(nodes);
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
    return QString("%1b/s").arg(QString::number(rate));
  }
  if (rate < 2e6) {
    return QString("%1kb").arg(QString::number(rate/1000.));
  }
  return QString("%1Mb/s").arg(QString::number(rate/1e6));
}
