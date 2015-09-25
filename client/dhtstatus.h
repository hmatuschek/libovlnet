#ifndef DHTSTATUS_H
#define DHTSTATUS_H

#include <QObject>
#include <QList>
#include "lib/dht.h"


/** Simple object to collect and compute some information about the status of the DHT node. */
class DHTStatus : public QObject
{
  Q_OBJECT

public:
  /** Constructor. */
  explicit DHTStatus(DHT *dht, QObject *parent = 0);

  /** Returns the number of neighbors in the routing table of the DHT node. */
  size_t numNeighbors() const;

  /** Returns the number of mappings in the hash table of this node. */
  size_t numMappings() const;

  /** Returns the number of data items provided by the node. */
  size_t numDataItems() const;

  /** Stores the neighbors of DHT node into the list with their distance on a logarithmic
   * scale (0,1]. */
  void neighbors(QList< QPair<double, NodeItem> > &nodes) const;

protected:
  DHT *_dht;
};

#endif // DHTSTATUS_H