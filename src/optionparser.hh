#ifndef OPTIONPARSER_HH
#define OPTIONPARSER_HH

#include <QString>
#include <QHash>


class OptionParser
{
protected:
  class Option
  {
  public:
    Option(const QString &name, QChar shortOpt=0, bool flag=false);
    Option(const Option &other);

    Option &operator=(const Option &other);

    bool matches(const char *arg) const;
    QString value(const char *arg) const;

    bool isFlag() const;
    const QString &name() const;

  protected:
    bool _flag;
    QString _name;
    QChar _shortOpt;
  };

public:
  OptionParser();
  void add(const QString &name, QChar shortOpt=0, bool flag=false);

  bool parse(int &argc, char *argv[]);

  bool hasOption(const QString &name) const;
  QString option(const QString &name) const;

  size_t numValues() const;
  QString value(size_t i) const;

protected:
  QList<Option> _options;
  QHash<QString, QString> _arguments;
  QList<QString> _values;
};

#endif // OPTIONPARSER_HH
