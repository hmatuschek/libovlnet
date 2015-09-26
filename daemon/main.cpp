#include "lib/dht.h"
#include "lib/ntp.h"
#include "lib/crypto.h"

#include "application.h"
#include <time.h>

int main(int argc, char *argv[]) {
  qsrand(time(0));

  Application app(argc, argv);

  app.exec();

  return 0;
}
