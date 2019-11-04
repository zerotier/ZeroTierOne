// +build ignore

#include "webview.h"

#ifdef _WIN32
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine, int nCmdShow)
#else
int main()
#endif
{
  webview::webview w(true, nullptr);
  w.set_title("Example");
  w.set_size(480, 320, true);
  w.bind("noop", [](std::string s) -> std::string { printf("%s\n", s.c_str());return s; });
  w.bind("add", [](std::string s) -> std::string {
    auto a = std::stoi(webview::json_parse(s, "", 0));
    auto b = std::stoi(webview::json_parse(s, "", 1));
    return std::to_string(a + b);
  });
  w.navigate(R"(data:text/html,
    <!doctype html>
    <html>
      <body>hello</body>
      <script>
        window.onload = function() {
          noop('hello').then(function(res) {
            console.log('noop res', res);
          });
          add(1, 2).then(function(res) {
            console.log('add res', res);
          });
        };
      </script>
    </html>
  )");
  w.run();
  return 0;
}
