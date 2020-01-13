// +build ignore

#include "webview.h"

#include <cstring>
#include <cassert>

static void test_terminate() {
  webview::webview w(false, nullptr);
  w.dispatch([&]() { w.terminate(); });
  w.run();
}

static void cb_assert_arg(webview_t w, void *arg) {
  assert(w != NULL);
  assert(memcmp(arg, "arg", 3) == 0);
}
static void cb_terminate(webview_t w, void *arg) {
  assert(arg == NULL);
  webview_terminate(w);
}
static void test_c_api() {
  webview_t w;
  w = webview_create(false, NULL);
  webview_set_bounds(w, 100, 100, 480, 320, 0);
  webview_set_title(w, "Test");
  webview_navigate(w, "https://github.com/zserge/webview");
  webview_dispatch(w, cb_assert_arg, (void *)"arg");
  webview_dispatch(w, cb_terminate, NULL);
  webview_run(w);
  webview_destroy(w);
}

int main() {
  test_terminate();
  test_c_api();
  return 0;
}
