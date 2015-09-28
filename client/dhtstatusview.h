#ifndef DHTSTATUSVIEW_H
#define DHTSTATUSVIEW_H

#include "dhtstatus.h"
#include "dhtnetgraph.h"
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
  QString _formatBytes(size_t bytes);
  QString _formatRate(double rate);
  void closeEvent(QCloseEvent *evt);

protected:
  DHTStatus *_status;

  QLabel *_numPeers;
  QLabel *_numKeys;
  QLabel *_numData;
  QLabel *_numStreams;
  QLabel *_bytesReceived;
  QLabel *_bytesSend;
  QLabel *_inRate;
  QLabel *_outRate;

  DHTNetGraph *_dhtNet;

  QTimer _updateTimer;
};

#endif // DHTSTATUSVIEW_H
