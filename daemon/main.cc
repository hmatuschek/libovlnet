#include "lib/node.hh"
#include "lib/ntp.hh"
#include "lib/crypto.hh"
#include "lib/logger.hh"

#include "application.hh"
#include <time.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  // init RNG
  qsrand(time(0));
  // Register log handler
  Logger::addHandler(new IOLogHandler(LogMessage::DEBUG));

  Application app(argc, argv);

  app.exec();

  return 0;
}
