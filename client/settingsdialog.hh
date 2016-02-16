#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QListWidget>
#include "settings.hh"


class WhiteListView: public QWidget
{
  Q_OBJECT

public:
  WhiteListView(QSet<Identifier> &whitelist, QWidget *parent=0);

public slots:
  void apply();

public:
  QSet<Identifier> &_whitelist;
  QListWidget *_listview;
};


class SocksServiceSettingsView: public QWidget
{
  Q_OBJECT

public:
  SocksServiceSettingsView(SocksServiceSettings &settings, QWidget *parent=0);

public slots:
  void apply();

protected slots:
  void _onEnableToggled(bool value);
  void _onAllowBuddiesToggled(bool value);
  void _onAllowWhiteListToggled(bool value);

protected:
  SocksServiceSettings &_settings;
  QCheckBox *_enabled;
  QCheckBox *_allowBuddies;
  QCheckBox *_allowWhiteList;
  WhiteListView *_whitelist;
};


class SettingsDialog : public QDialog
{
  Q_OBJECT

public:
  SettingsDialog(Settings &settings, QWidget *parent = 0);

public slots:
  void apply();

protected:
  Settings &_settings;
  SocksServiceSettingsView *_socks;
};

#endif // SETTINGSDIALOG_H
