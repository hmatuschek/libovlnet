#include "application.hh"

int main(int argc, char *argv[])
{
  // Register log handler
  Logger::addHandler(new IOLogHandler(LogMessage::DEBUG));

  Application app(argc, argv);

  app.exec();
}
