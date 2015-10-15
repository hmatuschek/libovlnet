#include "logwindow.h"
#include <QTableView>
#include <QVBoxLayout>
#include <QBrush>
#include <QFont>
#include <QHeaderView>
#include <QListView>
#include <QCloseEvent>


LogModel::LogModel(QObject *parent)
  : QAbstractTableModel(parent), LogHandler(LogMessage::DEBUG)
{
  // pass...
}

LogModel::~LogModel() {
  logDebug() << "LogModel: Destroyed.";
}


int
LogModel::rowCount(const QModelIndex &parent) const {
  return _messages.size();
}

int
LogModel::columnCount(const QModelIndex &parent) const {
  return 1;
}

QVariant
LogModel::data(const QModelIndex &index, int role) const {
  if (! index.isValid()) { return QVariant(); }
  if (index.row() >= _messages.size()) { return QVariant(); }
  if (Qt::DisplayRole == role) {
    if (0 == index.column()) {
      return _messages[index.row()].message();
    }
  } else if (Qt::ForegroundRole == role) {
    switch (_messages[index.row()].level()) {
    case LogMessage::DEBUG: return QBrush(Qt::gray);
    case LogMessage::INFO: return QBrush(Qt::black);
    case LogMessage::WARNING: return QBrush(Qt::black);
    case LogMessage::ERROR: return QBrush(Qt::red);
    }
  } else if (Qt::FontRole == role) {
    QFont font;
    switch (_messages[index.row()].level()) {
    case LogMessage::WARNING:
    case LogMessage::ERROR: font.setBold(true);
    default: break;
    }
    return font;
  }

  return QVariant();
}


QVariant
LogModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (Qt::Vertical != orientation) { return QVariant(); }
  if (Qt::DisplayRole != role) { return QVariant(); }
  if (section >= _messages.size()) { return QVariant(); }
  return _messages[section].timestamp().time().toString();
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

  setMinimumSize(640, 360);

  _table = new QTableView();
  _table->setModel(model);
  _table->horizontalHeader()->setVisible(false);
  _table->horizontalHeader()->setStretchLastSection(true);

  connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)),
          _table, SLOT(scrollToBottom()));
  QVBoxLayout *layout = new QVBoxLayout();
  layout->addWidget(_table);
  layout->setContentsMargins(0,0,0,0);
  setLayout(layout);
}

void
LogWindow::closeEvent(QCloseEvent *evt)  {
  QWidget::closeEvent(evt);
  evt->accept();
  this->deleteLater();
}
