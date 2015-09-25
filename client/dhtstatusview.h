#ifndef DHTSTATUSVIEW_H
#define DHTSTATUSVIEW_H

#include "dhtstatus.h"
#include <QWidget>
#include <QTimer>
#include <QLabel>

class DHTStatusView : public QWidget
{
  Q_OBJECT

public:
  explicit DHTStatusView(DHTStatus *status, QWidget *parent = 0);


protected slots:
  void _onUpdate();

protected:
  DHTStatus *_status;

  QLabel *_numPeers;
  QLabel *_numKeys;
  QLabel *_numData;

  QTimer _updateTimer;
};

#endif // DHTSTATUSVIEW_H
