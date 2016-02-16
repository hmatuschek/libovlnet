#ifndef DHTSTATUSVIEW_H
#define DHTSTATUSVIEW_H

#include "dhtstatus.hh"
#include "dhtnetgraph.hh"
#include <QWidget>
#include <QTimer>
#include <QLabel>


class DHTStatusWindow: public QWidget
{
  Q_OBJECT

public:
  DHTStatusWindow(Application &app, QWidget *parent=0);

protected:
  void closeEvent(QCloseEvent *evt);
};


class DHTStatusView : public QWidget
{
  Q_OBJECT

public:
  explicit DHTStatusView(Application &app, QWidget *parent = 0);


protected slots:
  void _onUpdate();

protected:
  QString _formatBytes(size_t bytes);
  QString _formatRate(double rate);

protected:
  DHTStatus *_status;

  QLabel *_numPeers;
  QLabel *_numStreams;
  QLabel *_bytesReceived;
  QLabel *_bytesSend;
  QLabel *_inRate;
  QLabel *_outRate;

  DHTNetGraph *_dhtNet;

  QTimer _updateTimer;
};

#endif // DHTSTATUSVIEW_H
