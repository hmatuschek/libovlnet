#ifndef SECURECALL_H
#define SECURECALL_H

#include "lib/crypto.h"
#include "opus.h"
#include "portaudio.h"

/** Specifies the number of frames per datagram. */
#define VLF_CALL_NUM_FRAMES 960
/** Specifies the maximum size of the datagram payload (encoded audio). */
#define VLF_CALL_MAX_BUFFER_SIZE (DHT_MAX_MESSAGE_SIZE-DHT_HASH_SIZE-24)

#include <QObject>

// Forward declaration
class Application;

/** Target bit rate about 12kb/s, mono, wide band and 20ms frame duration. At 48k sample rate,
 * this implies 960 samples per buffer/frame. */
class SecureCall : public QObject, public SecureSocket
{
  Q_OBJECT

public:
  /** Possible states of the call. */
  typedef enum {
    INITIALIZED,   ///< Secure connection established, call not started yet.
    RUNNING,       ///< Call started.
    TERMINATED     ///< One side ended the call.
  } State;

public:
  /** Constructor.
   * @param incomming Indicates whether the call was initiated by the peer or this node.
   * @param application A weak reference to the application instance. */
  explicit SecureCall(bool incomming, Application &application);
  /** Destructor. */
  virtual ~SecureCall();

  /** Retruns the state of the call. */
  State state() const;
  /** Returns true if the call was initiated by the remote party. */
  bool isIncomming() const;

  /** Gets called by the dispatcher to signal a successful initiation of the secure connection. */
  void initialized();
  /** Handles incomming datagrams. */
  void handleDatagram(const uint8_t *data, size_t len);

public slots:
  /** Accept an incomming call. (Does nothing if the call is not incomming). */
  void accept();
  /** Terminates a call. */
  void hangUp();

signals:
  /** Gets emitted once the call starts. */
  void started();
  /** Gets emitted once the call ends. */
  void ended();

protected:
  /** PortAudio callback for audio stream IO. */
  static int _handleFrames(const void *input, void *output,
                           unsigned long frameCount,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData);

protected:
  /** If @c true, this stream was initiated by the remote party. */
  bool _incomming;
  /** A weak reference to the application instance. */
  Application &_application;
  /** The opus audio encoder. */
  OpusEncoder *_encoder;
  /** The opus audio decoder. */
  OpusDecoder *_decoder;
  /** The PortAudio stream. */
  PaStream    *_paStream;
  /** The current state of the call. */
  State       _state;
  /** The current framenumber of the input stream. */
  uint32_t    _inFrameNumber;
  /** The amount of data held by the input buffer. */
  size_t      _inBufferSize;
  /** Holds @c _inBufferSize bytes of encoded audio received from the peer. */
  uint8_t     *_inBuffer[VLF_CALL_MAX_BUFFER_SIZE];
  /** The current output frame number. */
  uint32_t    _outFrameNumber;
};

#endif // SECURECALL_H
