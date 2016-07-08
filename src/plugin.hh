#ifndef PLUGIN_HH
#define PLUGIN_HH

#include "node.hh"

class QPluginLoader;
class PluginLoader;

class Plugin
{
public:
  explicit Plugin();
  virtual ~Plugin();

  virtual void init(PluginLoader &loader) = 0;
  virtual bool registerServices(Network &net) = 0;
  virtual bool unregisterServices() = 0;
};

#define Plugin_iid "io.github.hmatuschek.ovlnet.Plugin"
Q_DECLARE_INTERFACE(Plugin, Plugin_iid)


class PluginMeta
{
public:
  PluginMeta();
  PluginMeta(const QString &path);
  PluginMeta(const PluginMeta &other);

  PluginMeta &operator= (const PluginMeta &other);

  bool isValid() const;

  const QString &name() const;
  const QString &version() const;
  const QString &path() const;
  const QStringList &depends() const;

protected:
  QString _path;
  QString _name;
  QString _version;
  QStringList _depends;
};


class PluginLoader: public QObject
{
  Q_OBJECT

public:
  PluginLoader(Node &node, const QString &path, QObject *parent=0);
  virtual ~PluginLoader();

  bool isAvailable(const QString &name) const;
  QStringList availablePlugins() const;
  bool isLoaded(const QString &name) const;
  QStringList loadedPlugins() const;

  bool load(const QString &name);
  bool unload(const QString &name);
  Plugin *plugin(const QString &name);

  const QString &baseDirectory() const;

protected:
  bool isValid(const QString &path);

protected:
  Node &_node;
  QString _basepath;
  QHash<QString, PluginMeta> _availablePlugins;
  QHash<QString, QPluginLoader *> _loadedPlugins;
};

#endif // PLUGIN_HH
