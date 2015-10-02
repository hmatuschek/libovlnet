#ifndef FILETRANSFERDIALOG_H
#define FILETRANSFERDIALOG_H

#include "fileupload.h"

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>


class FileUploadDialog: public QWidget
{
  Q_OBJECT

public:
  FileUploadDialog(FileUpload *upload, Application &app, QWidget *parent=0);
  virtual ~FileUploadDialog();

protected slots:
  void _onAbort();
  void _onAccepted();
  void _onClosed();
  void _onBytesWritten(size_t bytes);

protected:
  Application  &_application;
  FileUpload   *_upload;
  QFile        _file;
  size_t       _bytesSend;

  QLabel       *_info;
  QProgressBar *_progress;
};


class FileDownloadDialog: public QWidget
{
  Q_OBJECT

public:
   FileDownloadDialog(FileDownload *download, Application &app, QWidget *parent=0);
   virtual ~FileDownloadDialog();

protected slots:
   void _onAcceptStop();
   void _onRequest(const QString &filename, size_t size);
   void _onReadyRead();
   void _onClosed();

protected:
   Application  &_application;
   FileDownload *_download;
   QFile        _file;

   QLabel       *_info;
   QPushButton  *_acceptStop;
   QProgressBar *_progress;
};

#endif // FILETRANSFERDIALOG_H