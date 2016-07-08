#ifndef __OVLNET_HTTP_CLIENT_HH__
#define __OVLNET_HTTP_CLIENT_HH__

#include <QObject>
#include <QJsonDocument>
#include "node.hh"
#include "stream.hh"
#include "http.hh"


class HttpClientConnection;
class HttpClientResponse;


class HttpClientConnection: public SecureStream
{
  Q_OBJECT

public:
  HttpClientConnection(Network &net, const NodeItem &remote, const QString &service, QObject *parent=0);

  HttpClientResponse *get(const QString &path);
  const NodeItem &remote() const;

protected:
  bool start(const Identifier &streamId, const PeerItem &peer);
  void failed();

protected slots:
  void _onRequestFinished(QObject *req);

protected:
  typedef enum {
    CONNECTING, IDLE, PROCESS_REQUEST, ERROR
  } State;

protected:
  State _state;
  QString _service;
  NodeItem _remote;
  friend class HttpClientSearchQuery;
};


class HttpClientResponse: public QIODevice
{
  Q_OBJECT

public:
  HttpClientResponse(HttpClientConnection *connection, HttpMethod method, const QString &path);

  bool isSequential() const;
  void close();
  qint64 bytesAvailable() const;
  qint64 bytesToWrite() const;

  HttpResponseCode responseCode() const;
  bool hasResponseHeader(const QByteArray &header) const;
  QByteArray responseHeader(const QByteArray &header) const;

signals:
  void finished();
  void error();

protected:
  qint64 readData(char *data, qint64 maxlen);
  qint64 writeData(const char *data, qint64 len);

private slots:
  void _onDataAvailable();

protected:
  typedef enum {
    SEND_HEADER, SEND_BODY, RECV_RESPONSE_CODE,
    RECV_HEADER, RECV_BODY, FINISHED, ERROR
  } State;

protected:
  HttpClientConnection *_connection;
  State _state;

  HttpMethod _method;
  QString _path;

  HttpResponseCode _resCode;
  QHash<QByteArray, QByteArray> _resHeaders;
};


/** Self-destructing HTTP Json query. This class forms the base class for all
 * info queries send to a station. */
class JsonQuery: public QObject
{
  Q_OBJECT

public:
  /** Constructs a query object for the given path send to the remote specified by its identifier.
   * With this contructor, first the identifier gets resolved gefore the query is send. */
  JsonQuery(const QString &service, const QString &path, Network &net, const Identifier &remote);
  /** Constructs a query object for the given path send to the remote specified by the node item. */
  JsonQuery(const QString &service, const QString &path, Network &net, const NodeItem &remote);
  /** Constructs a PUSH query object for the given path and data send to the remote specified by the node item. */
  JsonQuery(const QString &service, const QString &path, const QJsonDocument &data, Network &net, const Identifier &remote);
  /** Constructs a PUSH query object for the given path and data send to the remote specified by the node item. */
  JsonQuery(const QString &service, const QString &path, const QJsonDocument &data, Network &net, const NodeItem &remote);

signals:
  /** Gets emitted if the query fails. */
  void failed();
  /** Gets emmited if the query succeeds with the given document. */
  void success(const NodeItem &remote, const QJsonDocument &doc);

protected slots:
  /** Gets called if the node lookup succeeds. */
  void nodeFound(const NodeItem &node);
  /** Gets called once the connection is established. */
  void connectionEstablished();
  /** Gets called once the response header was received. */
  void responseReceived();
  /** Gets called to accept or reject the response. */
  bool accept();
  /** Gets called on error.*/
  void error();
  /** Gets called if the query succeeds with the resulting Json document. */
  void finished(const QJsonDocument &doc);
  /** Gets called when data can be received. */
  void _onReadyRead();

protected:
  /** The service id. */
  QString _service;
  /** The query path. */
  QString _query;
  /** This network. */
  Network &_network;
  /** The remote identifier. */
  Identifier _remoteId;
  /** The HTTP method to use. */
  HttpMethod _method;
  /** The data to send if @c _method is push. */
  QJsonDocument _data;
  /** The connection to the remote station. */
  HttpClientConnection *_connection;
  /** The HTTP response instance. */
  HttpClientResponse *_response;
  /** The HTTP response body length. */
  size_t _responseLength;
  /** The response body buffer. */
  QByteArray _buffer;
};




#endif // __OVLNET_HTTP_CLIENT_HH__
