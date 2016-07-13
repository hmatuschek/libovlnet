#include "mailservice.hh"

MailService::MailService(Network &network, MailSpooler *spooler, QObject *parent)
  : HttpService(network, 0, parent), _spooler(spooler)
{
  // Take ownership
  _spooler->setParent(this);
}

