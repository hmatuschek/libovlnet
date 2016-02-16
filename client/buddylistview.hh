#ifndef BUDDYLISTVIEW_H
#define BUDDYLISTVIEW_H

#include <QWidget>
#include <QTreeView>
#include "buddylist.hh"

class Application;

class BuddyListView : public QWidget
{
  Q_OBJECT

public:
  explicit BuddyListView(Application &application, BuddyList *buddies, QWidget *parent=0);

protected slots:
  void onChat();
  void onCall();
  void onSendFile();
  void onStartProxy();
  void onSearch();
  void onDelete();

protected:
  void closeEvent(QCloseEvent *evt);

protected:
  Application &_application;
  BuddyList *_buddies;
  QTreeView *_tree;
};

#endif // BUDDYLISTVIEW_H
