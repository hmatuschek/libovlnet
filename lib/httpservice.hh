/** @defgroup http Http service
 * @ingroup services */

#ifndef HTTPSERVICE_H
#define HTTPSERVICE_H

#include <QString>
#include <QIODevice>
#include <QHash>
#include <QTcpServer>

#include "crypto.hh"
#include "stream.hh"

/** Enum of implemented HTTP methods.
 * @ingroup http */
typedef enum {
  HTTP_GET,            ///< Get method.
  HTTP_HEAD,           ///< Head method.
  HTTP_POST,           ///< Post method.
  HTTP_INVALID_METHOD  ///< Invalid method.
} HttpMethod;


/** Supported HTTP versions.
 * @ingroup http */
typedef enum {
  HTTP_1_0,             ///< Version 1.0
  HTTP_1_1,             ///< Version 1.1
  HTTP_INVALID_VERSION  ///< Invalid version number.
} HttpVersion;


/** Possible HTTP response codes.
 * @ingroup http */
typedef enum {
  HTTP_RESP_INCOMPLETE = 0, ///< A dummy response code to indicate an incomplete response header.
  HTTP_OK = 200,            ///< OK.
  HTTP_BAD_REQUEST = 400,   ///< Bad requrst.
  HTTP_FORBIDDEN = 403,     ///< Forbidden.
  HTTP_NOT_FOUND = 404,     ///< Resource not found.
  HTTP_SERVER_ERROR = 500,  ///< Internal error.
  HTTP_BAD_GATEWAY = 502    ///< Bad Gateway
} HttpResponseCode;


// Forward declarations
class HttpRequest;
class HttpResponse;

/** Abstract interface for a Http request handler.
 * @ingroup http */
class HttpRequestHandler: public QObject
{
  Q_OBJECT

protected:
  /** Hidden constructor. */
  HttpRequestHandler(QObject *parent=0);

public:
  /** Destructor. */
  virtual ~HttpRequestHandler();

  /** Returns @c true if the given request can be processed.
   * This method needs to be implemented by every request handler. */
  virtual bool acceptReqest(HttpRequest *request) = 0;
  /** Returns the response object for the given request.
   * This method needs to be implemented by every request handler. */
  virtual HttpResponse *processRequest(HttpRequest *request) = 0;
};


/** A local HTTP server.
 * This class implements a HTTP server listening on the localhost.
 * @ingroup http */
class LocalHttpServer: public QObject
{
  Q_OBJECT

public:
  /** Constructs a local http server.
   * @param dispatcher Specifies the HTTP request handler instance.
   * @param port Specifies the TCP port to listen on. */
  LocalHttpServer(HttpRequestHandler *dispatcher, uint16_t port=8080);
  /** Destructor. */
  virtual ~LocalHttpServer();

protected slots:
  /** Gets called on incomming connections. */
  void _onNewConnection();

protected:
  /** The request handler instance. */
  HttpRequestHandler *_dispatcher;
  /** The local TCP server. */
  QTcpServer _server;
};


/** Represents a connection to a HTTP client.
 * @ingroup http */
class HttpConnection: public QObject
{
  Q_OBJECT

public:
  /** Constructs a new HTTP connection.
   * @param service Specifies the request handler instance for the connection.
   * @param socket Specifies the socket of the connection. */
  HttpConnection(HttpRequestHandler *service, const NodeItem &remote, QIODevice *socket);
  /** Destructor. */
  virtual ~HttpConnection();

  /** Returns the socket of the connection. */
  inline QIODevice *socket() const { return _socket; }
  /** Returns the remote node. */
  inline NodeItem remote() const { return _remote; }

protected slots:
  /** Gets called if a request header has been read. */
  void _requestHeadersRead();
  /** Gets called if a request is malformed. */
  void _badRequest();
  /** Gets called once a response is completed. */
  void _responseCompleted();

protected:
  /** The request handler instance. */
  HttpRequestHandler *_service;
  /** Remote node. */
  NodeItem _remote;
  /** The connection socket. */
  QIODevice *_socket;
  /** The current request being parsed or processed. */
  HttpRequest *_currentRequest;
  /** The current response being send. */
  HttpResponse *_currentResponse;
};


/** Implements a HTTP request (parser).
 * @ingroup http */
class HttpRequest : public QObject
{
  Q_OBJECT

public:
  typedef QHash<QString, QString>::const_iterator iterator;

public:
  /** Constructs a request parser for the given connection. */
  HttpRequest(HttpConnection *connection);

  /** Starts reading the HTTP request. */
  void parse();

  /** Returns the connection instance of the request. */
  inline HttpConnection *connection() const { return _connection; }
  /** Returns the request method. */
  inline HttpMethod method() const { return _method; }
  /** Returns the HTTP version. */
  inline HttpVersion version() const { return _version; }
  /** Returns the resource path. */
  inline const QString &path() const { return _path; }
  /** Returns @c true if the specified header was set. */
  inline bool hasHeader(const QString &name) const {
    return _headers.contains(name);
  }
  /** Returns the value of the specified header. */
  inline QString header(const QString &name) const {
    return _headers[name];
  }
  /** Returns @c true if the connection is keep-alive. */
  inline bool isKeepAlive() const {
    return ((HTTP_1_1 == _version) ||
            (hasHeader("Connection") && ("keep-alive"==header("Connection"))));
  }

  inline iterator begin() const { return _headers.begin(); }
  inline iterator end() const { return _headers.end(); }

signals:
  /** Gets emitted once the headers has been read. */
  void headerRead();
  /** Gets emitted if the request is malformed. */
  void badRequest();

protected slots:
  /** Gets called on arriving data. */
  void _onReadyRead();

protected:
  /** Possible parser states. */
  typedef enum {
    READ_REQUEST, ///< Start, read request line.
    READ_HEADER,  ///< Read header lines.
    READ_BODY     ///< Finished.
  } ParserState;

protected:
  /** Map string to method. */
  HttpMethod _getMethod(const char *str, int len);
  /** Map string to HTTP version. */
  HttpVersion _getVersion(const char *str, int len);

protected:
  /** The connection of the request. */
  HttpConnection *_connection;
  /** The parser state. */
  ParserState    _parserState;
  /** The HTTP method. */
  HttpMethod     _method;
  /** The requested resource. */
  QString        _path;
  /** The HTTP version. */
  HttpVersion    _version;
  /** Table of headers. */
  QHash<QString, QString> _headers;
};


/** A HTTP response.
 * @ingroup http */
class HttpResponse: public QObject
{
  Q_OBJECT

protected:
  /** Hidden constructor.
   * @param resp Specifies the response code.
   * @param connection Specifies the HTTP connection for the response. */
  HttpResponse(HttpVersion version, HttpResponseCode resp, HttpConnection *connection);

public:
  /** Returns the response code. */
  inline HttpResponseCode responseCode() const { return _code; }
  /** Resets the response code. */
  inline void setResponseCode(HttpResponseCode code) { _code = code; }
  /** Returns @c true if the given header has been set. */
  inline bool hasHeader(const QString &name) const { return _headers.contains(name); }
  /** Set a header. */
  inline void setHeader(const QString &name, const QString &value) {
    _headers.insert(name, value);
  }
  /** Returns the value of the given header. */
  inline QString header(const QString &name) const {
    return _headers[name];
  }

  /** Start response. */
  void sendHeaders();

signals:
  /** Gets emitted once the response has been started. That is, the headers are ready to be send. */
  void started();
  /** Gets emitted once the response headers has been send. */
  void headersSend();
  /** Gets emitted once the response is completed. */
  void completed();

protected slots:
  /** Gets called on new data. */
  void _onBytesWritten(qint64 bytes);

protected:
  /** The HTTP connection. */
  HttpConnection *_connection;
  HttpVersion _version;
  /** The response code. */
  HttpResponseCode _code;
  /** If @c true, the headers has been send. */
  bool _headersSend;
  /** Index of the header buffer. */
  qint64 _headerSendIdx;
  /** Buffer of the serialized headers. */
  QByteArray _headerBuffer;
  /** Header table. */
  QHash<QString, QString> _headers;
};


/** A specialized response of a simple (short) string.
 * @ingroup http */
class HttpStringResponse: public HttpResponse
{
  Q_OBJECT

public:
  /** Constructs a new HTTP string response.
   * @param resp Specifies the response code.
   * @param text Specifies the response text.
   * @param connection Specifies the connection.
   * @param contentType Specifies the content type of the response. */
  HttpStringResponse(HttpVersion version, HttpResponseCode resp, const QString &text,
                     HttpConnection *connection, const QString contentType="text/text");

protected slots:
  /** Gets called once the headers has been send. */
  void _onHeadersSend();
  /** Gets called once some data has been send. */
  void _onBytesWritten(qint64 bytes);

protected:
  /** Index of the text buffer. */
  quint64    _textIdx;
  /** The text buffer. */
  QByteArray _text;
};


/** Implements a HTTP service for a DHT node.
 * @ingroup http */
class HttpService: public QObject, public AbstractService
{
  Q_OBJECT

public:
  /** Constructs a new HTTP service.
   * @param dht Specifies the DHT node instance.
   * @param handler The request handler.
   * @param parent Optional QObject parent. */
  HttpService(DHT &dht, HttpRequestHandler *handler, QObject *parent=0);
  /** Destructor. */
  virtual ~HttpService();

  /** Constructs a new stream socket. */
  SecureSocket *newSocket();
  /** Retruns @c true by default. */
  bool allowConnection(const NodeItem &peer);
  /** Starts HTTP parser. */
  void connectionStarted(SecureSocket *socket);
  /** Frees failed connections. */
  void connectionFailed(SecureSocket *socket);

protected:
  /** DHT instance. */
  DHT &_dht;
  /** The HTTP request handler. */
  HttpRequestHandler *_handler;
};


#endif // HTTPSERVICE_H
