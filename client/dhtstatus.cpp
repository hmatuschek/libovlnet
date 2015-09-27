#include "dhtstatus.h"
#include "lib/dht_config.h"

#include <cmath>


DHTStatus::DHTStatus(DHT *dht, QObject *parent)
  : QObject(parent), _dht(dht)
{
  // pass...
}

const Identifier &
DHTStatus::identifier() const {
  return _dht->id();
}

size_t
DHTStatus::numNeighbors() const {
  return _dht->numNodes();
}

size_t
DHTStatus::numMappings() const {
  return _dht->numKeys();
}

size_t
DHTStatus::numDataItems() const {
  return _dht->numData();
}

size_t
DHTStatus::numStreams() const {
  return _dht->numStreams();
}

size_t
DHTStatus::bytesReceived() const {
  return _dht->bytesReceived();
}

size_t
DHTStatus::bytesSend() const {
  return _dht->bytesSend();
}

double
DHTStatus::inRate() const {
  return _dht->inRate();
}

double
DHTStatus::outRate() const {
  return _dht->outRate();
}

void
DHTStatus::neighbors(QList< QPair<double, NodeItem> > &nodes) const {
  // Collect all nodes
  QList<NodeItem> nodeitems; _dht->nodes(nodeitems);
  QList<NodeItem>::iterator item = nodeitems.begin();
  for (; item != nodeitems.end(); item++) {
    // Compute log distance to self
    Distance d = _dht->id() - item->id();
    nodes.push_back(
          QPair<double, NodeItem>(double(d.leadingBit())/(8*DHT_HASH_SIZE),*item));
  }
}
