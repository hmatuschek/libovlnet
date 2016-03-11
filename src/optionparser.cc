#include "optionparser.hh"
#include "logger.hh"


/* ********************************************************************************************* *
 * Implementation of OptionParser::Option
 * ********************************************************************************************* */
OptionParser::Option::Option(const QString &name, QChar shortOpt, bool flag)
  : _flag(flag), _name(name), _shortOpt(shortOpt)
{
  // pass...
}

OptionParser::Option::Option(const Option &other)
  : _flag(other._flag), _name(other._name), _shortOpt(other._shortOpt)
{
  // pass...
}

OptionParser::Option &
OptionParser::Option::operator =(const OptionParser::Option &other) {
  _flag = other._flag;
  _name = other._name;
  _shortOpt = other._shortOpt;
  return *this;
}

bool
OptionParser::Option::isFlag() const {
  return _flag;
}

const QString &
OptionParser::Option::name() const {
  return _name;
}

bool
OptionParser::Option::matches(const char *arg) const {
  QString arg_str(arg);
  // check short name if defined
  if ((! _shortOpt.isNull()) && arg_str.startsWith(QString("-%1").arg(_shortOpt))) {
    return true;
  }
  // check long name
  return arg_str.startsWith(QString("--%1").arg(_name));
}

QString
OptionParser::Option::value(const char *arg) const {
  QString arg_str(arg);
  if ((! _shortOpt.isNull()) && arg_str.startsWith(QString("-%1").arg(_shortOpt))) {
    return arg_str.mid(3);
  }
  return arg_str.mid(3+_name.length());
}


/* ********************************************************************************************* *
 * Implementation of OptionParser
 * ********************************************************************************************* */
OptionParser::OptionParser()
  : _options(), _arguments(), _values()
{
  // pass...
}

void
OptionParser::add(const QString &name, QChar shortOpt, bool flag) {
  _options.append(Option(name, shortOpt, flag));
}

bool
OptionParser::parse(int &argc, char *argv[]) {
  for (int i=1; i<argc; i++) {
    logDebug() << "Parse " << argv[i];
    QList<Option>::iterator option=_options.begin();
    bool handled=false;
    for (; option!=_options.end(); option++) {
      if ((! handled) && option->matches(argv[i])) {
        if (option->isFlag()) {
          logDebug() << "Matched flag " << option->name();
          _arguments.insert(option->name(), "");
        } else {
          logDebug() << "Matched option " << option->name()
                     << ", value: " << option->value(argv[i]);
          _arguments.insert(option->name(), option->value(argv[i]));
        }
        handled = true;
        break;
      }
    }
    if (! handled) {
      logDebug() << "Add value argument: " << argv[i];
      _values.append(argv[i]);
    }
  }
  return true;
}

bool
OptionParser::hasOption(const QString &name) const {
  return _arguments.contains(name);
}

QString
OptionParser::option(const QString &name) const {
  return _arguments[name];
}

size_t
OptionParser::numValues() const {
  return _values.size();
}

QString
OptionParser::value(size_t i) const {
  return _values.at(i);
}
