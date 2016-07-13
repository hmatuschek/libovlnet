/** @defgroup mail Mail service
 * The mail service implements the delivery and cryptographic routines to deliver messages from
 * one node to another node paricipating in a friend-to-friend network.
 * @ingroup services */

#ifndef MAILSERVICE_HH
#define MAILSERVICE_HH

#include <QObject>
#include <QByteArray>
#include "httpservice.hh"
#include "subnetwork.hh"


/** Defines the interface for all MailSpoller classes.
 * @ingroup mail */
class MailSpooler: public QObject
{
  Q_OBJECT

protected:
  /** Hidden constructor. */
  MailSpooler(QObject *parent=0);

public:
  /** Needs to be implemented by every spooler to accept/deny a message with @c message identifier
   * and size @c size, that should be delivered to @c dest originating. */
  virtual bool accept(const Identifier &msgid, const Identifier &dest, size_t size) const = 0;
  /** Spools an accepted message. */
  virtual void spool(const Identifier &msgid, const QByteArray &message) = 0;
};


class MailService : public HttpService
{
  Q_OBJECT

public:
  MailService(Network &network, MailSpooler *spooler, QObject *parent=0);

protected:
  MailSpooler *_spooler;
};


#endif // MAILSERVICE_HH
