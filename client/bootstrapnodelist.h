#ifndef BOOTSTRAPNODELIST_H
#define BOOTSTRAPNODELIST_H

#include <QList>
#include <QString>
#include <QPair>
#include <QFile>
#include <inttypes.h>


class BootstrapNodeList : public QList< QPair<QString, uint16_t> >
{
public:
  /** Empty constructor. */
  BootstrapNodeList();
  /** Read list from file. */
  explicit BootstrapNodeList(const QString &path);
  /** Copy constructor. */
  BootstrapNodeList(const BootstrapNodeList &path);

  /** Assignment operator. */
  BootstrapNodeList &operator= (const BootstrapNodeList &other);

  void insert(const QString &host, uint16_t port);

protected:
  QFile _file;
};

#endif // BOOTSTRAPNODELIST_H
