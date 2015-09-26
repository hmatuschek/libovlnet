#include "dhtstatus.h"

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

void
DHTStatus::neighbors(QList< QPair<double, NodeItem> > &nodes) const {

}
