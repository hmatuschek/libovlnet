#ifndef SECURECALL_H
#define SECURECALL_H

#include "lib/crypto.h"
#include "opus.h"
#include "portaudio.h"

/** Specifies the number of frames per datagram. */
#define VLF_CALL_NUM_FRAMES 960
#define VLF_CALL_MAX_BUFFER_SIZE (DHT_MAX_MESSAGE_SIZE-DHT_HASH_SIZE-24)

#include <QObject>

class Application;


/** Target bit rate 12kb/s, mono, medium band and 20ms frame duration. At 48k sample rate,
 * this implies 960 samples per buffer/frame. */
class SecureCall : public QObject, public SecureStream
{
  Q_OBJECT

public:
  explicit SecureCall(Application &application, QObject *parent = 0);
  virtual ~SecureCall();

  void started();

  void handleDatagram(uint32_t seq, const uint8_t *data, size_t len);

protected:
  /** PortAudio callback for audio stream IO. */
  static int _handleFrames(const void *input, void *output,
                           unsigned long frameCount,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData);

protected:
  Application &_application;
  OpusEncoder *_encoder;
  OpusDecoder *_decoder;
  PaStream    *_paStream;
  uint32_t    _inFrameNumber;
  size_t      _inBufferSize;
  uint8_t     *_inBuffer[VLF_CALL_MAX_BUFFER_SIZE];
  uint32_t    _outFrameNumber;
};

#endif // SECURECALL_H
