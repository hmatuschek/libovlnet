#include "application.h"
#include "lib/crypto.h"

#include <time.h>

int main(int argc, char *argv[]) {
  qsrand(time(0));

  Application app(argc, argv);
  app.exec();

  return 0;
}
