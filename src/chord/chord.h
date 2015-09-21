#ifndef CHORD_H
#define CHORD_H

#include <QObject>
#include "libutp/utp.h"


class Chord : public QObject
{
  Q_OBJECT

public:
  explicit Chord(QObject *parent = 0);


};

#endif // CHORD_H
