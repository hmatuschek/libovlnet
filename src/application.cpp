#include "application.h"
#include "qntp/NtpReply.h"
#include <QHostAddress>
#include <QDebug>

Application::Application(int &argc, char **argv)
  : QApplication(argc, argv), _ntpClient(), _clockOffset(0)
{
  QObject::connect(&_ntpClient, SIGNAL(replyReceived(QHostAddress,quint16,NtpReply)),
                   this, SLOT(onNTPUpdate(QHostAddress,quint16,NtpReply)));

  updateClockOffset();
}


void
Application::updateClockOffset() {
  // Resolve NTP server address
  QHostInfo::lookupHost("pool.ntp.org", this, SLOT(onNTPAddrResolved(QHostInfo)));
}

void
Application::onNTPAddrResolved(const QHostInfo &host) {
  if (0 == host.addresses().size()) {
    qDebug() << "Can not resolve hostname: " << host.hostName();
    return;
  }
  // sent NTP request
  _ntpClient.sendRequest(host.addresses().front(), 123);
}

void
Application::onNTPUpdate(const QHostAddress &address, quint16 port, const NtpReply &reply) {
  qDebug() << "Clock offset:" << reply.localClockOffset() << "ms";
  _clockOffset = reply.localClockOffset();
}
