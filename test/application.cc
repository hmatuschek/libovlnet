#include "application.hh"

Application::Application(int &argc, char *argv[])
  : QCoreApplication(argc, argv)
{
  _id1 = Identity::newIdentity();
  _id2 = Identity::newIdentity();
  _node1 = new DHT(*_id1, QHostAddress::LocalHost, 7741, this);
  _node2 = new DHT(*_id2, QHostAddress::LocalHost, 7742, this);

  _bootstrapTimer.setInterval(500);
  _bootstrapTimer.setSingleShot(true);
  _bootstrapTimer.start();

  connect(&_bootstrapTimer, SIGNAL(timeout()), this, SLOT(_onStartUp()));
  connect(_node1, SIGNAL(nodeFound(NodeItem)), this, SLOT(_onNodeFound(NodeItem)));
}

void
Application::_onStartUp() {
  logDebug() << "Bootstrap...";
  // bootstrap
  _node1->ping(QHostAddress::LocalHost, 7742);
  _node2->ping(QHostAddress::LocalHost, 7741);
  disconnect(&_bootstrapTimer, SIGNAL(timeout()), this, SLOT(_onStartUp()));
  connect(&_bootstrapTimer, SIGNAL(timeout()), this, SLOT(_onSartTest()));
  _bootstrapTimer.start(500);
}

void
Application::_onSartTest() {
  _node1->findNode(_node2->id());
}

void
Application::_onNodeFound(NodeItem node) {
  _stream = new SecureStream(*_node1, this);
  connect(_stream, SIGNAL(established()), this, SLOT(_onConnected()));
  _node1->startConnection(0, node, _stream);
}

void
Application::_onConnected() {
  //_stream->write();
}



EchoStream::EchoStream(DHT &dht)
  : SecureStream(dht)
{
  connect(this, SIGNAL(readyRead()), this, SLOT(onDataAvailable()));
  connect(this, SIGNAL(bytesWritten(qint64)), this, SLOT(onDataWritten(qint64)));
}

void
EchoStream::onDataAvailable() {
  char buffer[0xffff];
  uint16_t len = this->read(buffer, 0xffff);
  if (len) {
    write(buffer, len);
  }
}

void
EchoStream::onDataWritten(qint64 len) {
  // pass...
}
