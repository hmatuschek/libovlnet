#include "securecall.h"
#include "application.h"

SecureCall::SecureCall(Application &application, QObject *parent)
  : QObject(parent), SecureStream(application.identity()), _application(application)
{
  // Init encoder & decoder
  int err = 0;
  _encoder = opus_encoder_create(48000, 1, OPUS_APPLICATION_VOIP, &err);
  opus_encoder_ctl(_encoder, OPUS_SET_BITRATE(OPUS_BITRATE_MAX));
  opus_encoder_ctl(_encoder, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_MEDIUMBAND));
  opus_encoder_ctl(_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
  _decoder = opus_decoder_create(48000, 1, &err);
  //
}
