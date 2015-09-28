#ifndef SECURECALL_H
#define SECURECALL_H

#include "lib/crypto.h"
#include "opus.h"
#include <QObject>

class Application;


/** Target bit rate 12kb/s, mono, medium band. */
class SecureCall : public QObject, public SecureStream
{
  Q_OBJECT

public:
  explicit SecureCall(Application &application, QObject *parent = 0);

protected:
  Application &_application;
  OpusEncoder *_encoder;
  OpusDecoder *_decoder;
};

#endif // SECURECALL_H
