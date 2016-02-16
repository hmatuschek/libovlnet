#include "lib/dht.h"
#include "lib/ntp.h"
#include "lib/crypto.h"
#include "lib/logger.h"

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
