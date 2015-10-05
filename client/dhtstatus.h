#ifndef DHTSTATUS_H
#define DHTSTATUS_H

#include <QObject>
#include <QList>
#include "lib/dht.h"

// forward declarations
class Application;

/** Simple object to collect and compute some information about the status of the DHT node. */
class DHTStatus : public QObject
{
  Q_OBJECT

public:
  /** Constructor. */
  explicit DHTStatus(Application &app, QObject *parent = 0);

  QString identifier() const;

  /** Returns the number of neighbors in the routing table of the DHT node. */
  size_t numNeighbors() const;

  /** Returns the number of mappings in the hash table of this node. */
  size_t numMappings() const;

  /** Returns the number of data items provided by the node. */
  size_t numDataItems() const;

  /** Returns the number of active streams. */
  size_t numStreams() const;

  size_t bytesReceived() const;
  size_t bytesSend() const;
  double inRate() const;
  double outRate() const;

  /** Stores the neighbors of DHT node into the list with their distance on a logarithmic
   * scale (0,1]. */
  void neighbors(QList< QPair<double, bool> > &nodes) const;

protected:
  Application &_application;
};

#endif // DHTSTATUS_H
