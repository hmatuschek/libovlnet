#ifndef __OVLNET_HTTP_CLIENT_HH__
#define __OVLNET_HTTP_CLIENT_HH__

#include <QObject>
#include "node.hh"
#include "stream.hh"
#include "httpservice.hh"


class HttpClientConnection;
class HttpClientResponse;


class HttpClientConnection: public SecureStream
{
  Q_OBJECT

public:
  HttpClientConnection(Node &node, const NodeItem &remote, const QString &service, QObject *parent=0);

  HttpClientResponse *get(const QString &path);

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


#endif // __OVLNET_HTTP_CLIENT_HH__
