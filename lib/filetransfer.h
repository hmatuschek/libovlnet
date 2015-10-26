#ifndef FILETRANSFER_H
#define FILETRANSFER_H

#include <QObject>
#include "lib/crypto.h"
#include "lib/stream.h"

/** Maximum amount of data transferred in a single message. */
#define FILETRANSFER_MAX_DATA_LEN     (DHT_SEC_MAX_DATA_SIZE-5UL)


/** Implements the file transfer sender side.
 * @ingroup services */
class FileUpload : public QObject, public SecureSocket
{
  Q_OBJECT

public:
  /** State of the transfer. */
  typedef enum {
    INITIALIZED,   ///< Initialized.
    REQUEST_SEND,  ///< File transfer request send.
    STARTED,       ///< File transfer request accepted.
    TERMINATED     ///< Transmission terminated.
  } State;

public:
  /** Constructor.
   * @param dht Specifies the DHT instance.
   * @param filename Filename of the file to be transferred (do not need to exists).
   * @param filesize Size of the file.
   * @param parent Specifies the optional QObject parent. */
  explicit FileUpload(DHT &dht, const QString &filename, size_t filesize, QObject *parent = 0);
  /** Destructor. */
  virtual ~FileUpload();

  /** Implements the @c SecureSocket interface. */
  void handleDatagram(const uint8_t *data, size_t len);

  /** Returns the state of the transfer. */
  State state() const;
  /** Returns the amount of free space in the transfer buffer. */
  size_t free() const;
  /** Sends some data. */
  size_t write(const QByteArray &data);
  /** Sends some data. */
  size_t write(const uint8_t *buffer, size_t len);

  /** Returns the filename of the file being transferred. */
  const QString &fileName() const;
  /** Returns the size of the file being transferred. */
  size_t fileSize() const;

public slots:
  /** Once the secure connection is established, use this method to initiate the file transfer. */
  bool sendRequest();
  /** Stops the file transfer. */
  void stop();

signals:
  /** Gets emitted once the file transfer is accepted by the remote. */
  void accepted();
  /** Gets emitted when some data has been transferred. */
  void bytesWritten(size_t bytes);
  /** Gets emitted once the connection is closed. */
  void closed();

protected:
  /** Current state of the transmission. */
  State _state;
  /** A transmission packet buffer. */
  StreamOutBuffer _packetBuffer;
  /** The name of the file to transmit. */
  QString _fileName;
  /** The size of the file to transmit. */
  size_t  _fileSize;
};


/** Implements the file transfer receiver side.
 * @ingroup services */
class FileDownload : public QObject, public SecureSocket
{
  Q_OBJECT

public:
  /** Possible states of the file transfer. */
  typedef enum {
    INITIALIZED,      ///< Initialized.
    REQUEST_RECEIVED, ///< Request received.
    ACCEPTED,         ///< Request accepted.
    STARTED,          ///< File transfer started.
    COMPLETE,         ///< File transfer completed.
    TERMINATED        ///< File transfer terminated.
  } State;

public:
  /** Constructor.
   * @param dht Specifies the DHT instance.
   * @param parent Specifies the optional QObject parent. */
  explicit FileDownload(DHT &dht, QObject *parent = 0);
  /** Destructor. */
  virtual ~FileDownload();

  /** Implements the @c SecureSocket interface. */
  void handleDatagram(const uint8_t *data, size_t len);

  /** Returns the current state of the file transfer. */
  State state() const;
  /** Returns the size of the file being transferred. */
  size_t fileSize() const;
  /** Returns the number of bytes available for reading. */
  size_t available() const;
  /** Receives some data from the rx buffer. */
  size_t read(QByteArray &buffer);
  /** Receives some data from the rx buffer. */
  size_t read(uint8_t *buffer, size_t len);

public slots:
  /** Accepts an incomming file. */
  void accept();
  /** Stops the file transfer. */
  void stop();

signals:
  /** Gets emitted in a request arrived. */
  void request(const QString &name, uint64_t size);
  /** Gets emitted if new data arrives. */
  void readyRead();
  /** Gets emitted if the connection is closed. */
  void closed();

protected:
  /** State of the filetransfer. */
  State _state;
  /** Size of the file. */
  size_t _fileSize;
  /** The internal receive buffer. */
  StreamInBuffer _packetBuffer;
};

#endif // FILETRANSFER_H
