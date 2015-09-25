#include "dhtstatusview.h"
#include <QString>
#include <QFormLayout>


DHTStatusView::DHTStatusView(DHTStatus *status, QWidget *parent) :
  QWidget(parent), _status(status), _updateTimer()
{
  _updateTimer.setInterval(5000);
  _updateTimer.setSingleShot(false);

  _numPeers = new QLabel(QString::number(_status->numNeighbors()));
  _numKeys  = new QLabel(QString::number(_status->numMappings()));
  _numData  = new QLabel(QString::number(_status->numDataItems()));

  QFormLayout *layout = new QFormLayout();
  layout->addRow(tr("Peers:"), _numPeers);
  layout->addRow(tr("Keys:"), _numKeys);
  layout->addRow(tr("Data items:"), _numData);
  setLayout(layout);

  QObject::connect(&_updateTimer, SIGNAL(timeout()), this, SLOT(_onUpdate()));
}

void
DHTStatusView::_onUpdate() {
  _numPeers->setText(QString::number(_status->numNeighbors()));
  _numKeys->setText(QString::number(_status->numMappings()));
  _numData->setText(QString::number(_status->numDataItems()));
}
