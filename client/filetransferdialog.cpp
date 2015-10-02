#include "filetransferdialog.h"
#include "application.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QIcon>
#include <QFileDialog>

/* ********************************************************************************************* *
 * Implementation of FileUploadDialog
 * ********************************************************************************************* */
FileUploadDialog::FileUploadDialog(FileUpload *upload, Application &app, QWidget *parent)
  : QWidget(parent), _application(app), _upload(upload), _file(upload->fileName())
{
  QFileInfo fileinfo(_file.fileName());
  _info = new QLabel(tr("Wait for transfer of file %1...").arg(fileinfo.baseName()));

  _progress = new QProgressBar();
  _progress->setMaximum(100);
  _progress->setValue(0);

  QPushButton *stop = new QPushButton(QIcon("://icons/circle-x.png"), tr("abort"));

  QHBoxLayout *layout = new QHBoxLayout();
  //layout->addWidget(QIcon("://icons/data-transfer-upload.png"));
  QVBoxLayout *box = new QVBoxLayout();
  box->addWidget(_info);
  box->addWidget(_progress);
  layout->addLayout(box);
  layout->addWidget(stop);
  setLayout(layout);

  QObject::connect(stop, SIGNAL(clicked()), this, SLOT(_onAbort()));
  QObject::connect(_upload, SIGNAL(accepted()), this, SLOT(_onAccepted()));
  QObject::connect(_upload, SIGNAL(closed()), this, SLOT(_onClosed()));
  QObject::connect(_upload, SIGNAL(bytesWritten(uint64_t)), this, SLOT(_onBytesWritten(uint64_t)));
}

FileUploadDialog::~FileUploadDialog() {
  delete _upload;
}

void
FileUploadDialog::_onAbort() {
  _upload->stop();
  _file.close();
}

void
FileUploadDialog::_onAccepted() {
  QFileInfo fileinfo(_file.fileName());
  _info->setText(tr("Transfer file %1...").arg(fileinfo.baseName()));

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
  _file.close();
  QFileInfo fileinfo(_file.fileName());
  _info->setText(tr("File transfer aborted %1...").arg(fileinfo.baseName()));
  _progress->setValue(100);
}

void
FileUploadDialog::_onBytesWritten(size_t bytes) {
  _bytesSend += bytes;
  // Update progress bar
  _progress->setValue(100*double(_bytesSend)/_upload->fileSize());
  // if complete -> close stream etc.
  if (_upload->fileSize() == bytes) {
    QFileInfo fileinfo(_file.fileName());
    _info->setText(tr("File transfer completed.").arg(fileinfo.baseName()));
    _upload->stop(); _file.close();
    return;
  }
  // If not complete -> continue
  size_t offset = _file.pos();
  uint8_t buffer[FILETRANSFER_MAX_DATA_LEN];
  while (_upload->free() && (!_file.atEnd())) {
    int len = _file.read((char *) buffer, FILETRANSFER_MAX_DATA_LEN);
    len = _upload->write(buffer, len);
    offset += len; _file.seek(offset);
  }
}



/* ********************************************************************************************* *
 * Implementation of FileDownloadDialog
 * ********************************************************************************************* */
FileDownloadDialog::FileDownloadDialog(FileDownload *download, Application &app, QWidget *parent)
  : QWidget(parent), _application(app), _download(download)
{
  _info = new QLabel(tr("Incomming file transfer..."));

  _acceptStop = new QPushButton(QIcon("://icons/circle-check.png"), tr("accept"));
  _acceptStop->setEnabled(false);

  _progress = new QProgressBar();
  _progress->setMaximum(100);
  _progress->setValue(0);

  QHBoxLayout *layout = new QHBoxLayout();
  //layout->addWidget(QIcon(":/icons/data-transfer-download.png"));
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
    _acceptStop->setIcon(QIcon("://icons/circle-x.png"));
    _acceptStop->setText(tr("stop"));
    _download->accept();
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
  size_t len = _download->read(buffer, FILETRANSFER_MAX_DATA_LEN);
  _file.write((const char *) buffer, len);
}

void
FileDownloadDialog::_onClosed() {
  _file.close();
}
