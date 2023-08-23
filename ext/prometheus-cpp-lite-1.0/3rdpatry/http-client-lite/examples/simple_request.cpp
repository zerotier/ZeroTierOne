/*
 *  example for httpclientlite.hxx
 */

#include <iostream>
#include <map>
#include <string>

#include <jdl/httpclientlite.h>


using namespace jdl;


int main(int argc, char *argv[]) {
  if (argc == 1) {
    std::cout << "Use " << argv[0] << " http://example.org" << std::endl;
    return EXIT_SUCCESS;
  }

  HTTPResponse response = HTTPClient::request(HTTPClient::POST, URI(argv[1]));

  if (!response.success) {
    std::cout << "Request failed!" << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "Request success" << endl;

  std::cout << "Server protocol: " << response.protocol << std::endl;
  std::cout << "Response code: " << response.response << std::endl;
  std::cout << "Response string: " << response.responseString << std::endl;

  std::cout << "Headers:" << std::endl;

  for (stringMap::iterator it = response.header.begin(); it != response.header.end(); ++it) {
    std::cout << "\t" << it->first << "=" << it->second << std::endl;
  }

  std::cout << response.body << std::endl;

  return EXIT_SUCCESS;
}
