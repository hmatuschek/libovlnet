#include "application.hh"
#include "lib/crypto.h"
#include "lib/logger.h"

#include <time.h>
#include <iostream>

#include <QMessageBox>


int main(int argc, char *argv[]) {
  // Init weak RNG
  qsrand(time(0));

  // Setup logger (output = stderr)
  Logger::addHandler(new IOLogHandler(LogMessage::DEBUG));

  Application app(argc, argv);

  if (! app.started()) {
    QMessageBox::critical(
          0, QObject::tr("Can not start Overlay Network Client"),
          QObject::tr("Can not start Overlay Network Client. Is another instance already running?"));
    return 0;
  }

  // go.
  app.exec();

  return 0;
}
