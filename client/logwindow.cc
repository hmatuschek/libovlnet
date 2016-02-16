#include "logwindow.hh"
#include <QTableView>
#include <QVBoxLayout>
#include <QBrush>
#include <QFont>
#include <QHeaderView>
#include <QListView>
#include <QCloseEvent>
#include <QFileInfo>

#include "application.hh"


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
  return 2;
}

QVariant
LogModel::data(const QModelIndex &index, int role) const {
  if (! index.isValid()) { return QVariant(); }
  if (index.row() >= _messages.size()) { return QVariant(); }
  if (Qt::DisplayRole == role) {
    if (0 == index.column()) {
      QFileInfo info(_messages[index.row()].filename());
      return QString("%1:%2").arg(info.fileName()).arg(_messages[index.row()].linenumber());
    } else if (1 == index.column()) {
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
  if (Qt::Horizontal == orientation) {
    if (Qt::DisplayRole != role) { return QVariant(); }
    if (0 == section) { return "File"; }
    if (1 == section) { return "Message"; }
    return QVariant();
  }
  if (Qt::Vertical == orientation){
    if (Qt::DisplayRole != role) { return QVariant(); }
    if (section >= _messages.size()) { return QVariant(); }
    return _messages[section].timestamp().time().toString();
  }
  return QVariant();
}

void
LogModel::handleMessage(const LogMessage &msg) {
  this->beginInsertRows(QModelIndex(), _messages.size(), _messages.size());
  _messages.append(msg);
  this->endInsertRows();
}



LogWidget::LogWidget(Application &app)
  : QWidget(0)
{
  setMinimumSize(640, 360);

  _table = new QTableView();
  _table->setModel(&app.log());
  _table->horizontalHeader()->setStretchLastSection(true);

  connect(&app.log(), SIGNAL(rowsInserted(QModelIndex,int,int)),
          _table, SLOT(scrollToBottom()));
  QVBoxLayout *layout = new QVBoxLayout();
  layout->addWidget(_table);
  layout->setContentsMargins(0,0,0,0);
  setLayout(layout);

  _table->scrollToBottom();
}
