#include "filetransferdialog.hh"
#include "application.hh"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QIcon>
#include <QFileDialog>
#include <QCloseEvent>
#include <QPixmap>
#include <QImage>


/* ********************************************************************************************* *
 * Implementation of FileUploadDialog
 * ********************************************************************************************* */
FileUploadDialog::FileUploadDialog(FileUpload *upload, Application &app, QWidget *parent)
  : QWidget(parent), _application(app), _upload(upload), _file(upload->fileName()),
    _bytesSend(0)
{
  setWindowTitle(tr("File upload"));

  QFileInfo fileinfo(_file.fileName());
  _info = new QLabel(tr("Wait for transfer of file %1...").arg(fileinfo.baseName()));

  _progress = new QProgressBar();
  _progress->setMaximum(100);
  _progress->setValue(0);

  _button = new QPushButton(QIcon("://icons/circle-x.png"), tr("stop"));
  QLabel *icon = new QLabel(); icon->setPixmap(
        QPixmap::fromImage(QImage("://icons/data-transfer-upload.png")));

  QHBoxLayout *layout = new QHBoxLayout();
  layout->addWidget(icon);
  QVBoxLayout *box = new QVBoxLayout();
  box->addWidget(_info);
  box->addWidget(_progress);
  layout->addLayout(box);
  layout->addWidget(_button);
  setLayout(layout);

  QObject::connect(_button, SIGNAL(clicked()), this, SLOT(_onClick()));
  QObject::connect(_upload, SIGNAL(accepted()), this, SLOT(_onAccepted()));
  QObject::connect(_upload, SIGNAL(closed()), this, SLOT(_onClosed()));
  QObject::connect(_upload, SIGNAL(bytesWritten(size_t)), this, SLOT(_onBytesWritten(size_t)));
}

FileUploadDialog::~FileUploadDialog() {
  delete _upload;
}

void
FileUploadDialog::_onClick() {
  // If transfer is terminated or done
  if ( (FileUpload::TERMINATED == _upload->state()) ||
       (_bytesSend == _upload->fileSize()) ) {
    this->close();
  } else {
    _upload->stop();
  }
}

void
FileUploadDialog::_onAccepted() {
  QFileInfo fileinfo(_file.fileName());
  _info->setText(tr("Transfer file \"%1\" ...").arg(fileinfo.fileName()));
  logDebug() << "Start transfer of file" << _file.fileName();
  _file.open(QIODevice::ReadOnly);
  size_t offset = 0;
  uint8_t buffer[FILETRANSFER_MAX_DATA_LEN];
  while (_upload->free() && (!_file.atEnd())) {
    int len = _file.read((char *) buffer, FILETRANSFER_MAX_DATA_LEN);
    len = _upload->write(buffer, len);
    offset += len; _file.seek(offset);
  }
}

void
FileUploadDialog::_onClosed() {
  logDebug() << "Stop transfer of file" << _file.fileName();
  _file.close();

  QFileInfo fileinfo(_file.fileName());
  if (_bytesSend == _upload->fileSize()) {
    _info->setText(tr("Transfer of file \"%1\" completed.").arg(fileinfo.fileName()));
    _button->setIcon(QIcon("://icons/circle-check.png"));
  } else {
    _info->setText(tr("Transfer of file \"%1\" aborted.").arg(fileinfo.fileName()));
    _button->setIcon(QIcon("://icons/circle-x.png"));
  }
  _button->setText(tr("close"));
  _progress->setValue(100);
}

void
FileUploadDialog::_onBytesWritten(size_t bytes) {
  // Update bytecount & progress bar
  _bytesSend += bytes;
  _progress->setValue(100*double(_bytesSend)/_upload->fileSize());

  // if complete -> close stream etc.
  if (_upload->fileSize() == _bytesSend) {
    logDebug() << "Transmission complete.";
    _upload->stop();
    _file.close();
    return;
  }

  logDebug() << "Continue transfer of file" << _file.fileName() << "at byte" << _bytesSend;

  // If not complete -> continue
  size_t offset = _file.pos();
  uint8_t buffer[FILETRANSFER_MAX_DATA_LEN];
  while (_upload->free() && (!_file.atEnd())) {
    int len = _file.read((char *) buffer, FILETRANSFER_MAX_DATA_LEN);
    len = _upload->write(buffer, len);
    offset += len; _file.seek(offset);
  }
}

void
FileUploadDialog::closeEvent(QCloseEvent *evt) {
  evt->accept(); this->deleteLater();
}


/* ********************************************************************************************* *
 * Implementation of FileDownloadDialog
 * ********************************************************************************************* */
FileDownloadDialog::FileDownloadDialog(FileDownload *download, Application &app, QWidget *parent)
  : QWidget(parent), _application(app), _download(download)
{
  setWindowTitle(tr("File download"));

  _info = new QLabel(tr("Incomming file transfer..."));

  _acceptStop = new QPushButton(QIcon("://icons/circle-check.png"), tr("accept"));
  _acceptStop->setEnabled(false);

  _progress = new QProgressBar();
  _progress->setMaximum(100);
  _progress->setValue(0);

  QLabel *icon = new QLabel(); icon->setPixmap(
        QPixmap::fromImage(QImage("://icons/data-transfer-download.png")));

  QHBoxLayout *layout = new QHBoxLayout();
  layout->addWidget(icon);
  QVBoxLayout *box = new QVBoxLayout();
  box->addWidget(_info);
  box->addWidget(_progress);
  layout->addLayout(box);
  layout->addWidget(_acceptStop);
  setLayout(layout);

  connect(_acceptStop, SIGNAL(clicked()), this, SLOT(_onAcceptStop()));
  connect(_download, SIGNAL(request(QString,uint64_t)),
          this, SLOT(_onRequest(QString,uint64_t)));
  connect(_download, SIGNAL(readyRead()), this, SLOT(_onReadyRead()));
  connect(_download, SIGNAL(closed()), this, SLOT(_onClosed()));
}

FileDownloadDialog::~FileDownloadDialog() {
  delete _download;
}

void
FileDownloadDialog::_onAcceptStop() {
  if (FileDownload::STARTED == _download->state()) {
    _file.close(); _download->stop(); this->close();
  } else if (FileDownload::REQUEST_RECEIVED == _download->state()) {
    QString fname = QFileDialog::getSaveFileName(0, tr("Save file as"));
    if (0 == fname) { _download->stop(); return; }
    _file.setFileName(fname);
    _file.open(QIODevice::WriteOnly);
    QFileInfo fileinfo(fname);
    _info->setText(tr("Downloading file \"%1\" ...").arg(fileinfo.fileName()));
    _acceptStop->setIcon(QIcon("://icons/circle-x.png"));
    _acceptStop->setText(tr("stop"));
    _download->accept();
  } else if ((FileDownload::TERMINATED == _download->state()) ||
             (_bytesReceived == _download->fileSize())) {
    // if transfer is terminated (or done) close window
    this->close();
  }
}

void
FileDownloadDialog::_onRequest(const QString &filename, uint64_t size) {
  _info->setText(tr("Accept file %1 (%2)?").arg(filename).arg(size));
  _acceptStop->setEnabled(true);
}

void
FileDownloadDialog::_onReadyRead() {
  uint8_t buffer[FILETRANSFER_MAX_DATA_LEN];
  logDebug() << "In _onReadyRead:";
  while (_download->available()) {
    size_t len = _download->read(buffer, FILETRANSFER_MAX_DATA_LEN);
    logDebug() << " received" << len << "bytes.";
    /// @bug Check if data was written.
    _file.write((const char *) buffer, len);
    _bytesReceived += len;
  }
  // Update progress
  _progress->setValue(100*double(_bytesReceived)/_download->fileSize());
  // Check download complete
  if (_bytesReceived == _download->fileSize()) {
    _file.close();
    _info->setText(tr("Download complete."));
    _acceptStop->setIcon(QIcon(":/icons/circle-check.png"));
    _acceptStop->setText(tr("close"));
  }
}

void
FileDownloadDialog::_onClosed() {
  if (_bytesReceived != _download->fileSize()) {
    _info->setText(tr("Download terminated..."));
    _acceptStop->setIcon(QIcon(":/icons/circle-x.png"));
    _acceptStop->setText(tr("close"));
  }
  _file.close();
}

void
FileDownloadDialog::closeEvent(QCloseEvent *evt) {
  evt->accept();
  this->deleteLater();
}
