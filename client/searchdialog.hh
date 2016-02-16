#ifndef SEARCHDIALOG_H
#define SEARCHDIALOG_H

#include "lib/dht.h"
#include "buddylist.hh"

#include <QWidget>
#include <QLineEdit>
#include <QTableWidget>

class SearchDialog : public QWidget
{
  Q_OBJECT

public:
  explicit SearchDialog(DHT *dht, BuddyList *buddies, QWidget *parent = 0);

protected slots:
  void _onStartSearch();
  void _onSearchSuccess(const NodeItem &node);
  void _onSearchFailed(const Identifier &id, const QList<NodeItem> &best);
  void _onAddAsNewBuddy();
  void _onAddToBuddy();

protected:
  void closeEvent(QCloseEvent *evt);

protected:
  DHT *_dht;
  BuddyList *_buddies;
  Identifier _currentSearch;
  QLineEdit *_query;
  QTableWidget *_result;
};

#endif // SEARCHDIALOG_H
