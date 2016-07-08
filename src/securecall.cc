#include "securecall.hh"
#include "node.hh"
#include <netinet/in.h>

SecureCall::SecureCall(bool incomming, Network &net)
  : QObject(0), SecureSocket(net),
    _incomming(incomming), _encoder(0), _decoder(0), _paStream(0)
{
  // Init encoder
  int err = 0;
  _encoder = opus_encoder_create(48000, 1, OPUS_APPLICATION_VOIP, &err);
  if (OPUS_OK != err) {
    logError() << "Cannot setup opus encoder:" << opus_strerror(err);
    return;
  }
  opus_encoder_ctl(_encoder, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_WIDEBAND));
  opus_encoder_ctl(_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));

  // Init decoder
  _decoder = opus_decoder_create(48000, 1, &err);
  if (OPUS_OK != err) {
    logError() << "Cannot setup opus decoder:" << opus_strerror(err);
    return;
  }

  // Init port audio device
  PaError paerr = Pa_OpenDefaultStream(&_paStream, 1, 1, paInt16, 48000, OVL_CALL_NUM_FRAMES,
                                       SecureCall::_handleFrames, this);
  if (paNoError != paerr) {
    logError() << "Can not configure PortAudio source:"
               << ((const char *)Pa_GetErrorText(err));
    return;
  }
}

SecureCall::~SecureCall() {
  if (_paStream) { Pa_CloseStream(_paStream); }
  if (_encoder) { opus_encoder_destroy(_encoder); }
  if (_decoder) { opus_decoder_destroy(_decoder); }
}

SecureCall::State
SecureCall::state() const {
  return _state;
}

bool
SecureCall::isIncomming() const {
  return _incomming;
}

void
SecureCall::initialized() {
  logDebug() << "SecureCall stream initialized.";
  _state = INITIALIZED;
}

void
SecureCall::accept() {
  // accept an incomming call
  if (_incomming && (INITIALIZED == _state)) {
    _state = RUNNING;
    if (_paStream) {
      Pa_StartStream(_paStream);
      logDebug() << "Audio stream started.";
    }
    emit started();
  }
}

void
SecureCall::hangUp() {
  _state = TERMINATED;
  Pa_StopStream(_paStream);
  // Send empty data
  sendNull();
  // signal call has ended
  emit ended();
}

void
SecureCall::handleDatagram(const uint8_t *data, size_t len) {
  if (len>=4) {
    // If the first data arives and the outgoing stream has not been started yet
    //  -> start audio device
    if ((INITIALIZED == _state) && (! _incomming)) {
      _state = RUNNING;
      if (_paStream) {
        Pa_StartStream(_paStream);
        logDebug() << "Audio stream started.";
      }
      emit started();
    }
    // Store payload in
    _inFrameNumber = ntohl(*(uint32_t *)data); data += 4; len -= 4;
    _inBufferSize = len;
    memcpy(_inBuffer, data, len);
  } else if ((0 == len) && (0 == data)) {
    // An null datagram indicates end of stream,
    if (RUNNING == _state) {
      logDebug() << "Null datagram received -> stop stream.";
      _state = TERMINATED;
      Pa_StopStream(_paStream);
      emit ended();
    }
  }
}

int
SecureCall::_handleFrames(const void *input, void *output, unsigned long frameCount,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags, void *userData)
{
  SecureCall *self = reinterpret_cast<SecureCall *>(userData);
  int outFrames = 0;
  // Decode received data and store it in output
  if (self->_inBufferSize) {
    outFrames = opus_decode(self->_decoder,
                            (const unsigned char *)self->_inBuffer, self->_inBufferSize,
                            (int16_t *)output, OVL_CALL_NUM_FRAMES, 0);
    self->_inBufferSize = 0;
  }
  // assemble datagram
  uint8_t outBuffer[OVL_CALL_MAX_BUFFER_SIZE+4];
  // store frame number
  *(uint32_t *)outBuffer = htonl(self->_outFrameNumber);
  // encode frame
  int32_t outlen = opus_encode(self->_encoder, (int16_t *) input, frameCount,
                               outBuffer+4, OVL_CALL_MAX_BUFFER_SIZE);
  // update frame number
  self->_outFrameNumber += frameCount;
  // send encoded frame
  self->sendDatagram(outBuffer, outlen+4);
  // Return number of decoded frames
  return paContinue;
}
