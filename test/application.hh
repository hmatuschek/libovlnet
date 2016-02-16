#ifndef APPLICATION_HH
#define APPLICATION_HH

#include "lib/dht.h"
#include "lib/crypto.h"
#include "lib/stream.h"
#include <QCoreApplication>
#include <QTimer>


class EchoStream: public SecureStream
{
  Q_OBJECT

public:
  EchoStream(DHT &dht);

protected slots:
  void onDataAvailable();
  void onDataWritten(qint64 len);
};


class Application: public QCoreApplication
{
  Q_OBJECT

public:
  Application(int &argc, char *argv[]);

protected slots:
  void _onStartUp();
  void _onSartTest();
  void _onNodeFound(NodeItem node);
  void _onConnected();

protected:
  Identity *_id1;
  Identity *_id2;
  DHT *_node1;
  DHT *_node2;
  SecureStream *_stream;

  QTimer _bootstrapTimer;
};


#endif // APPLICATION_HH
