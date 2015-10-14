#include "logger.h"
#include <QFileInfo>

/* ********************************************************************************************* *
 * Implementation of LogMessage
 * ********************************************************************************************* */
LogMessage::LogMessage()
  : _filename(), _line(0), _level(DEBUG), _message(), _timestamp()
{
  // pass...
}

LogMessage::LogMessage(const QString &filename, int line, Level level, const QString &message)
  : _filename(filename), _line(line), _level(level), _message(message),
    _timestamp(QDateTime::currentDateTime())
{
  // pass...
}

LogMessage::LogMessage(const LogMessage &other)
  : _filename(other._filename), _line(other._line), _level(other._level), _message(other._message),
    _timestamp(other._timestamp)
{
  // pass...
}

LogMessage &
LogMessage::operator =(const LogMessage &other) {
  _filename  = other._filename;
  _line      = other._line;
  _level     = other._level;
  _timestamp = other._timestamp;
  return *this;
}

const QString &
LogMessage::filename() const {
  return _filename;
}

int
LogMessage::linenumber() const {
  return _line;
}

LogMessage::Level
LogMessage::level() const {
  return _level;
}

const QString &
LogMessage::message() const {
  return _message;
}

const QDateTime &
LogMessage::timestamp() const {
  return _timestamp;
}


/* ********************************************************************************************* *
 * Implementation of LogMessageStream
 * ********************************************************************************************* */
LogMessageStream::LogMessageStream(const QString &filename, int line, LogMessage::Level level)
  : QTextStream(), _filename(filename), _line(line), _level(level), _buffer()
{
  if (_buffer.open(QIODevice::Append)) {
    setDevice(&_buffer);
  }
}

LogMessageStream::LogMessageStream(const LogMessageStream &other)
  : QTextStream(), _filename(other._filename), _line(other._line), _level(other._level), _buffer()
{
  if (_buffer.open(QIODevice::ReadWrite)) {
    _buffer.write(other._buffer.buffer());
    setDevice(&_buffer);
  }
}

LogMessageStream::~LogMessageStream() {
  if (_buffer.isOpen()) {
    flush();
    Logger::log(LogMessage(_filename, _line, _level, QString::fromLocal8Bit(_buffer.buffer())));
    setDevice(0);
  }
}


/* ********************************************************************************************* *
 * Implementation of LogHandler
 * ********************************************************************************************* */
LogHandler::LogHandler(LogMessage::Level level)
  : _minLevel(level)
{
  // pass...
}

LogHandler::~LogHandler() {
  // pass...
}


/* ********************************************************************************************* *
 * Implementation of IOLogHandler
 * ********************************************************************************************* */
IOLogHandler::IOLogHandler(LogMessage::Level level, FILE *device)
  : LogHandler(level), _stream(device)
{
  // pass...
}

void
IOLogHandler::handleMessage(const LogMessage &msg) {
  if (msg.level() < _minLevel) { return; }
  switch (msg.level()) {
  case LogMessage::DEBUG: _stream << "DEBUG: "; break;
  case LogMessage::INFO: _stream << "INFO: "; break;
  case LogMessage::WARNING: _stream << "WARNING: "; break;
  case LogMessage::ERROR: _stream << "ERROR: "; break;
  }
  QFileInfo info(msg.filename());
  _stream << msg.timestamp().time().toString() << ", @"  << info.fileName()
          << " line " << msg.linenumber() << ": " << msg.message() << "\n";
  _stream.flush();
}


/* ********************************************************************************************* *
 * Implementation of Logger
 * ********************************************************************************************* */
Logger *Logger::_instance = 0;

Logger::Logger()
  : _handler()
{
  // pass...
}

Logger::~Logger() {
  QList<LogHandler *>::iterator handler = _handler.begin();
  for (; handler != _handler.end(); handler++) {
    delete (*handler);
  }
}

Logger *
Logger::get() {
  if (0 == _instance) {
    _instance = new Logger();
  }
  return _instance;
}

void
Logger::log(const LogMessage &msg) {
  Logger *self = Logger().get();
  QList<LogHandler *>::iterator handler = self->_handler.begin();
  for (; handler != self->_handler.end(); handler++) {
    (*handler)->handleMessage(msg);
  }
}

void
Logger::addHandler(LogHandler *handler) {
  Logger *self = Logger().get();
  self->_handler.append(handler);
}
