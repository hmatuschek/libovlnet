#ifndef LOGGER_H
#define LOGGER_H

#include <QTextStream>
#include <QString>
#include <QDateTime>
#include <QIODevice>
#include <QBuffer>
#include <QList>
#include <QHostAddress>


class LogMessage
{
public:
  typedef enum {
    DEBUG = 0,
    INFO,
    WARNING,
    ERROR
  } Level;

public:
  LogMessage();
  LogMessage(const QString &filename, int line, Level level, const QString &message);
  LogMessage(const LogMessage &other);

  LogMessage &operator=(const LogMessage &other);

  const QString &filename() const;
  int linenumber() const;
  Level level() const;
  const QString &message() const;
  const QDateTime &timestamp() const;

protected:
  QString _filename;
  int _line;
  Level _level;
  QString _message;
  QDateTime _timestamp;
};


class LogMessageStream: public QTextStream
{
public:
  LogMessageStream(const QString &filename, int line, LogMessage::Level level);
  LogMessageStream(const LogMessageStream &other);
  virtual ~LogMessageStream();

protected:
  QString _filename;
  int _line;
  LogMessage::Level _level;
  QBuffer _buffer;
};

inline QTextStream &operator<<(QTextStream &stream, const QHostAddress &addr) {
  stream << addr.toString(); return stream;
}

class LogHandler
{
protected:
  LogHandler(LogMessage::Level level);

public:
  virtual ~LogHandler();

  virtual void handleMessage(const LogMessage &msg) = 0;

protected:
  LogMessage::Level _minLevel;
};


class IOLogHandler: public LogHandler
{
public:
  IOLogHandler(LogMessage::Level level=LogMessage::DEBUG, FILE *device=stderr);

  void handleMessage(const LogMessage &msg);

protected:
  QTextStream _stream;
};


class Logger
{
protected:
  Logger();
  virtual ~Logger();

public:
  static void log(const LogMessage &msg);
  static void addHandler(LogHandler *handler);

protected:
  static Logger *get();

protected:
  QList<LogHandler *> _handler;

protected:
  static Logger *_instance;
};

#define logDebug()   (LogMessageStream(__FILE__, __LINE__, LogMessage::DEBUG))
#define logInfo()    (LogMessageStream(__FILE__, __LINE__, LogMessage::INFO))
#define logWarning() (LogMessageStream(__FILE__, __LINE__, LogMessage::WARNING))
#define logError()   (LogMessageStream(__FILE__, __LINE__, LogMessage::ERROR))

#endif // LOGGER_H
