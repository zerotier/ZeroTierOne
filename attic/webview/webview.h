/*
 * MIT License
 *
 * Copyright (c) 2017 Serge Zaitsev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef WEBVIEW_H
#define WEBVIEW_H

#ifndef WEBVIEW_API
#define WEBVIEW_API extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void *webview_t;

// Create a new webview instance
WEBVIEW_API webview_t webview_create(int debug, void *wnd);

// Destroy a webview
WEBVIEW_API void webview_destroy(webview_t w);

// Run the main loop
WEBVIEW_API void webview_run(webview_t w);

// Stop the main loop
WEBVIEW_API void webview_terminate(webview_t w);

// Post a function to be executed on the main thread
WEBVIEW_API void
webview_dispatch(webview_t w, void (*fn)(webview_t w, void *arg), void *arg);

WEBVIEW_API void *webview_get_window(webview_t w);

WEBVIEW_API void webview_set_title(webview_t w, const char *title);

WEBVIEW_API void webview_set_bounds(webview_t w, int x, int y, int width,
                                    int height, int flags);
WEBVIEW_API void webview_get_bounds(webview_t w, int *x, int *y, int *width,
                                    int *height, int *flags);

WEBVIEW_API void webview_navigate(webview_t w, const char *url);
WEBVIEW_API void webview_init(webview_t w, const char *js);
WEBVIEW_API void webview_eval(webview_t w, const char *js);

#ifdef __cplusplus
}
#endif

#ifndef WEBVIEW_HEADER

#if !defined(WEBVIEW_GTK) && !defined(WEBVIEW_COCOA) &&                        \
    !defined(WEBVIEW_MSHTML) && !defined(WEBVIEW_EDGE)
#error "please, specify webview backend"
#endif

#include <atomic>
#include <functional>
#include <future>
#include <map>
#include <string>
#include <vector>

#include <cstring>

namespace webview {
using dispatch_fn_t = std::function<void()>;
using msg_cb_t = std::function<void(const char *msg)>;

inline std::string url_encode(std::string s) {
  std::string encoded;
  for (unsigned int i = 0; i < s.length(); i++) {
    auto c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded = encoded + c;
    } else {
      char hex[4];
      snprintf(hex, sizeof(hex), "%%%02x", c);
      encoded = encoded + hex;
    }
  }
  return encoded;
}

inline std::string url_decode(std::string s) {
  std::string decoded;
  for (unsigned int i = 0; i < s.length(); i++) {
    if (s[i] == '%') {
      int n;
      sscanf(s.substr(i + 1, 2).c_str(), "%x", &n);
      decoded = decoded + static_cast<char>(n);
      i = i + 2;
    } else if (s[i] == '+') {
      decoded = decoded + ' ';
    } else {
      decoded = decoded + s[i];
    }
  }
  return decoded;
}

inline std::string html_from_uri(std::string s) {
  if (s.substr(0, 15) == "data:text/html,") {
    return url_decode(s.substr(15));
  }
  return "";
}

inline int json_parse_c(const char *s, size_t sz, const char *key, size_t keysz,
                        const char **value, size_t *valuesz) {
  enum {
    JSON_STATE_VALUE,
    JSON_STATE_LITERAL,
    JSON_STATE_STRING,
    JSON_STATE_ESCAPE,
    JSON_STATE_UTF8
  } state = JSON_STATE_VALUE;
  const char *k = NULL;
  int index = 1;
  int depth = 0;
  int utf8_bytes = 0;

  if (key == NULL) {
    index = keysz;
    keysz = 0;
  }

  *value = NULL;
  *valuesz = 0;

  for (; sz > 0; s++, sz--) {
    enum {
      JSON_ACTION_NONE,
      JSON_ACTION_START,
      JSON_ACTION_END,
      JSON_ACTION_START_STRUCT,
      JSON_ACTION_END_STRUCT
    } action = JSON_ACTION_NONE;
    unsigned char c = *s;
    switch (state) {
    case JSON_STATE_VALUE:
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' ||
          c == ':') {
        continue;
      } else if (c == '"') {
        action = JSON_ACTION_START;
        state = JSON_STATE_STRING;
      } else if (c == '{' || c == '[') {
        action = JSON_ACTION_START_STRUCT;
      } else if (c == '}' || c == ']') {
        action = JSON_ACTION_END_STRUCT;
      } else if (c == 't' || c == 'f' || c == 'n' || c == '-' ||
                 (c >= '0' && c <= '9')) {
        action = JSON_ACTION_START;
        state = JSON_STATE_LITERAL;
      } else {
        return -1;
      }
      break;
    case JSON_STATE_LITERAL:
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' ||
          c == ']' || c == '}' || c == ':') {
        state = JSON_STATE_VALUE;
        s--;
        sz++;
        action = JSON_ACTION_END;
      } else if (c < 32 || c > 126) {
        return -1;
      } // fallthrough
    case JSON_STATE_STRING:
      if (c < 32 || (c > 126 && c < 192)) {
        return -1;
      } else if (c == '"') {
        action = JSON_ACTION_END;
        state = JSON_STATE_VALUE;
      } else if (c == '\\') {
        state = JSON_STATE_ESCAPE;
      } else if (c >= 192 && c < 224) {
        utf8_bytes = 1;
        state = JSON_STATE_UTF8;
      } else if (c >= 224 && c < 240) {
        utf8_bytes = 2;
        state = JSON_STATE_UTF8;
      } else if (c >= 240 && c < 247) {
        utf8_bytes = 3;
        state = JSON_STATE_UTF8;
      } else if (c >= 128 && c < 192) {
        return -1;
      }
      break;
    case JSON_STATE_ESCAPE:
      if (c == '"' || c == '\\' || c == '/' || c == 'b' || c == 'f' ||
          c == 'n' || c == 'r' || c == 't' || c == 'u') {
        state = JSON_STATE_STRING;
      } else {
        return -1;
      }
      break;
    case JSON_STATE_UTF8:
      if (c < 128 || c > 191) {
        return -1;
      }
      utf8_bytes--;
      if (utf8_bytes == 0) {
        state = JSON_STATE_STRING;
      }
      break;
    default:
      return -1;
    }

    if (action == JSON_ACTION_END_STRUCT) {
      depth--;
    }

    if (depth == 1) {
      if (action == JSON_ACTION_START || action == JSON_ACTION_START_STRUCT) {
        if (index == 0) {
          *value = s;
        } else if (keysz > 0 && index == 1) {
          k = s;
        } else {
          index--;
        }
      } else if (action == JSON_ACTION_END ||
                 action == JSON_ACTION_END_STRUCT) {
        if (*value != NULL && index == 0) {
          *valuesz = (size_t)(s + 1 - *value);
          return 0;
        } else if (keysz > 0 && k != NULL) {
          if (keysz == (size_t)(s - k - 1) && memcmp(key, k + 1, keysz) == 0) {
            index = 0;
          } else {
            index = 2;
          }
          k = NULL;
        }
      }
    }

    if (action == JSON_ACTION_START_STRUCT) {
      depth++;
    }
  }
  return -1;
}

inline std::string json_escape(std::string s) {
  // TODO: implement
  return '"' + s + '"';
}

inline int json_unescape(const char *s, size_t n, char *out) {
  int r = 0;
  if (*s++ != '"') {
    return -1;
  }
  while (n > 2) {
    char c = *s;
    if (c == '\\') {
      s++;
      n--;
      switch (*s) {
      case 'b':
        c = '\b';
        break;
      case 'f':
        c = '\f';
        break;
      case 'n':
        c = '\n';
        break;
      case 'r':
        c = '\r';
        break;
      case 't':
        c = '\t';
        break;
      case '\\':
        c = '\\';
        break;
      case '/':
        c = '/';
        break;
      case '\"':
        c = '\"';
        break;
      default: // TODO: support unicode decoding
        return -1;
      }
    }
    if (out != NULL) {
      *out++ = c;
    }
    s++;
    n--;
    r++;
  }
  if (*s != '"') {
    return -1;
  }
  if (out != NULL) {
    *out = '\0';
  }
  return r;
}

inline std::string json_parse(std::string s, std::string key, int index) {
  const char *value;
  size_t value_sz;
  if (key == "") {
    json_parse_c(s.c_str(), s.length(), nullptr, index, &value, &value_sz);
  } else {
    json_parse_c(s.c_str(), s.length(), key.c_str(), key.length(), &value,
                 &value_sz);
  }
  if (value != nullptr) {
    if (value[0] != '"') {
      return std::string(value, value_sz);
    }
    int n = json_unescape(value, value_sz, nullptr);
    if (n > 0) {
      char *decoded = new char[n];
      json_unescape(value, value_sz, decoded);
      auto result = std::string(decoded, n);
      delete[] decoded;
      return result;
    }
  }
  return "";
}

} // namespace webview

#if defined(WEBVIEW_GTK)
//
// ====================================================================
//
// This implementation uses webkit2gtk backend. It requires gtk+3.0 and
// webkit2gtk-4.0 libraries. Proper compiler flags can be retrieved via:
//
//   pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0
//
// ====================================================================
//
#include <JavaScriptCore/JavaScript.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

namespace webview {

class browser_engine {
public:
  browser_engine(msg_cb_t cb, bool debug, void *window)
      : m_cb(cb), m_window(static_cast<GtkWidget *>(window)) {
    gtk_init_check(0, NULL);
    m_window = static_cast<GtkWidget *>(window);
    if (m_window == nullptr) {
      m_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    }
    g_signal_connect(G_OBJECT(m_window), "destroy",
                     G_CALLBACK(+[](GtkWidget *w, gpointer arg) {
                       static_cast<browser_engine *>(arg)->terminate();
                     }),
                     this);
    // Initialize webview widget
    m_webview = webkit_web_view_new();
    WebKitUserContentManager *manager =
        webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(m_webview));
    g_signal_connect(manager, "script-message-received::external",
                     G_CALLBACK(+[](WebKitUserContentManager *m,
                                    WebKitJavascriptResult *r, gpointer arg) {
                       auto *w = static_cast<browser_engine *>(arg);
#if WEBKIT_MAJOR_VERSION >= 2 && WEBKIT_MINOR_VERSION >= 22
                       JSCValue *value =
                           webkit_javascript_result_get_js_value(r);
                       char *s = jsc_value_to_string(value);
#else
                       JSGlobalContextRef ctx =
                           webkit_javascript_result_get_global_context(r);
                       JSValueRef value = webkit_javascript_result_get_value(r);
                       JSStringRef js = JSValueToStringCopy(ctx, value, NULL);
                       size_t n = JSStringGetMaximumUTF8CStringSize(js);
                       char *s = g_new(char, n);
                       JSStringGetUTF8CString(js, s, n);
                       JSStringRelease(js);
#endif
                       w->m_cb(s);
                       g_free(s);
                     }),
                     this);
    webkit_user_content_manager_register_script_message_handler(manager,
                                                                "external");
    init("window.external={invoke:function(s){window.webkit.messageHandlers."
         "external.postMessage(s);}}");

    gtk_container_add(GTK_CONTAINER(m_window), GTK_WIDGET(m_webview));
    gtk_widget_grab_focus(GTK_WIDGET(m_webview));

    if (debug) {
      WebKitSettings *settings =
          webkit_web_view_get_settings(WEBKIT_WEB_VIEW(m_webview));
      webkit_settings_set_enable_write_console_messages_to_stdout(settings,
                                                                  true);
      webkit_settings_set_enable_developer_extras(settings, true);
    }

    gtk_widget_show_all(m_window);
  }
  void run() { gtk_main(); }
  void terminate() { gtk_main_quit(); }
  void dispatch(std::function<void()> f) {
    g_idle_add_full(G_PRIORITY_HIGH_IDLE, (GSourceFunc)([](void *f) -> int {
                      (*static_cast<dispatch_fn_t *>(f))();
                      return G_SOURCE_REMOVE;
                    }),
                    new std::function<void()>(f),
                    [](void *f) { delete static_cast<dispatch_fn_t *>(f); });
  }

  void set_title(const char *title) {
    gtk_window_set_title(GTK_WINDOW(m_window), title);
  }

  void set_size(int width, int height, bool resizable) {
    gtk_window_set_resizable(GTK_WINDOW(m_window), !!resizable);
    if (resizable) {
      gtk_window_set_default_size(GTK_WINDOW(m_window), width, height);
    } else {
      gtk_widget_set_size_request(m_window, width, height);
    }
  }

  void navigate(const char *url) {
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(m_webview), url);
  }

  void init(const char *js) {
    WebKitUserContentManager *manager =
        webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(m_webview));
    webkit_user_content_manager_add_script(
        manager, webkit_user_script_new(
                     js, WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
                     WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, NULL, NULL));
  }

  void eval(const char *js) {
    webkit_web_view_run_javascript(WEBKIT_WEB_VIEW(m_webview), js, NULL, NULL,
                                   NULL);
  }

protected:
  std::function<void(const char *)> m_cb;
  GtkWidget *m_window;
  GtkWidget *m_webview;
};

} // namespace webview

#elif defined(WEBVIEW_COCOA)

//
// ====================================================================
//
// This implementation uses Cocoa WKWebView backend on macOS. It is
// written using ObjC runtime and uses WKWebView class as a browser runtime.
// You should pass "-framework Webkit" flag to the compiler.
//
// ====================================================================
//

#include <CoreGraphics/CoreGraphics.h>
#include <objc/objc-runtime.h>

#define NSBackingStoreBuffered 2

#define NSWindowStyleMaskResizable 8
#define NSWindowStyleMaskMiniaturizable 4
#define NSWindowStyleMaskTitled 1
#define NSWindowStyleMaskClosable 2

#define NSApplicationActivationPolicyRegular 0

#define WKUserScriptInjectionTimeAtDocumentStart 0

namespace webview {

id operator"" _cls(const char *s, std::size_t sz) {
  return (id)objc_getClass(s);
}
SEL operator"" _sel(const char *s, std::size_t sz) {
  return sel_registerName(s);
}
id operator"" _str(const char *s, std::size_t sz) {
  return objc_msgSend("NSString"_cls, "stringWithUTF8String:"_sel, s);
}

class browser_engine {
public:
  browser_engine(msg_cb_t cb, bool debug, void *window) : m_cb(cb) {
    // Application
    id app = objc_msgSend("NSApplication"_cls, "sharedApplication"_sel);
    objc_msgSend(app, "setActivationPolicy:"_sel,
                 NSApplicationActivationPolicyRegular);

    // Delegate
    auto cls = objc_allocateClassPair((Class) "NSObject"_cls, "AppDelegate", 0);
    class_addProtocol(cls, objc_getProtocol("NSApplicationDelegate"));
    class_addProtocol(cls, objc_getProtocol("WKScriptMessageHandler"));
    class_addMethod(
        cls, "applicationShouldTerminateAfterLastWindowClosed:"_sel,
        (IMP)(+[](id self, SEL cmd, id notification) -> BOOL { return 1; }),
        "c@:@");
    class_addMethod(
        cls, "userContentController:didReceiveScriptMessage:"_sel,
        (IMP)(+[](id self, SEL cmd, id notification, id msg) {
          auto w = (browser_engine *)objc_getAssociatedObject(self, "webview");
          w->m_cb((const char *)objc_msgSend(objc_msgSend(msg, "body"_sel),
                                             "UTF8String"_sel));
        }),
        "v@:@@");
    objc_registerClassPair(cls);

    auto delegate = objc_msgSend((id)cls, "new"_sel);
    objc_setAssociatedObject(delegate, "webview", (id)this,
                             OBJC_ASSOCIATION_ASSIGN);
    objc_msgSend(app, sel_registerName("setDelegate:"), delegate);

    // Main window
    if (window == nullptr) {
      m_window = objc_msgSend("NSWindow"_cls, "alloc"_sel);
      m_window = objc_msgSend(
          m_window, "initWithContentRect:styleMask:backing:defer:"_sel,
          CGRectMake(0, 0, 0, 0), 0, NSBackingStoreBuffered, 0);
      set_size(480, 320, true);
    } else {
      m_window = (id)window;
    }

    // Webview
    auto config = objc_msgSend("WKWebViewConfiguration"_cls, "new"_sel);
    m_manager = objc_msgSend(config, "userContentController"_sel);
    m_webview = objc_msgSend("WKWebView"_cls, "alloc"_sel);
    objc_msgSend(m_webview, "initWithFrame:configuration:"_sel,
                 CGRectMake(0, 0, 0, 0), config);
    objc_msgSend(m_manager, "addScriptMessageHandler:name:"_sel, delegate,
                 "external"_str);
    init(R"script(
                      window.external = {
                        invoke: function(s) {
                          window.webkit.messageHandlers.external.postMessage(s);
                        },
                      };
                     )script");
    if (debug) {
      objc_msgSend(objc_msgSend(config, "preferences"_sel),
                   "setValue:forKey:"_sel, 1, "developerExtrasEnabled"_str);
    }
    objc_msgSend(m_window, "setContentView:"_sel, m_webview);
    objc_msgSend(m_window, "makeKeyAndOrderFront:"_sel, nullptr);
  }
  ~browser_engine() { objc_msgSend(m_window, "close"_sel); }
  void terminate() { objc_msgSend("NSApp"_cls, "terminate:"_sel, nullptr); }
  void run() {
    id app = objc_msgSend("NSApplication"_cls, "sharedApplication"_sel);
    dispatch([&]() { objc_msgSend(app, "activateIgnoringOtherApps:"_sel, 1); });
    objc_msgSend(app, "run"_sel);
  }
  void dispatch(std::function<void()> f) {
    dispatch_async_f(dispatch_get_main_queue(), new dispatch_fn_t(f),
                     (dispatch_function_t)([](void *arg) {
                       auto f = static_cast<dispatch_fn_t *>(arg);
                       (*f)();
                       delete f;
                     }));
  }
  void set_title(const char *title) {
    objc_msgSend(
        m_window, "setTitle:"_sel,
        objc_msgSend("NSString"_cls, "stringWithUTF8String:"_sel, title));
  }
  void set_size(int width, int height, bool resizable) {
    auto style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                 NSWindowStyleMaskMiniaturizable;
    if (resizable) {
      style = style | NSWindowStyleMaskResizable;
    }
    objc_msgSend(m_window, "setStyleMask:"_sel, style);
    objc_msgSend(m_window, "setFrame:display:animate:"_sel,
                 CGRectMake(0, 0, width, height), 1, 0);
  }
  void navigate(const char *url) {
    auto nsurl = objc_msgSend(
        "NSURL"_cls, "URLWithString:"_sel,
        objc_msgSend("NSString"_cls, "stringWithUTF8String:"_sel, url));
    objc_msgSend(
        m_webview, "loadRequest:"_sel,
        objc_msgSend("NSURLRequest"_cls, "requestWithURL:"_sel, nsurl));
  }
  void init(const char *js) {
    objc_msgSend(
        m_manager, "addUserScript:"_sel,
        objc_msgSend(
            objc_msgSend("WKUserScript"_cls, "alloc"_sel),
            "initWithSource:injectionTime:forMainFrameOnly:"_sel,
            objc_msgSend("NSString"_cls, "stringWithUTF8String:"_sel, js),
            WKUserScriptInjectionTimeAtDocumentStart, 1));
  }
  void eval(const char *js) {
    objc_msgSend(m_webview, "evaluateJavaScript:completionHandler:"_sel,
                 objc_msgSend("NSString"_cls, "stringWithUTF8String:"_sel, js),
                 nullptr);
  }

protected:
  id m_window;
  id m_webview;
  id m_manager;
  msg_cb_t m_cb;
};

} // namespace webview

#elif defined(WEBVIEW_MSHTML) || defined(WEBVIEW_EDGE)

//
// ====================================================================
//
// This implementation uses Win32 API to create a native window. It can
// use either MSHTML or EdgeHTML backend as a browser engine.
//
// ====================================================================
//

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#pragma comment(lib, "user32.lib")
namespace webview {
class browser_window {
public:
  browser_window(msg_cb_t cb, void *window) : m_cb(cb) {
    if (window == nullptr) {
      WNDCLASSEX wc;
      ZeroMemory(&wc, sizeof(WNDCLASSEX));
      wc.cbSize = sizeof(WNDCLASSEX);
      wc.hInstance = GetModuleHandle(nullptr);
      wc.lpszClassName = "webview";
      wc.lpfnWndProc =
          (WNDPROC)(+[](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> int {
            auto w = (browser_window *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            switch (msg) {
            case WM_SIZE:
              w->resize();
              break;
            case WM_CLOSE:
              DestroyWindow(hwnd);
              break;
            case WM_DESTROY:
              w->terminate();
              break;
            default:
              return DefWindowProc(hwnd, msg, wp, lp);
            }
            return 0;
          });
      RegisterClassEx(&wc);
      m_window = CreateWindow("webview", "", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                              CW_USEDEFAULT, 640, 480, nullptr, nullptr,
                              GetModuleHandle(nullptr), nullptr);
      SetWindowLongPtr(m_window, GWLP_USERDATA, (LONG_PTR)this);
    } else {
      m_window = *(static_cast<HWND *>(window));
    }

    ShowWindow(m_window, SW_SHOW);
    UpdateWindow(m_window);
    SetFocus(m_window);
  }

  void run() {
    MSG msg;
    BOOL res;
    while ((res = GetMessage(&msg, nullptr, 0, 0)) != -1) {
      if (msg.hwnd) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        continue;
      }
      if (msg.message == WM_APP) {
        auto f = (dispatch_fn_t *)(msg.lParam);
        (*f)();
        delete f;
      } else if (msg.message == WM_QUIT) {
        return;
      }
    }
  }

  void terminate() { PostQuitMessage(0); }
  void dispatch(dispatch_fn_t f) {
    PostThreadMessage(m_main_thread, WM_APP, 0, (LPARAM) new dispatch_fn_t(f));
  }

  void set_title(const char *title) { SetWindowText(m_window, title); }

  void set_size(int width, int height, bool resizable) {
    RECT r;
    r.left = 50;
    r.top = 50;
    r.right = width;
    r.bottom = height;
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, 0);
    SetWindowPos(m_window, NULL, r.left, r.top, r.right - r.left,
                 r.bottom - r.top,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  }

protected:
  virtual void resize() {}
  HWND m_window;
  DWORD m_main_thread = GetCurrentThreadId();
  msg_cb_t m_cb;
};
} // namespace webview

#if defined(WEBVIEW_MSHTML)
#include <exdisp.h>
#include <exdispid.h>
#include <mshtmhst.h>
#include <mshtml.h>
#include <shobjidl.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

#define DISPID_EXTERNAL_INVOKE 0x1000

namespace webview {
class browser_engine : public browser_window,
                       public IOleClientSite,
                       public IOleInPlaceSite,
                       public IOleInPlaceFrame,
                       public IDocHostUIHandler,
                       public DWebBrowserEvents2 {
public:
  browser_engine(msg_cb_t cb, bool debug, void *window)
      : browser_window(cb, window) {
    RECT rect;
    LPCLASSFACTORY cf = nullptr;
    IOleObject *obj = nullptr;

    fix_ie_compat_mode();

    OleInitialize(nullptr);
    CoGetClassObject(CLSID_WebBrowser,
                     CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER, nullptr,
                     IID_IClassFactory, (void **)&cf);
    cf->CreateInstance(nullptr, IID_IOleObject, (void **)&obj);
    cf->Release();

    obj->SetClientSite(this);
    OleSetContainedObject(obj, TRUE);
    GetWindowRect(m_window, &rect);
    obj->DoVerb(OLEIVERB_INPLACEACTIVATE, NULL, this, -1, m_window, &rect);
    obj->QueryInterface(IID_IWebBrowser2, (void **)&m_webview);

    IConnectionPointContainer *cpc;
    IConnectionPoint *cp;
    DWORD cookie;
    m_webview->QueryInterface(IID_IConnectionPointContainer, (void **)&cpc);
    cpc->FindConnectionPoint(DIID_DWebBrowserEvents2, &cp);
    cpc->Release();
    cp->Advise(static_cast<IOleClientSite *>(this), &cookie);

    resize();
    navigate("about:blank");
  }

  ~browser_engine() { OleUninitialize(); }

  void navigate(const char *url) {
    VARIANT v;
    DWORD size = MultiByteToWideChar(CP_UTF8, 0, url, -1, 0, 0);
    WCHAR *ws = (WCHAR *)GlobalAlloc(GMEM_FIXED, sizeof(WCHAR) * size);
    MultiByteToWideChar(CP_UTF8, 0, url, -1, ws, size);
    VariantInit(&v);
    v.vt = VT_BSTR;
    v.bstrVal = SysAllocString(ws);
    m_webview->Navigate2(&v, nullptr, nullptr, nullptr, nullptr);
    VariantClear(&v);
  }

  void eval(const char *js) {
    // TODO
  }

private:
  IWebBrowser2 *m_webview;

  int fix_ie_compat_mode() {
    const char *WEBVIEW_KEY_FEATURE_BROWSER_EMULATION =
        "Software\\Microsoft\\Internet "
        "Explorer\\Main\\FeatureControl\\FEATURE_BROWSER_EMULATION";
    HKEY hKey;
    DWORD ie_version = 11000;
    TCHAR appname[MAX_PATH + 1];
    TCHAR *p;
    if (GetModuleFileName(NULL, appname, MAX_PATH + 1) == 0) {
      return -1;
    }
    for (p = &appname[strlen(appname) - 1]; p != appname && *p != '\\'; p--) {
    }
    p++;
    if (RegCreateKey(HKEY_CURRENT_USER, WEBVIEW_KEY_FEATURE_BROWSER_EMULATION,
                     &hKey) != ERROR_SUCCESS) {
      return -1;
    }
    if (RegSetValueEx(hKey, p, 0, REG_DWORD, (BYTE *)&ie_version,
                      sizeof(ie_version)) != ERROR_SUCCESS) {
      RegCloseKey(hKey);
      return -1;
    }
    RegCloseKey(hKey);
    return 0;
  }

  // Inheruted via browser_window
  void resize() override {
    RECT rect;
    GetClientRect(m_window, &rect);
    m_webview->put_Left(0);
    m_webview->put_Top(0);
    m_webview->put_Width(rect.right);
    m_webview->put_Height(rect.bottom);
    m_webview->put_Visible(VARIANT_TRUE);
  }

  // Inherited via IUnknown
  ULONG __stdcall AddRef(void) override { return 1; }
  ULONG __stdcall Release(void) override { return 1; }
  HRESULT __stdcall QueryInterface(REFIID riid, void **obj) override {
    if (riid == IID_IUnknown || riid == IID_IOleClientSite) {
      *obj = static_cast<IOleClientSite *>(this);
      return S_OK;
    }
    if (riid == IID_IOleInPlaceSite) {
      *obj = static_cast<IOleInPlaceSite *>(this);
      return S_OK;
    }
    if (riid == IID_IDocHostUIHandler) {
      *obj = static_cast<IDocHostUIHandler *>(this);
      return S_OK;
    }
    if (riid == IID_IDispatch || riid == DIID_DWebBrowserEvents2) {
      *obj = static_cast<IDispatch *>(this);
      return S_OK;
    }
    *obj = nullptr;
    return E_NOINTERFACE;
  }

  // Inherited via IOleClientSite
  HRESULT __stdcall SaveObject(void) override { return E_NOTIMPL; }
  HRESULT __stdcall GetMoniker(DWORD dwAssign, DWORD dwWhichMoniker,
                               IMoniker **ppmk) override {
    return E_NOTIMPL;
  }
  HRESULT __stdcall GetContainer(IOleContainer **ppContainer) override {
    *ppContainer = nullptr;
    return E_NOINTERFACE;
  }
  HRESULT __stdcall ShowObject(void) override { return S_OK; }
  HRESULT __stdcall OnShowWindow(BOOL fShow) override { return S_OK; }
  HRESULT __stdcall RequestNewObjectLayout(void) override { return E_NOTIMPL; }

  // Inherited via IOleInPlaceSite
  HRESULT __stdcall GetWindow(HWND *phwnd) override {
    *phwnd = m_window;
    return S_OK;
  }
  HRESULT __stdcall ContextSensitiveHelp(BOOL fEnterMode) override {
    return E_NOTIMPL;
  }
  HRESULT __stdcall CanInPlaceActivate(void) override { return S_OK; }
  HRESULT __stdcall OnInPlaceActivate(void) override { return S_OK; }
  HRESULT __stdcall OnUIActivate(void) override { return S_OK; }
  HRESULT __stdcall GetWindowContext(
      IOleInPlaceFrame **ppFrame, IOleInPlaceUIWindow **ppDoc,
      LPRECT lprcPosRect, LPRECT lprcClipRect,
      LPOLEINPLACEFRAMEINFO lpFrameInfo) override {
    *ppFrame = static_cast<IOleInPlaceFrame *>(this);
    *ppDoc = nullptr;
    lpFrameInfo->fMDIApp = FALSE;
    lpFrameInfo->hwndFrame = m_window;
    lpFrameInfo->haccel = 0;
    lpFrameInfo->cAccelEntries = 0;
    return S_OK;
  }
  HRESULT __stdcall Scroll(SIZE scrollExtant) override { return E_NOTIMPL; }
  HRESULT __stdcall OnUIDeactivate(BOOL fUndoable) override { return S_OK; }
  HRESULT __stdcall OnInPlaceDeactivate(void) override { return S_OK; }
  HRESULT __stdcall DiscardUndoState(void) override { return E_NOTIMPL; }
  HRESULT __stdcall DeactivateAndUndo(void) override { return E_NOTIMPL; }
  HRESULT __stdcall OnPosRectChange(LPCRECT lprcPosRect) override {
    IOleInPlaceObject *inplace;
    m_webview->QueryInterface(IID_IOleInPlaceObject, (void **)&inplace);
    inplace->SetObjectRects(lprcPosRect, lprcPosRect);
    return S_OK;
  }

  // Inherited via IDocHostUIHandler
  HRESULT __stdcall ShowContextMenu(DWORD dwID, POINT *ppt,
                                    IUnknown *pcmdtReserved,
                                    IDispatch *pdispReserved) override {
    return S_OK;
  }
  HRESULT __stdcall GetHostInfo(DOCHOSTUIINFO *pInfo) override {
    pInfo->dwDoubleClick = DOCHOSTUIDBLCLK_DEFAULT;
    pInfo->dwFlags = DOCHOSTUIFLAG_NO3DBORDER;
    return S_OK;
  }
  HRESULT __stdcall ShowUI(DWORD dwID, IOleInPlaceActiveObject *pActiveObject,
                           IOleCommandTarget *pCommandTarget,
                           IOleInPlaceFrame *pFrame,
                           IOleInPlaceUIWindow *pDoc) override {
    return S_OK;
  }
  HRESULT __stdcall HideUI(void) override { return S_OK; }
  HRESULT __stdcall UpdateUI(void) override { return S_OK; }
  HRESULT __stdcall EnableModeless(BOOL fEnable) override { return S_OK; }
  HRESULT __stdcall OnDocWindowActivate(BOOL fActivate) override {
    return S_OK;
  }
  HRESULT __stdcall OnFrameWindowActivate(BOOL fActivate) override {
    return S_OK;
  }
  HRESULT __stdcall ResizeBorder(LPCRECT prcBorder,
                                 IOleInPlaceUIWindow *pUIWindow,
                                 BOOL fRameWindow) override {
    return S_OK;
  }
  HRESULT __stdcall GetOptionKeyPath(LPOLESTR *pchKey, DWORD dw) override {
    return S_FALSE;
  }
  HRESULT __stdcall GetDropTarget(IDropTarget *pDropTarget,
                                  IDropTarget **ppDropTarget) override {
    return E_NOTIMPL;
  }
  HRESULT __stdcall GetExternal(IDispatch **ppDispatch) override {
    *ppDispatch = static_cast<IDispatch *>(this);
    return S_OK;
  }
  HRESULT __stdcall TranslateUrl(DWORD dwTranslate, LPWSTR pchURLIn,
                                 LPWSTR *ppchURLOut) override {
    *ppchURLOut = nullptr;
    return S_FALSE;
  }
  HRESULT __stdcall FilterDataObject(IDataObject *pDO,
                                     IDataObject **ppDORet) override {
    *ppDORet = nullptr;
    return S_FALSE;
  }
  HRESULT __stdcall TranslateAcceleratorA(LPMSG lpMsg,
                                          const GUID *pguidCmdGroup,
                                          DWORD nCmdID) {
    return S_FALSE;
  }

  // Inherited via IOleInPlaceFrame
  HRESULT __stdcall GetBorder(LPRECT lprectBorder) override { return S_OK; }
  HRESULT __stdcall RequestBorderSpace(LPCBORDERWIDTHS pborderwidths) override {
    return S_OK;
  }
  HRESULT __stdcall SetBorderSpace(LPCBORDERWIDTHS pborderwidths) override {
    return S_OK;
  }
  HRESULT __stdcall SetActiveObject(IOleInPlaceActiveObject *pActiveObject,
                                    LPCOLESTR pszObjName) override {
    return S_OK;
  }
  HRESULT __stdcall InsertMenus(HMENU hmenuShared,
                                LPOLEMENUGROUPWIDTHS lpMenuWidths) override {
    return S_OK;
  }
  HRESULT __stdcall SetMenu(HMENU hmenuShared, HOLEMENU holemenu,
                            HWND hwndActiveObject) override {
    return S_OK;
  }
  HRESULT __stdcall RemoveMenus(HMENU hmenuShared) override { return S_OK; }
  HRESULT __stdcall SetStatusText(LPCOLESTR pszStatusText) override {
    return S_OK;
  }
  HRESULT __stdcall TranslateAcceleratorA(LPMSG lpmsg, WORD wID) {
    return S_OK;
  }

  // Inherited via IDispatch
  HRESULT __stdcall GetTypeInfoCount(UINT *pctinfo) override { return S_OK; }
  HRESULT __stdcall GetTypeInfo(UINT iTInfo, LCID lcid,
                                ITypeInfo **ppTInfo) override {
    return S_OK;
  }
  HRESULT __stdcall GetIDsOfNames(REFIID riid, LPOLESTR *rgszNames, UINT cNames,
                                  LCID lcid, DISPID *rgDispId) override {
    *rgDispId = DISPID_EXTERNAL_INVOKE;
    return S_OK;
  }
  HRESULT __stdcall Invoke(DISPID dispIdMember, REFIID riid, LCID lcid,
                           WORD wFlags, DISPPARAMS *pDispParams,
                           VARIANT *pVarResult, EXCEPINFO *pExcepInfo,
                           UINT *puArgErr) override {
    if (dispIdMember == DISPID_NAVIGATECOMPLETE2) {
    } else if (dispIdMember == DISPID_DOCUMENTCOMPLETE) {
    } else if (dispIdMember == DISPID_EXTERNAL_INVOKE) {
    }
    return S_OK;
  }
};
} // namespace webview

#elif defined(WEBVIEW_EDGE)
#include <objbase.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.UI.Interop.h>

#pragma comment(lib, "windowsapp")

namespace webview {

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Web::UI;
using namespace Windows::Web::UI::Interop;

class browser_engine : public browser_window {
public:
  browser_engine(msg_cb_t cb, bool debug, void *window)
      : browser_window(cb, window) {
    init_apartment(winrt::apartment_type::single_threaded);
    m_process = WebViewControlProcess();
    auto op = m_process.CreateWebViewControlAsync(
        reinterpret_cast<int64_t>(m_window), Rect());
    if (op.Status() != AsyncStatus::Completed) {
      handle h(CreateEvent(nullptr, false, false, nullptr));
      op.Completed([h = h.get()](auto, auto) { SetEvent(h); });
      HANDLE hs[] = {h.get()};
      DWORD i;
      CoWaitForMultipleHandles(COWAIT_DISPATCH_WINDOW_MESSAGES |
                                   COWAIT_DISPATCH_CALLS |
                                   COWAIT_INPUTAVAILABLE,
                               INFINITE, 1, hs, &i);
    }
    m_webview = op.GetResults();
    m_webview.Settings().IsScriptNotifyAllowed(true);
    m_webview.IsVisible(true);
    m_webview.ScriptNotify([=](auto const &sender, auto const &args) {
      std::string s = winrt::to_string(args.Value());
      m_cb(s.c_str());
    });
    m_webview.NavigationStarting([=](auto const &sender, auto const &args) {
      m_webview.AddInitializeScript(winrt::to_hstring(init_js));
    });
    init("window.external.invoke = s => window.external.notify(s)");
    resize();
  }

  void navigate(const char *url) {
    Uri uri(winrt::to_hstring(url));
    // TODO: if url starts with 'data:text/html,' prefix then use it as a string
    m_webview.Navigate(uri);
    // m_webview.NavigateToString(winrt::to_hstring(url));
  }
  void init(const char *js) {
    init_js = init_js + "(function(){" + js + "})();";
  }
  void eval(const char *js) {
    m_webview.InvokeScriptAsync(
        L"eval", single_threaded_vector<hstring>({winrt::to_hstring(js)}));
  }

private:
  void resize() {
    RECT r;
    GetClientRect(m_window, &r);
    Rect bounds(r.left, r.top, r.right - r.left, r.bottom - r.top);
    m_webview.Bounds(bounds);
  }
  WebViewControlProcess m_process;
  WebViewControl m_webview = nullptr;
  std::string init_js = "";
};
} // namespace webview
#endif

#endif /* WEBVIEW_GTK, WEBVIEW_COCOA, WEBVIEW_MSHTML, WEBVIEW_MSHTML */

namespace webview {

class webview : public browser_engine {
public:
  webview(bool debug = false, void *wnd = nullptr)
      : browser_engine(
            std::bind(&webview::on_message, this, std::placeholders::_1), debug,
            wnd) {}

  void *window() { return (void *)m_window; }

  void navigate(const char *url) {
    std::string html = html_from_uri(url);
    if (html != "") {
      browser_engine::navigate(("data:text/html," + url_encode(html)).c_str());
    } else {
      browser_engine::navigate(url);
    }
  }

  using binding_t = std::function<std::string(std::string)>;

  void bind(const char *name, binding_t f) {
    auto js = "(function() { var name = '" + std::string(name) + "';" + R"(
      window[name] = function() {
        var me = window[name];
        var errors = me['errors'];
        var callbacks = me['callbacks'];
        if (!callbacks) {
          callbacks = {};
          me['callbacks'] = callbacks;
        }
        if (!errors) {
          errors = {};
          me['errors'] = errors;
        }
        var seq = (me['lastSeq'] || 0) + 1;
        me['lastSeq'] = seq;
        var promise = new Promise(function(resolve, reject) {
          callbacks[seq] = resolve;
          errors[seq] = reject;
        });
        window.external.invoke(JSON.stringify({
          name: name,
          seq:seq,
          args: Array.prototype.slice.call(arguments),
        }));
        return promise;
      }
    })())";
    init(js.c_str());
    bindings[name] = new binding_t(f);
  }

private:
  void on_message(const char *msg) {
    auto seq = json_parse(msg, "seq", 0);
    auto name = json_parse(msg, "name", 0);
    auto args = json_parse(msg, "args", 0);
    auto fn = bindings[name];
    if (fn == nullptr) {
      return;
    }
    std::async(std::launch::async, [=]() {
      auto result = (*fn)(args);
      dispatch([=]() {
        eval(("var b = window['" + name + "'];b['callbacks'][" + seq + "](" +
              result + ");b['callbacks'][" + seq +
              "] = undefined;b['errors'][" + seq + "] = undefined;")
                 .c_str());
      });
    });
  }
  std::map<std::string, binding_t *> bindings;
};
} // namespace webview

WEBVIEW_API webview_t webview_create(int debug, void *wnd) {
  return new webview::webview(debug, wnd);
}

WEBVIEW_API void webview_destroy(webview_t w) {
  delete static_cast<webview::webview *>(w);
}

WEBVIEW_API void webview_run(webview_t w) {
  static_cast<webview::webview *>(w)->run();
}

WEBVIEW_API void webview_terminate(webview_t w) {
  static_cast<webview::webview *>(w)->terminate();
}

WEBVIEW_API void
webview_dispatch(webview_t w, void (*fn)(webview_t w, void *arg), void *arg) {
  static_cast<webview::webview *>(w)->dispatch([=]() { fn(w, arg); });
}

WEBVIEW_API void *webview_get_window(webview_t w) {
  return static_cast<webview::webview *>(w)->window();
}

WEBVIEW_API void webview_set_title(webview_t w, const char *title) {
  static_cast<webview::webview *>(w)->set_title(title);
}

WEBVIEW_API void webview_set_bounds(webview_t w, int x, int y, int width,
                                    int height, int flags) {
  // TODO: x, y, flags
  static_cast<webview::webview *>(w)->set_size(width, height, true);
}

WEBVIEW_API void webview_get_bounds(webview_t w, int *x, int *y, int *width,
                                    int *height, int *flags) {
  // TODO
}

WEBVIEW_API void webview_navigate(webview_t w, const char *url) {
  static_cast<webview::webview *>(w)->navigate(url);
}

WEBVIEW_API void webview_init(webview_t w, const char *js) {
  static_cast<webview::webview *>(w)->init(js);
}

WEBVIEW_API void webview_eval(webview_t w, const char *js) {
  static_cast<webview::webview *>(w)->eval(js);
}

#endif /* WEBVIEW_HEADER */

#endif /* WEBVIEW_H */
