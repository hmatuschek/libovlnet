#include "logwindow.h"
#include <QTableView>
#include <QVBoxLayout>


LogModel::LogModel(QObject *parent)
  : QAbstractTableModel(parent), LogHandler(LogMessage::DEBUG)
{
  // pass...
}


int
LogModel::rowCount(const QModelIndex &parent) const {
  return _messages.size();
}

int
LogModel::columnCount(const QModelIndex &parent) const {
  return 2;
}

QVariant
LogModel::data(const QModelIndex &index, int role) const {
  if (Qt::DisplayRole != role) { return QVariant(); }
  if (! index.isValid()) { return QVariant(); }
  if (index.row() >= _messages.size()) { return QVariant(); }

  if (0 == index.column()) {
    return _messages[index.row()].timestamp().time().toString();
  } else if (1 == index.column()) {
    return _messages[index.row()].message();
  }
  return QVariant();
}


void
LogModel::handleMessage(const LogMessage &msg) {
  this->beginInsertRows(QModelIndex(), _messages.size(), _messages.size());
  _messages.append(msg);
  this->endInsertRows();
}



LogWindow::LogWindow(LogModel *model)
  : QWidget(0)
{
  setWindowTitle(tr("Log Messages"));
  QTableView *table = new QTableView();
  table->setModel(model);
  QVBoxLayout *layout = new QVBoxLayout();
  layout->addWidget(table);
  setLayout(layout);
}



