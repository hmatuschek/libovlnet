#ifndef SOCKSCONNECTION_H
#define SOCKSCONNECTION_H

#include <QObject>
#include "lib/buckets.hh"


class SOCKSWhitelist
{
public:
  SOCKSWhitelist(const QString &filename);

  bool empty() const;
  bool allowed(const Identifier &id) const;

protected:
  QSet<Identifier> _whitelist;
};

#endif // SOCKSCONNECTION_H
