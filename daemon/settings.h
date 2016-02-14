#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QFile>
#include <QSet>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include "lib/buckets.h"


class ServiceWhiteList : public QSet<Identifier>
{
public:
  ServiceWhiteList(const QJsonArray &lst = QJsonArray());

  QJsonArray toJson() const;
};


/** Implements a persistent settings object, collecting the options of several modules and
 * services and keep them in a single file. */
class Settings : public QObject
{
  Q_OBJECT

public:
  /** Loads the settings from the given file (or initializes them with the default ones). */
  explicit Settings(const QString &filename, QObject *parent = 0);

  /** Returns a weak reference to the socks service whitelist. */
  ServiceWhiteList &socksServiceWhiteList();
  /** Returns a weak reference to the shell service whitelist. */
  ServiceWhiteList &shellServiceWhiteList();

public slots:
  /** Save the current settings into the file give to the constructor. */
  void save();

protected:
  /** The settings file. */
  QFile _file;
  /** Whitelist for the socks proxy service. */
  ServiceWhiteList _socksServiceWhitelist;
  /** Whitelist for the shell service. */
  ServiceWhiteList _shellServiceWhiteList;
};

#endif // SETTINGS_H
