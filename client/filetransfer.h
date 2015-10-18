#ifndef FILETRANSFER_H
#define FILETRANSFER_H

#include <QObject>
#include "lib/crypto.h"
#include "lib/stream.h"

// Forward declarations
class Application;

#define FILETRANSFER_MAX_DATA_LEN     (DHT_SEC_MAX_DATA_SIZE-5UL)

/** Implements the file transfer sender side. */
class FileUpload : public QObject, public SecureSocket
{
  Q_OBJECT

public:
  typedef enum {
    INITIALIZED, REQUEST_SEND, STARTED, TERMINATED
  } State;

public:
  explicit FileUpload(Application &app, const QString &filename, size_t fileSize, QObject *parent = 0);
  virtual ~FileUpload();

  void handleDatagram(const uint8_t *data, size_t len);

  State state() const;
  size_t free() const;

  size_t write(const QByteArray &data);
  size_t write(const uint8_t *buffer, size_t len);

  const QString &fileName() const;
  size_t fileSize() const;

public slots:
  /** Once the secure connection is established, use this method to initiate the file transfer. */
  bool sendRequest();
  /** Stops the file transfer. */
  void stop();

signals:
  void accepted();
  void bytesWritten(size_t bytes);
  void closed();

protected:
  Application &_application;
  /** Current state of the transmission. */
  State _state;
  /** A transmission packet buffer. */
  StreamOutBuffer _packetBuffer;
  /** The name of the file to transmit. */
  QString _fileName;
  /** The size of the file to transmit. */
  size_t  _fileSize;
};


/** Implements the file transfer receiver side. */
class FileDownload : public QObject, public SecureSocket
{
  Q_OBJECT

public:
  typedef enum {
    INITIALIZED, REQUEST_RECEIVED, ACCEPTED, STARTED, COMPLETE, TERMINATED
  } State;

public:
  explicit FileDownload(Application &app, QObject *parent = 0);
  virtual ~FileDownload();

  void handleDatagram(const uint8_t *data, size_t len);

  State state() const;
  size_t fileSize() const;

  size_t available() const;
  size_t read(QByteArray &buffer);
  size_t read(uint8_t *buffer, size_t len);


public slots:
  void accept();
  void stop();

signals:
  void request(const QString &name, uint64_t size);
  void readyRead();
  void closed();

protected:
  Application &_application;
  State _state;
  size_t _fileSize;
  StreamInBuffer _packetBuffer;
};

#endif // FILETRANSFER_H
