#include "plugin.hh"
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QPluginLoader>
#include <QJsonArray>


/* ********************************************************************************************* *
 * Implementation of Plugin
 * ********************************************************************************************* */
Plugin::Plugin()
{
  // pass...
}

Plugin::~Plugin() {
  // pass...
}


/* ********************************************************************************************* *
 * Implementation of PluginMeta
 * ********************************************************************************************* */
PluginMeta::PluginMeta()
  : _path()
{
  // pass...
}

PluginMeta::PluginMeta(const QString &path)
  : _path()
{
  if (! QLibrary::isLibrary(path))
    return;

  QPluginLoader *loader = new QPluginLoader(path);
  _name = loader->metaData().value("MetaData").toObject().value("name").toString();
  _version = loader->metaData().value("MetaData").toObject().value("version").toString();
  QJsonArray deps = loader->metaData().value("MetaData").toObject().value("dependencies").toArray();
  foreach(QJsonValue val, deps) {
    QString dname = val.toString();
    if (! dname.isEmpty())
      _depends.append(dname);
  }
  _path = path;

  delete loader;

  return;
}

PluginMeta::PluginMeta(const PluginMeta &other)
  : _path(other._path), _name(other._name), _version(other._version), _depends(other._depends)
{
  // pass...
}

PluginMeta &
PluginMeta::operator =(const PluginMeta &other) {
  _path = other._path;
  _name = other._name;
  _version = other._version;
  _depends = other._depends;
  return *this;
}

bool
PluginMeta::isValid() const {
  if (_path.isEmpty() || _path.isNull())
    return false;

  if (! QLibrary::isLibrary(_path))
    return false;

  return true;
}

const QString &
PluginMeta::path() const {
  return _path;
}

const QString &
PluginMeta::name() const {
  return _name;
}

const QString &
PluginMeta::version() const {
  return _version;
}

const QStringList &
PluginMeta::depends() const {
  return _depends;
}


/* ********************************************************************************************* *
 * Implementation of PluginLoader
 * ********************************************************************************************* */
PluginLoader::PluginLoader(Node &node, const QString &path, QObject *parent)
  : QObject(parent), _node(node), _basepath(path)
{
  QDir dir(path);
  foreach(QFileInfo info, dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
    if (isValid(info.absoluteFilePath())) {
      load(info.absoluteFilePath());
    }
  }
}

PluginLoader::~PluginLoader() {
  QStringList loaded = loadedPlugins();
  foreach (QString name, loaded) {
    unload(name);
  }
}

bool
PluginLoader::isAvailable(const QString &name) const {
  return _availablePlugins.contains(name);
}

bool
PluginLoader::isLoaded(const QString &name) const {
  return _loadedPlugins.contains(name);
}

bool
PluginLoader::isValid(const QString &path) {
  if (! QLibrary::isLibrary(path))
    return false;

  PluginMeta meta(path);
  if (! meta.isValid())
    return false;

  logDebug() << "Found plugin " << meta.name() << " @ " << path << ".";
  _availablePlugins.insert(meta.name(), meta);

  return true;
}

bool
PluginLoader::load(const QString &name)
{
  if (_loadedPlugins.contains(name))
    return true;

  if (! _availablePlugins.contains(name))
    return false;

  PluginMeta meta = _availablePlugins[name];
  if (! meta.isValid())
    return false;

  // load all dependencies
  foreach (QString dname, meta.depends()) {
    if (! load(dname))
      return false;
  }

  QPluginLoader *loader = new QPluginLoader(meta.path());
  if (Plugin *plugin = qobject_cast<Plugin *>(loader->instance())) {
    _loadedPlugins.insert(name, loader);
    plugin->init(*this);
    plugin->registerServices(_node);
    return true;
  }

  delete loader;
  return false;
}


bool
PluginLoader::unload(const QString &name)
{
  if (! _loadedPlugins.contains(name))
    return true;

  if (! _availablePlugins.contains(name))
    return false;

  PluginMeta meta = _availablePlugins[name];
  if (! meta.isValid())
    return false;

  if (Plugin *plugin = qobject_cast<Plugin *>(_loadedPlugins[name]->instance())) {
    plugin->unregisterServices();
    delete _loadedPlugins[name];
    _loadedPlugins.remove(name);
    return true;
  }

  return false;
}

Plugin *
PluginLoader::plugin(const QString &name) {
  if (! isLoaded(name)) {
    return 0;
  }
  return qobject_cast<Plugin *>(_loadedPlugins[name]->instance());
}

QStringList
PluginLoader::availablePlugins() const {
  return _availablePlugins.keys();
}

QStringList
PluginLoader::loadedPlugins() const {
  return _loadedPlugins.keys();
}

const QString &
PluginLoader::baseDirectory() const {
  return _basepath;
}
