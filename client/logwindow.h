#ifndef LOGWINDOW_H
#define LOGWINDOW_H

#include <QWidget>
#include <QAbstractTableModel>
#include "lib/logger.h"


class LogModel: public QAbstractTableModel, public LogHandler
{
  Q_OBJECT

public:
  explicit LogModel(QObject *parent = 0);

  void handleMessage(const LogMessage &msg);

  int rowCount(const QModelIndex &parent) const;
  int columnCount(const QModelIndex &parent) const;
  QVariant data(const QModelIndex &index, int role) const;

protected:
  QVector<LogMessage> _messages;
};


class LogWindow: public QWidget
{
  Q_OBJECT

public:
  explicit LogWindow(LogModel *model);
};

#endif // LOGWINDOW_H
