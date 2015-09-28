#include "securecall.h"
#include "application.h"

SecureCall::SecureCall(Application &application, QObject *parent)
  : QObject(parent), SecureStream(application.identity()), _application(application),
    _encoder(0), _decoder(0), _paStream(0)
{
  // Init encoder & decoder
  int err = 0;
  _encoder = opus_encoder_create(48000, 1, OPUS_APPLICATION_VOIP, &err);
  if (OPUS_OK != err) {
    qDebug() << "Cannot setup opus encoder.";
    return;
  }
  // Choose by available buffer size
  opus_encoder_ctl(_encoder, OPUS_SET_BITRATE(OPUS_BITRATE_MAX));
  opus_encoder_ctl(_encoder, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_MEDIUMBAND));
  opus_encoder_ctl(_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
  // Create decoder
  _decoder = opus_decoder_create(48000, 1, &err);
  if (OPUS_OK != err) {
    qDebug() << "Cannot setup opus decoder.";
    return;
  }
  // Init port audio device
  PaError paerr = Pa_OpenDefaultStream(&_paStream, 1, 1, paInt16, 48000, 960,
                                       SecureCall::_handleFrames, this);
  if (0 != paerr) {
    qDebug() << "Can not configure PortAudio source:"
             << ((const char *)Pa_GetErrorText(err));
    return;
  }
}


int
SecureCall::_handleFrames(const void *input, void *output, unsigned long frameCount,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags, void *userData)
{
  SecureCall *self = reinterpret_cast<SecureCall *>(userData);

}
