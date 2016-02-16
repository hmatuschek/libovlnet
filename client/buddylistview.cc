#include "buddylistview.hh"
#include "application.hh"
#include "searchdialog.hh"
#include "sockswindow.hh"

#include <QVBoxLayout>
#include <QToolBar>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>


BuddyListView::BuddyListView(Application &application, BuddyList *buddies, QWidget *parent)
  : QWidget(parent), _application(application), _buddies(buddies)
{
  setWindowTitle(tr("OvlNet Contacts"));
  setMinimumWidth(300);
  setMinimumHeight(500);

  _tree = new QTreeView();
  _tree->setHeaderHidden(true);
  _tree->setSelectionMode(QAbstractItemView::SingleSelection);
  _tree->setModel(&(_application.buddies()));

  QToolBar *box = new QToolBar();
  box->addAction(QIcon("://icons/chat.png"), tr("Chat"), this, SLOT(onChat()));
  box->addAction(QIcon("://icons/phone.png"), tr("Call"), this, SLOT(onCall()));
  box->addAction(QIcon("://icons/data-transfer-upload.png"), tr("Send file..."),
                 this, SLOT(onSendFile()));
  box->addAction(QIcon("://icons/globe.png"), tr("Start tunnel"), this, SLOT(onStartProxy()));
  box->addSeparator();
  box->addAction(QIcon("://icons/search.png"), tr("Search"), this, SLOT(onSearch()));
  box->addAction(QIcon("://icons/circle-x.png"), tr("Delete"), this, SLOT(onDelete()));

  QVBoxLayout *layout = new QVBoxLayout();
  layout->setSpacing(0);
  layout->setContentsMargins(0,0,0,0);
  layout->addWidget(box);
  layout->addWidget(_tree);
  setLayout(layout);
}

void
BuddyListView::onChat() {
  // Get selected items
  QModelIndexList items = _tree->selectionModel()->selectedIndexes();
  if (0 == items.size()) { return; }
  if (! items.first().isValid()) { return; }

  if (_application.buddies().isBuddy(items.first())) {
    if (0 == _application.buddies().getBuddy(items.first())->numNodes()) { return; }
    _application.startChatWith(
          _application.buddies().getBuddy(items.first())->node(0)->id());
  } else if (_application.buddies().isNode(items.first())) {
    _application.startChatWith(
          _application.buddies().getNode(items.first())->id());
  }
}

void
BuddyListView::onCall() {
  // Get selected items
  QModelIndexList items = _tree->selectionModel()->selectedIndexes();
  if (0 == items.size()) { return; }
  if (! items.first().isValid()) { return; }

  if (_application.buddies().isBuddy(items.first())) {
    if (0 == _application.buddies().getBuddy(items.first())->numNodes()) { return; }
    _application.call(
          _application.buddies().getBuddy(items.first())->node(0)->id());
  } else if (_application.buddies().isNode(items.first())) {
    _application.call(
          _application.buddies().getNode(items.first())->id());
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
  QModelIndexList items = _tree->selectionModel()->selectedIndexes();
  if (0 == items.size()) { return; }
  if (! items.first().isValid()) { return; }

  if (_application.buddies().isBuddy(items.first())) {
    if (0 == _application.buddies().getBuddy(items.first())->numNodes()) { return; }
    _application.sendFile(
          fileinfo.absoluteFilePath(), fileinfo.size(),
          _application.buddies().getBuddy(items.first())->node(0)->id());
  } else if (_application.buddies().isNode(items.first())) {
    _application.sendFile(
          fileinfo.absoluteFilePath(), fileinfo.size(),
          _application.buddies().getNode(items.first())->id());
  }
}

void
BuddyListView::onStartProxy() {
  // Get selected items
  QModelIndexList items = _tree->selectionModel()->selectedIndexes();
  if (0 == items.size()) { return; }
  if (! items.first().isValid()) { return; }

  BuddyList::Node *node = 0;
  if (_application.buddies().isBuddy(items.first())) {
    if (0 == _application.buddies().getBuddy(items.first())->numNodes()) { return; }
    node = _application.buddies().getBuddy(items.first())->node(0);
  } else if (_application.buddies().isNode(items.first())) {
    node = _application.buddies().getNode(items.first());
  }
  if (node->hasBeenSeen()) {
    (new SocksWindow(_application, *node))->show();
  } else {
    QMessageBox::critical(0, tr("Cannot start proxy service."),
                          tr("Node %1 is not reachable.").arg(QString(node->id().toHex())));
  }
}

void
BuddyListView::onSearch() {
  _application.search();
}

void
BuddyListView::onDelete() {
  QModelIndexList items = _tree->selectionModel()->selectedIndexes();
  if ((0 == items.size()) || (!items.first().isValid())) { return; }
  if (_application.buddies().isBuddy(items.first())) {
    QString name = _application.buddies().getBuddy(items.first())->name();
    if (QMessageBox::Yes != QMessageBox::question(0, tr("Delete contact"),
                                                  tr("Delete contact %1").arg(name),
                                                  QMessageBox::No, QMessageBox::Yes)) {
      return;
    }
    _application.buddies().delBuddy(items.first());
  } else if (_application.buddies().isNode(items.first())) {
    QString name = _application.buddies().getNode(items.first())->id().toBase32();
    if (QMessageBox::Yes != QMessageBox::question(0, tr("Delete node"),
                                                  tr("Delete node %1").arg(name),
                                                  QMessageBox::No, QMessageBox::Yes)) {
      return;
    }
    _application.buddies().delNode(items.first());
  }
}

void
BuddyListView::closeEvent(QCloseEvent *evt) {
  evt->accept();
  this->deleteLater();
}
