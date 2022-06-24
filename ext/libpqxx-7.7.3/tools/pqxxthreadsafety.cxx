// Print thread-safety information for present libpqxx build.
#include <iostream>

#include "pqxx/util"


int main()
{
  std::cout << pqxx::describe_thread_safety().description << std::endl;
}
