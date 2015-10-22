#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QFile>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include "lib/buckets.h"


class SocksServiceWhiteList : public QSet<Identifier>
{
public:
  SocksServiceWhiteList();
  SocksServiceWhiteList(const QJsonArray &lst = QJsonArray());

  QJsonArray toJson() const;
};


/** Holds the settings for the socks proxy service. */
class SocksServiceSettings
{
public:
  /** Constructs the settings from the given JSON representation. */
  SocksServiceSettings(const QJsonObject &object=QJsonObject());
  /** Copy constructor. */
  SocksServiceSettings &operator =(const SocksServiceSettings &other);

  bool enabled() const;
  void enable(bool enabled);

  bool allowBuddies() const;
  void setAllowBuddies(bool allow);

  bool allowWhiteListed() const;
  void setAllowWhiteListed(bool allow);

  SocksServiceWhiteList &whitelist();
  const SocksServiceWhiteList &whitelist() const;

  QJsonObject toJson() const;

protected:
  bool _enabled;
  bool _allowBuddies;
  bool _allowWhitelist;
  SocksServiceWhiteList _whitelist;
};


/** Implements a persistent settings object, collecting the options of several modules and
 * services and keep them in a single file. */
class Settings : public QObject
{
  Q_OBJECT

public:
  /** Loads the settings from the given file (or initializes them with the default ones). */
  explicit Settings(const QString &filename, QObject *parent = 0);

  /** Returns a weak reference to the socks service settings. */
  SocksServiceSettings &socksServiceSettings();

public slots:
  /** Save the current settings into the file give to the constructor. */
  void save();

protected:
  /** The settings file. */
  QFile _file;
  /** Settings for the socks proxy service. */
  SocksServiceSettings _socksServiceSettings;
};

#endif // SETTINGS_H
