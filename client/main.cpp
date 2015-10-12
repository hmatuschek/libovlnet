#include "application.h"
#include "lib/crypto.h"
#include "lib/logger.h"

#include <time.h>

int main(int argc, char *argv[]) {
  // Init weak RNG
  qsrand(time(0));

  // Setup logger (default level = DEBUG, output = stderr)
  Logger::addHandler(new IOLogHandler());

  Application app(argc, argv);
  app.exec();

  return 0;
}
