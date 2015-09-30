#include "securecall.h"
#include "application.h"
#include <netinet/in.h>

SecureCall::SecureCall(Application &application, QObject *parent)
  : QObject(parent), SecureStream(application.identity()), _application(application),
    _encoder(0), _decoder(0), _paStream(0)
{
  // Init encoder
  int err = 0;
  _encoder = opus_encoder_create(48000, 1, OPUS_APPLICATION_VOIP, &err);
  if (OPUS_OK != err) {
    qDebug() << "Cannot setup opus encoder:" << opus_strerror(err);
    return;
  }
  opus_encoder_ctl(_encoder, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_WIDEBAND));
  opus_encoder_ctl(_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));

  // Init decoder
  _decoder = opus_decoder_create(48000, 1, &err);
  if (OPUS_OK != err) {
    qDebug() << "Cannot setup opus decoder:" << opus_strerror(err);
    return;
  }

  // Init port audio device
  PaError paerr = Pa_OpenDefaultStream(&_paStream, 1, 1, paInt16, 48000, VLF_CALL_NUM_FRAMES,
                                       SecureCall::_handleFrames, this);
  if (paNoError != paerr) {
    qDebug() << "Can not configure PortAudio source:"
             << ((const char *)Pa_GetErrorText(err));
    return;
  }
}

SecureCall::~SecureCall() {
  if (_paStream) { Pa_CloseStream(_paStream); }
  if (_encoder) { opus_encoder_destroy(_encoder); }
  if (_decoder) { opus_decoder_destroy(_decoder); }
  _application.dht().closeStream(_streamId);
}

void
SecureCall::started() {
  qDebug() << "SecureCall stream started.";
  if (_paStream) {
    Pa_StartStream(_paStream);
    qDebug() << "Audio stream started.";
  }
}

void
SecureCall::handleDatagram(uint32_t seq, const uint8_t *data, size_t len) {
  if (len<4) { return; }
  _inFrameNumber = ntohl(*(uint32_t *)data); data += 4; len -= 4;
  _inBufferSize = len;
  memcpy(_inBuffer, data, len);
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
                            (int16_t *)output, VLF_CALL_NUM_FRAMES, 0);
    self->_inBufferSize = 0;
  }
  // assemble datagram
  uint8_t outBuffer[VLF_CALL_MAX_BUFFER_SIZE+4];
  // store frame number
  *(uint32_t *)outBuffer = htonl(self->_outFrameNumber);
  // encode frame
  int32_t outlen = opus_encode(self->_encoder, (int16_t *) input, frameCount,
                               outBuffer+4, VLF_CALL_MAX_BUFFER_SIZE);
  // update frame number
  self->_outFrameNumber += frameCount;
  // send encoded frame
  self->sendDatagram(outBuffer, outlen+4);
  // Return number of decoded frames
  return paContinue;
}
