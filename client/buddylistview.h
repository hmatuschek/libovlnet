#ifndef BUDDYLISTVIEW_H
#define BUDDYLISTVIEW_H

#include <QWidget>
#include <QTreeWidget>
#include "buddylist.h"

class Application;

class BuddyListView : public QWidget
{
  Q_OBJECT

public:
  explicit BuddyListView(Application &application, BuddyList *buddies, QWidget *parent=0);

protected slots:
  void buddyAdded(const QString &buddy);
  void buddyDeleted(const QString &buddy);
  void nodeAdded(const QString &buddy, const Identifier &id);
  void nodeRemoved(const QString &buddy, const Identifier &id);

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
  QTreeWidget *_tree;
};

#endif // BUDDYLISTVIEW_H
