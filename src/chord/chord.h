#ifndef CHORD_H
#define CHORD_H

#include <QObject>
#include "transport.h"

class Chord : public QObject
{
  Q_OBJECT

public:
  explicit Chord(QObject *parent = 0);
  virtual ~Chord();
};

#endif // CHORD_H
