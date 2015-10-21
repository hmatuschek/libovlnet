#ifndef HTTPSERVICE_H
#define HTTPSERVICE_H

#include <QString>
#include <QIODevice>
#include <QHash>


typedef enum {
  HTTP_GET, HTTP_HEAD, HTTP_POST,
  HTTP_INVALID_METHOD
} HttpMethod;


typedef enum {
  HTTP_1_0, HTTP_1_1, HTTP_INVALID_VERSION
} HttpVersion;

typedef enum {
  HTTP_OK = 200,
  HTTP_SERVER_ERROR = 500
} HttpResponseCode;


class HttpConnection: public QObject
{
  Q_OBJECT

public:
  HttpConnection(QIODevice *socket);

  inline QIODevice *socket() const { return _socket; }

protected:
  QIODevice *_socket;
};


class HttpRequest : public QObject
{
  Q_OBJECT

public:
  HttpRequest(HttpConnection *connection);

  inline HttpMethod method() const { return _method; }
  inline HttpVersion version() const { return _version; }
  inline const QString &path() const { return _path; }

  inline bool hasHeader(const QString &name) const {
    return _headers.contains(name);
  }

  inline QString header(const QString &name) const {
    return _headers[name];
  }


protected slots:
  void _onReadyRead();

protected:
  typedef enum {
    READ_REQUEST,
    READ_HEADER,
    READ_BODY
  } ParserState;

protected:
  HttpMethod _getMethod(const char *str, int len);
  HttpVersion _getVersion(const char *str, int len);

protected:
  HttpConnection *_connection;
  ParserState    _parserState;
  HttpMethod     _method;
  QString        _path;
  HttpVersion    _version;
  QHash<QString, QString> _headers;
};


class HttpResponse: public QObject
{
  Q_OBJECT

public:
  HttpResponse(HttpResponseCode resp, HttpConnection *connection);

  inline HttpResponseCode responseCode() const { return _code; }
  inline bool hasHeader(const QString &name) const { return _headers.contains(name); }
  inline void setHeader(const QString &name, const QString &value) {
    _headers.insert(name, value);
  }
  inline QString header(const QString &name) const {
    return _headers[name];
  }

  void sendHeaders();

protected slots:
  void onBytesWritten(qint64 bytes);

protected:
  HttpConnection *_connection;
  HttpResponseCode _code;
  bool _headersSend;
  qint64 _headerSendIdx;
  QByteArray _headerBuffer;
  QHash<QString, QString> _headers;
};


#endif // HTTPSERVICE_H
