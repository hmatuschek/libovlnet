#ifndef NATPMPCLIENT_H
#define NATPMPCLIENT_H

#include <QObject>

class NATPMPClient : public QObject
{
  Q_OBJECT
public:
  explicit NATPMPClient(QObject *parent = 0);

signals:

public slots:

};

#endif // NATPMPCLIENT_H
