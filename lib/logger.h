#ifndef LOGGER_H
#define LOGGER_H

#include <QTextStream>
#include <QString>
#include <QDateTime>
#include <QIODevice>
#include <QBuffer>
#include <QList>
#include <QHostAddress>


/** A log message.
 * @ingroup utils */
class LogMessage
{
public:
  /** Specifies possible log levels. */
  typedef enum {
    DEBUG = 0,  ///< Debug information.
    INFO,       ///< Runtime & status information.
    WARNING,    ///< Warnings and minor issues.
    ERROR       ///< Errors and other major issues.
  } Level;

public:
  /** Empty constructor. */
  LogMessage();
  /** Constructor from @c filename, @c line, log @c level and @c message text. */
  LogMessage(const QString &filename, int line, Level level, const QString &message);
  /** Copy constructor. */
  LogMessage(const LogMessage &other);
  /** Assignment operator. */
  LogMessage &operator=(const LogMessage &other);

  /** Returns the filename, where the message originated. */
  const QString &filename() const;
  /** Returns the line number, where the message originated. */
  int linenumber() const;
  /** Returns the level of the message. */
  Level level() const;
  /** Returns the actual message. */
  const QString &message() const;
  /** Returns the timestamp of the message. */
  const QDateTime &timestamp() const;

protected:
  /** The filename where the message originated. */
  QString _filename;
  /** The line number where the message originated. */
  int _line;
  /** The message level. */
  Level _level;
  /** The actual message. */
  QString _message;
  /** The timestamp of the message. */
  QDateTime _timestamp;
};


/** A stream object assembling a log message. Upon destruction, a log message gets assembled and
 * passed to the logger.
 * @ingroup internal */
class LogMessageStream: public QTextStream
{
public:
  /** Constructs a ne log message stream at the position @c filename & @c line with the specified
   * log @c level. Usually the @c logDebug, @c logInfo, @c logWarning or @c logError macros are
   * used to instantiate a @c LogMessageStream. */
  LogMessageStream(const QString &filename, int line, LogMessage::Level level);
  /** Copy constructor. */
  LogMessageStream(const LogMessageStream &other);
  /** Destructor. Upon destruction, a log message will be assembled and send to the @c Logger
   * instance. */
  virtual ~LogMessageStream();

protected:
  /** The name of the file where the message originated. */
  QString _filename;
  /** The line where the message originated. */
  int _line;
  /** Level of the log message. */
  LogMessage::Level _level;
  /** The buffer for the message text. */
  QBuffer _buffer;
};


/** Implements the stream operator of the QHostAddress type. */
inline QTextStream &operator<<(QTextStream &stream, const QHostAddress &addr) {
  stream << addr.toString(); return stream;
}


/** The base class of all log-handlers.
 * @ingroup utils */
class LogHandler
{
protected:
  /** Hidden constructor. */
  LogHandler(LogMessage::Level level);

public:
  /** Destructor. */
  virtual ~LogHandler();

  /** Needs to be implemented to handle log messages. */
  virtual void handleMessage(const LogMessage &msg) = 0;

protected:
  /** Minimum log level to process. */
  LogMessage::Level _minLevel;
};


/** Serializes log messages to the given file.
 * @ingroup utils */
class IOLogHandler: public LogHandler
{
public:
  /** Constructor.
   * @param level Spicifies the mimimum log level to process.
   * @param device File to log to, default: stderr. */
  IOLogHandler(LogMessage::Level level=LogMessage::DEBUG, FILE *device=stderr);

  /** Implements the @c LogHandler interface. */
  void handleMessage(const LogMessage &msg);

protected:
  /** A textstream to serialize into. */
  QTextStream _stream;
};


/** A singleton logger class.
 * @ingroup utils */
class Logger
{
protected:
  /** Hidden constructor. */
  Logger();

public:
  /** Destructor. */
  virtual ~Logger();

  /** Logs a message. */
  static void log(const LogMessage &msg);
  /** Adds a handler to the logger, the ownership is transferred to the @c Logger. */
  static void addHandler(LogHandler *handler);

protected:
  /** Factory method. */
  static Logger *get();

protected:
  /** The list of registered handlers. */
  QList<LogHandler *> _handler;

protected:
  /** The singleton instance. */
  static Logger *_instance;
};

/** Convenience macro to create a @c LogMessageStream with log level "DEBUG".
 * @ingroup utils */
#define logDebug()   (LogMessageStream(__FILE__, __LINE__, LogMessage::DEBUG))
/** Convenience macro to create a @c LogMessageStream with log level "INFO".
 * @ingroup utils */
#define logInfo()    (LogMessageStream(__FILE__, __LINE__, LogMessage::INFO))
/** Convenience macro to create a @c LogMessageStream with log level "Warning".
 * @ingroup utils */
#define logWarning() (LogMessageStream(__FILE__, __LINE__, LogMessage::WARNING))
/** Convenience macro to create a @c LogMessageStream with log level "ERROR".
 * @ingroup utils */
#define logError()   (LogMessageStream(__FILE__, __LINE__, LogMessage::ERROR))

#endif // LOGGER_H
