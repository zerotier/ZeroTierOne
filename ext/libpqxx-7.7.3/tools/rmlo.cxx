// Remove large objects given on the command line from the default database.
#include <iostream>

#include "pqxx/pqxx"


int main(int, char *argv[])
{
  pqxx::connection conn;
  bool failures = false;

  try
  {
    for (int i{1}; argv[i]; ++i)
    {
      auto o{pqxx::from_string<pqxx::oid>(argv[i])};
      try
      {
        pqxx::perform([o, &conn] {
          pqxx::work tx{conn};
          pqxx::blob::remove(tx, o);
          tx.commit();
        });
      }
      catch (std::exception const &e)
      {
        std::cerr << e.what() << std::endl;
        failures = true;
      }
    }
  }
  catch (std::exception const &e)
  {
    std::cerr << e.what() << std::endl;
    return 2;
  }

  return failures;
}
