/** @defgroup http Http service
 * @ingroup services */

#ifndef HTTPSERVICE_H
#define HTTPSERVICE_H

#include <QString>
#include <QIODevice>
#include <QHash>
#include <QTcpServer>
#include <QDir>

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


/** Represents a hostname with optional port.
 * @ingroup http */
class HostName
{
public:
  /** Constructs a hostname and port tuple from the given string.
   * The string can be of the form "HOSTNAME[:PORT]" where @c defaultPort is used
   * if "PORT" is missing in the string. */
  HostName(const QString &name, uint16_t defaultPort=80);
  /** Copy constructor. */
  HostName(const HostName &other);
  /** Assignment operator. */
  HostName &operator=(const HostName &other);

  /** Returns the host name. */
  const QString &name() const;
  /** Returns the port. */
  uint16_t port() const;
  /** Returns @c true if the hostname is of the form "ID.ovl". */
  bool isOvlNode() const;
  /** Returns node ID if the hostname is of the form "ID.ovl". */
  Identifier ovlId() const;

protected:
  /** The host name. */
  QString _name;
  /** The port. */
  uint16_t _port;
};


/** Represents an URI. */
class URI
{
public:
  /** Empty constructor. */
  URI();
  /** Constructor from string representation. */
  URI(const QString &uri);
  /** Copy constructor. */
  URI(const URI &other);
  /** Assignment operator. */
  URI &operator= (const URI &other);

  /** Returns the protocol name. */
  inline const QString &protocol() const { return _proto; }
  /** Returns the host name and port. */
  inline const HostName &host() const { return _host; }
  /** Returns the path. */
  inline const QString &path() const { return _path; }
  /** Returns the query string. */
  inline const QString &query() const { return _query; }

protected:
  /** The protocol. */
  QString _proto;
  /** The host name. */
  HostName _host;
  /** The path. */
  QString _path;
  /** The query string. */
  QString _query;
};


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

  /** Returns @c true if the server bound to the specified port. */
  bool started() const;

protected slots:
  /** Gets called on incomming connections. */
  void _onNewConnection();

protected:
  /** The request handler instance. */
  HttpRequestHandler *_dispatcher;
  /** The local TCP server. */
  QTcpServer _server;
};


/** Represents an incomming connection from a HTTP client.
 * @ingroup http */
class HttpConnection: public QObject
{
  Q_OBJECT

public:
  /** Constructs a new HTTP connection.
   * @param service Specifies the request handler instance for the connection.
   * @param remote Specifies the remote node (at least address and port).
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
  /** The iterator over the request headers. */
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
  inline const URI &uri() const { return _uri; }
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
  /** Returns the iterator pointing at the first header. */
  inline iterator begin() const { return _headers.begin(); }
  /** Returns the iterator pointing right after the last header. */
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
  URI _uri;
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
   * @param version Specifies the HTTP version of the response.
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

  /** Start the response if ready. */
  void sendHeaders();

signals:
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
  /** The HTTP version. */
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
   * @param version Specifies the HTTP version of the response.
   * @param resp Specifies the response code.
   * @param text Specifies the response text.
   * @param connection Specifies the connection.
   * @param contentType Specifies the content type of the response. */
  HttpStringResponse(HttpVersion version, HttpResponseCode resp, const QString &text,
                     HttpConnection *connection, const QString contentType="text/plain");

protected slots:
  /** Gets called once the headers has been send. */
  void _onHeadersSend();
  /** Gets called once some data has been send. */
  void _bytesWritten(qint64 bytes);

protected:
  /** Index of the text buffer. */
  quint64    _textIdx;
  /** The text buffer. */
  QByteArray _text;
};


/** A simple class sending a JSON response.
 * @ingroup http */
class HttpJsonResponse: public HttpStringResponse
{
  Q_OBJECT

public:
  /** Constructor.
   * @param document Specifies the JSON document to send.
   * @param request Specifies the request, this is a response to. */
  HttpJsonResponse(const QJsonDocument &document, HttpRequest *request);
};


/** Implements a response class transmitting a file.
 * @ingroup http */
class HttpFileResponse: public HttpResponse
{
  Q_OBJECT

public:
  /** Constructor.
   * @param filename Specifies the file to transmit.
   * @param request The request, this is a response to. */
  HttpFileResponse(const QString &filename, HttpRequest *request);

public:
  /** Guess a MIME type for the given file extension. */
  static QString guessMimeType(const QString &ext);

protected slots:
  /** Gets called once the headers has been send. */
  void _onHeadersSend();
  /** Gets called once some data has been send. */
  void _bytesWritten(qint64 bytes);

protected:
  /** The file to transmit. */
  QFile   _file;
  /** The current offset within the file. */
  quint64 _offset;
};


/** Returns a directory listing in HTML as a response.
 * @ingroup http */
class HttpDirectoryResponse: public HttpResponse
{
  Q_OBJECT

public:
  /** Constructor.
   * @param dirname Specifies the directory to list.
   * @param request The request this is a response to. */
  HttpDirectoryResponse(const QString &dirname, HttpRequest *request);

protected slots:
  /** Gets called once the headers has been send. */
  void _onHeadersSend();
  /** Gets called once some data has been send. */
  void _bytesWritten(qint64 bytes);

protected:
  /** Buffer containing the HTML to send. */
  QByteArray _buffer;
  /** The current offset in the buffer. */
  quint64 _offset;
};


/** Serves a directory.
 * @ingroup http */
class HttpDirectoryHandler: public HttpRequestHandler
{
  Q_OBJECT

public:
  /** Constructor.
   * @param directory Specifies the directory to serve.
   * @param parent Specifies the optional QObject parent. */
  HttpDirectoryHandler(const QDir &directory, QObject *parent=0);

  /** Accepts all GET requests. */
  bool acceptReqest(HttpRequest *request);
  HttpResponse *processRequest(HttpRequest *request);

protected:
  /** The directory to serve. */
  QDir _directory;
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
  HttpService(Node &dht, HttpRequestHandler *handler, QObject *parent=0);
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
  Node &_dht;
  /** The HTTP request handler. */
  HttpRequestHandler *_handler;
};


#endif // HTTPSERVICE_H
