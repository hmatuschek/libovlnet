#include "settingsdialog.hh"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>


/* ********************************************************************************************* *
 * Implementation of SettingsDialog
 * ********************************************************************************************* */
SettingsDialog::SettingsDialog(Settings &settings, QWidget *parent)
  : QDialog(parent), _settings(settings)
{
  setMinimumWidth(640);

  _socks = new SocksServiceSettingsView(settings.socksServiceSettings());

  QTabWidget *tabs = new QTabWidget();
  tabs->addTab(_socks, QIcon("://icons/globe.png"), tr("SOCKS Proxy"));

  QDialogButtonBox *bbox = new QDialogButtonBox(
        QDialogButtonBox::Close | QDialogButtonBox::Apply | QDialogButtonBox::Ok);

  QVBoxLayout *layout = new QVBoxLayout();
  layout->addWidget(tabs);
  layout->addWidget(bbox);
  setLayout(layout);

  connect(bbox, SIGNAL(accepted()), this, SLOT(accept()));
  connect(bbox, SIGNAL(rejected()), this, SLOT(reject()));
  connect(bbox->button(QDialogButtonBox::Apply), SIGNAL(clicked(bool)), this, SLOT(apply()));
}

void
SettingsDialog::apply() {
  _socks->apply();
  _settings.save();
}


/* ********************************************************************************************* *
 * Implementation of SocksServiceSettingsView
 * ********************************************************************************************* */
SocksServiceSettingsView::SocksServiceSettingsView(SocksServiceSettings &settings, QWidget *parent)
  : QWidget(parent), _settings(settings)
{
  _enabled = new QCheckBox();
  _enabled->setChecked(_settings.enabled());
  _allowBuddies = new QCheckBox();
  _allowBuddies->setChecked(_settings.allowBuddies());
  _allowBuddies->setEnabled(_settings.enabled());
  _allowWhiteList = new QCheckBox();
  _allowWhiteList->setChecked(_settings.allowWhiteListed());
  _allowWhiteList->setEnabled(_settings.enabled());
  _whitelist = new WhiteListView(_settings.whitelist());
  _whitelist->setEnabled(_settings.enabled() && _settings.allowWhiteListed());

  QVBoxLayout *layout = new QVBoxLayout();
  QFormLayout *form = new QFormLayout();
  form->addRow(tr("Enabled"), _enabled);
  form->addRow(tr("Allow contacts"), _allowBuddies);
  form->addRow(tr("Allow whitelisted"), _allowWhiteList);
  layout->addLayout(form);
  layout->addWidget(new QLabel(tr("Whitelist:")));
  layout->addWidget(_whitelist);
  setLayout(layout);

  connect(_enabled, SIGNAL(toggled(bool)), this, SLOT(_onEnableToggled(bool)));
  connect(_allowBuddies, SIGNAL(toggled(bool)), this, SLOT(_onAllowBuddiesToggled(bool)));
  connect(_allowWhiteList, SIGNAL(toggled(bool)), this, SLOT(_onAllowWhiteListToggled(bool)));
}

void
SocksServiceSettingsView::apply() {
  _settings.enable(_enabled->isChecked());
  _settings.setAllowBuddies(_allowBuddies->isChecked());
  _settings.setAllowWhiteListed(_allowWhiteList->isChecked());
  _whitelist->apply();
}

void
SocksServiceSettingsView::_onEnableToggled(bool value) {
  _allowBuddies->setEnabled(value);
  _allowWhiteList->setEnabled(value);
  _whitelist->setEnabled(value && _allowWhiteList->isChecked());
}

void
SocksServiceSettingsView::_onAllowBuddiesToggled(bool value) {
  // pass...
}

void
SocksServiceSettingsView::_onAllowWhiteListToggled(bool value) {
  _whitelist->setEnabled(_enabled->isChecked() && value);
}


/* ********************************************************************************************* *
 * Implementation of WhiteListView
 * ********************************************************************************************* */
WhiteListView::WhiteListView(QSet<Identifier> &whitelist, QWidget *parent)
  : QWidget(parent), _whitelist(whitelist)
{
  _listview = new QListWidget();
  // populate listview
  for (QSet<Identifier>::iterator item = _whitelist.begin(); item != _whitelist.end(); item++) {
    _listview->addItem(item->toBase32());
  }

  QPushButton *add = new QPushButton("+");
  QPushButton *rem = new QPushButton("-");

  QVBoxLayout *layout = new QVBoxLayout();
  layout->setContentsMargins(0,0,0,0);
  layout->setSpacing(0);
  QHBoxLayout *bbox = new QHBoxLayout();
  bbox->setContentsMargins(0,0,0,0);
  bbox->addWidget(add);
  bbox->addWidget(rem);
  layout->addLayout(bbox);
  layout->addWidget(_listview, 1);
  setLayout(layout);
}

void
WhiteListView::apply() {
  _whitelist.clear();
  for (int i=0; i<_listview->count(); i++) {
    _whitelist.insert(Identifier::fromBase32(_listview->item(i)->text()));
  }
}




