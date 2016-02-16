#include "dhtstatus.hh"
#include "lib/dht_config.h"
#include "application.hh"

#include <cmath>


DHTStatus::DHTStatus(Application &app, QObject *parent)
  : QObject(parent), _application(app)
{
  // pass...
}

QString
DHTStatus::identifier() const {
  return _application.dht().id().toBase32();
}

size_t
DHTStatus::numNeighbors() const {
  return _application.dht().numNodes();
}

size_t
DHTStatus::numStreams() const {
  return _application.dht().numSockets();
}

size_t
DHTStatus::bytesReceived() const {
  return _application.dht().bytesReceived();
}

size_t
DHTStatus::bytesSend() const {
  return _application.dht().bytesSend();
}

double
DHTStatus::inRate() const {
  return _application.dht().inRate();
}

double
DHTStatus::outRate() const {
  return _application.dht().outRate();
}

void
DHTStatus::neighbors(QList<QPair<double, bool> > &nodes) const {
  // Collect all nodes
  QList<NodeItem> nodeitems; _application.dht().nodes(nodeitems);
  QList<NodeItem>::iterator item = nodeitems.begin();
  for (; item != nodeitems.end(); item++) {
    // Compute log distance to self
    Distance d = _application.dht().id() - item->id();
    nodes.push_back(
          QPair<double, bool>(
            double(d.leadingBit())/(8*DHT_HASH_SIZE),
            _application.buddies().hasNode(item->id())));
  }
}
