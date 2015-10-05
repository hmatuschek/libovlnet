#include "buddylistview.h"
#include "application.h"
#include "searchdialog.h"

#include <QVBoxLayout>
#include <QToolBar>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>


BuddyListView::BuddyListView(Application &application, BuddyList *buddies, QWidget *parent)
  : QWidget(parent), _application(application), _buddies(buddies)
{
  setWindowTitle(tr("VLF Contacts"));
  setMinimumWidth(300);
  setMinimumHeight(500);

  _tree = new QTreeWidget();
  _tree->setHeaderHidden(true);

  QToolBar *box = new QToolBar();
  box->addAction(QIcon("://icons/chat.png"), tr("Chat"), this, SLOT(onChat()));
  box->addAction(QIcon("://icons/phone.png"), tr("Call"), this, SLOT(onCall()));
  box->addAction(QIcon("://icons/data-transfer-upload.png"), tr("Send file..."),
                 this, SLOT(onSendFile()));
  box->addAction(QIcon("://icons/globe.png"), tr("Start tunnel"), this, SLOT(onStartProxy()));
  box->addSeparator();
  box->addAction(QIcon("://icons/search.png"), tr("Search"), this, SLOT(onSearch()));
  box->addAction(QIcon("://icons/circle-x.png"), tr("Delete"), this, SLOT(onDelete()));

  QHash<QString, Buddy *>::const_iterator buddy = _buddies->buddies().begin();
  for (; buddy!=_buddies->buddies().end(); buddy++) {
    QTreeWidgetItem *item = new QTreeWidgetItem(_tree);
    item->setText(0, buddy.key());
    item->setIcon(0, QIcon("://icons/person.png"));
    QHash<Identifier, Buddy::NodeItem>::const_iterator node = (*buddy)->nodes().begin();
    for (; node != (*buddy)->nodes().end(); node++) {
      QTreeWidgetItem *nodeitem = new QTreeWidgetItem(item);
      nodeitem->setText(0, QString(node.key().toHex()));
      nodeitem->setIcon(0, QIcon("://icons/fork.png"));
    }
  }

  QVBoxLayout *layout = new QVBoxLayout();
  layout->setSpacing(0);
  layout->setContentsMargins(0,0,0,0);
  layout->addWidget(box);
  layout->addWidget(_tree);
  setLayout(layout);
}

void
BuddyListView::buddyAdded(const QString &name) {
  Buddy *buddy = _buddies->getBuddy(name);
  QTreeWidgetItem *item = new QTreeWidgetItem(_tree);
  item->setText(0, name);
  item->setIcon(0, QIcon("://icons/person.png"));
  QHash<Identifier, Buddy::NodeItem>::const_iterator node = buddy->nodes().begin();
  for (; node != buddy->nodes().end(); node++) {
    QTreeWidgetItem *nodeitem = new QTreeWidgetItem(item);
    nodeitem->setText(0, QString(node.key().toHex()));
    nodeitem->setIcon(0, QIcon("://icons/fork.png"));
  }
}

void
BuddyListView::buddyDeleted(const QString &buddy) {
  QList<QTreeWidgetItem *> items = _tree->findItems(buddy, Qt::MatchExactly);
  QList<QTreeWidgetItem *>::iterator item = items.begin();
  for (; item != items.end(); item++) {
    _tree->removeItemWidget(*item, 0);
  }
}

void
BuddyListView::nodeAdded(const QString &buddy, const Identifier &id) {
  QList<QTreeWidgetItem *> items = _tree->findItems(buddy, Qt::MatchExactly);
  QList<QTreeWidgetItem *>::iterator item = items.begin();
  for (; item != items.end(); item++) {
    QTreeWidgetItem *nodeitem = new QTreeWidgetItem(*item);
    nodeitem->setText(0, QString(id.toHex()));
    nodeitem->setIcon(0, QIcon("://icons/fork.png"));
  }
}

void
BuddyListView::nodeRemoved(const QString &buddy, const Identifier &id) {
  QList<QTreeWidgetItem *> items = _tree->findItems(buddy, Qt::MatchExactly);
  QList<QTreeWidgetItem *>::iterator item = items.begin();
  for (; item != items.end(); item++) {
    for (int i=0; i<(*item)->childCount(); i++) {
      if ((*item)->child(i)->text(0) == QString(id.toHex())) {
        (*item)->removeChild((*item)->child(i));
        return;
      }
    }
  }
}

void
BuddyListView::onChat() {
  // Get selected items
  QList<QTreeWidgetItem *> items = _tree->selectedItems();
  if (0 == items.size()) { return; }
  if (items.first()->childCount()) {
    // If buddy is selected
    Identifier id(QByteArray::fromHex(items.first()->child(0)->text(0).toLocal8Bit()));
    _application.startChatWith(id);
  } else {
    // If node is selected
    Identifier id(QByteArray::fromHex(items.first()->text(0).toLocal8Bit()));
    _application.startChatWith(id);
  }
}

void
BuddyListView::onCall() {
  // Get selected items
  QList<QTreeWidgetItem *> items = _tree->selectedItems();
  if (0 == items.size()) { return; }
  if (items.first()->childCount()) {
    // If buddy is selected
    Identifier id(QByteArray::fromHex(items.first()->child(0)->text(0).toLocal8Bit()));
    _application.call(id);
  } else {
    // If node is selected
    Identifier id(QByteArray::fromHex(items.first()->text(0).toLocal8Bit()));
    _application.call(id);
  }
}

void
BuddyListView::onSendFile() {
  // Get file
  QString filename = QFileDialog::getOpenFileName(0, tr("Select file"));
  if (0 == filename.size()) { return; }
  // check file
  QFileInfo fileinfo(filename);
  if (!fileinfo.isReadable()) {
    QMessageBox::critical(0, tr("Can not open file."),
                          tr("Can not open file %1").arg(fileinfo.absoluteFilePath()));
  }
  // Get selected items
  QList<QTreeWidgetItem *> items = _tree->selectedItems();
  if (0 == items.size()) { return; }
  if (items.first()->childCount()) {
    // If buddy is selected
    Identifier id(QByteArray::fromHex(items.first()->child(0)->text(0).toLocal8Bit()));
    _application.sendFile(fileinfo.absoluteFilePath(),
                          fileinfo.size(), id);
  } else {
    // If node is selected
    Identifier id(QByteArray::fromHex(items.first()->text(0).toLocal8Bit()));
    _application.sendFile(fileinfo.absoluteFilePath(),
                          fileinfo.size(), id);
  }
}

void
BuddyListView::onStartProxy() {
  qDebug() << "Not implemented yet.";
}

void
BuddyListView::onSearch() {
  _application.search();
}

void
BuddyListView::onDelete() {
  qDebug() << "Not implemented yet.";
}

void
BuddyListView::closeEvent(QCloseEvent *evt) {
  evt->accept();
  this->deleteLater();
}
