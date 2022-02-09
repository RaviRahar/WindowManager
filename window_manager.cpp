extern "C" {
#include <X11/Xlib.h>
#include <X11/keysym.h>
}
//#include <memory>
#include <cstdlib>
#include <glog/logging.h>
#include <unordered_map>

// naming convention OnEventZ is used for function on taking action on event EventZ

class WindowManager{

  private:

    Display* _display;
    const Window _root;

    static bool _another_wm_detected;
    static int OnAnotherWMDetected(Display* display, XErrorEvent* e);
    static int OnXError(Display* display, XErrorEvent* e);

    std::unordered_map<Window, Window> _clients;

  public:

    bool no_display;
    WindowManager();
    ~WindowManager();
    // Run_Check checks if another window manager is already running
    void Run_Check();
    void Run();

    void OnDestroyNotify(const XDestroyWindowEvent& e);
    void OnReparentNotify(const XReparentEvent& e);
    void OnCreateNotify(const XCreateWindowEvent& e);
    void OnConfigureRequest(const XConfigureRequestEvent& e);
    void OnMapRequest(const XMapRequestEvent& e);
    void Frame(Window w, bool was_created_before_window_manager);
    void OnMapNotify(const XMapEvent& e);

    void OnUnmapNotify(const XUnmapEvent& e);
    void Unframe(Window w);

};


bool WindowManager::_another_wm_detected;


WindowManager::WindowManager():
  _display(XOpenDisplay(nullptr)),
  _root(DefaultRootWindow(_display))
{
  no_display = false;

  if (_display == nullptr){
    LOG(ERROR) << "Error Detecting Display";
    no_display = true;
  }

}

WindowManager::~WindowManager(){
  XCloseDisplay(_display);
}


void WindowManager::Run_Check(){

  _another_wm_detected = false;
  XSetErrorHandler(&WindowManager::OnAnotherWMDetected);

  XSelectInput(
      _display,
      _root,
      SubstructureRedirectMask | SubstructureNotifyMask);

  XSync(_display, false);

  if (_another_wm_detected) {
    LOG(ERROR) << "Detected another window manager on display";
  }

}

void WindowManager::Run(){

  XSetErrorHandler(&WindowManager::OnXError);

  XGrabServer(_display);

  Window returned_root, returned_parent;
  Window* top_level_windows;
  unsigned int num_top_level_windows;

  CHECK(XQueryTree(
      _display,
      _root,
      &returned_root,
      &returned_parent,
      &top_level_windows,
      &num_top_level_windows));

  CHECK_EQ(returned_root, _root);

  for (unsigned int i = 0; i < num_top_level_windows; ++i) {

    Frame(top_level_windows[i], true);

  }

  XFree(top_level_windows);

  XUngrabServer(_display);

  for (;;) {

    XEvent e;

    XNextEvent(_display, &e);
    LOG(INFO) << "Received event: " << e.type;

    switch (e.type) {

      case DestroyNotify:
        OnDestroyNotify(e.xdestroywindow);
        break;

      case ReparentNotify:
        OnReparentNotify(e.xreparent);
        break;

      case CreateNotify:
        OnCreateNotify(e.xcreatewindow);
        break;

      case ConfigureRequest:
        OnConfigureRequest(e.xconfigurerequest);
        break;

      case MapRequest:
        OnMapRequest(e.xmaprequest);
        break;

      case MapNotify:
        OnMapNotify(e.xmap);
        break;

      case UnmapNotify:
        OnUnmapNotify(e.xunmap);
        break;

      default:
        LOG(WARNING) << "Ignored event";
    }
  }

}


int WindowManager::OnAnotherWMDetected(Display* display, XErrorEvent* e) {

  CHECK_EQ((int)(e->error_code), BadAccess);
  _another_wm_detected = true;
  return 0;

}

int WindowManager::OnXError(Display* display, XErrorEvent* e) {return 0;}

void WindowManager::OnDestroyNotify(const XDestroyWindowEvent& e) {}

void WindowManager::OnReparentNotify(const XReparentEvent& e) {}

void WindowManager::OnCreateNotify(const XCreateWindowEvent& e) {}

void WindowManager::OnConfigureRequest(const XConfigureRequestEvent& e) {

  XWindowChanges changes;
  changes.x = e.x;
  changes.y = e.y;
  changes.width = e.width;
  changes.height = e.height;
  changes.border_width = e.border_width;
  changes.sibling = e.above;
  changes.stack_mode = e.detail;

  if (_clients.count(e.window)) {
    const Window frame = _clients[e.window];
    XConfigureWindow(_display, frame, e.value_mask, &changes);
    LOG(INFO) << "Resize [" << frame << "] to " << e.width << e.height;
  }

  // Grant request by calling XConfigureWindow().
  XConfigureWindow(_display, e.window, e.value_mask, &changes);
  LOG(INFO) << "Resize " << e.window << " to " << e.width << e.height;

}

void WindowManager::OnMapRequest(const XMapRequestEvent& e) {

  Frame(e.window, false);
  XMapWindow(_display, e.window);

}

void WindowManager::Frame(Window w, bool was_created_before_window_manager) {

  const unsigned int BORDER_WIDTH = 3;
  const unsigned long BORDER_COLOR = 0xff0000;
  const unsigned long BG_COLOR = 0x0000ff;

  XWindowAttributes x_window_attrs;
  CHECK(XGetWindowAttributes(_display, w, &x_window_attrs));

  if (was_created_before_window_manager) {
    if (x_window_attrs.override_redirect ||
        x_window_attrs.map_state != IsViewable) {
      return;
    }
  }

  const Window frame = XCreateSimpleWindow(
      _display,
      _root,
      x_window_attrs.x,
      x_window_attrs.y,
      x_window_attrs.width,
      x_window_attrs.height,
      BORDER_WIDTH,
      BORDER_COLOR,
      BG_COLOR);

  XSelectInput(
      _display,
      frame,
      SubstructureRedirectMask | SubstructureNotifyMask);

  XAddToSaveSet(_display, w);

  XReparentWindow(
      _display,
      w,
      frame,
      0, 0); 

  XMapWindow(_display, frame);
  _clients[w] = frame;

  XGrabButton(
      _display,
      Button1,
      Mod1Mask,
      w,
      false,
      ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
      GrabModeAsync,
      GrabModeAsync,
      None,
      None);

  XGrabButton(
      _display,
      Button3,
      Mod1Mask,
      w,
      false,
      ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
      GrabModeAsync,
      GrabModeAsync,
      None,
      None);

  XGrabKey(
      _display,
      XKeysymToKeycode(_display, XK_F4),
      Mod1Mask,
      w,
      false,
      GrabModeAsync,
      GrabModeAsync);

  XGrabKey(
      _display,
      XKeysymToKeycode(_display, XK_Tab),
      Mod1Mask,
      w,
      false,
      GrabModeAsync,
      GrabModeAsync);

  LOG(INFO) << "Framed window " << w << " [" << frame << "]";

}

void WindowManager::OnMapNotify(const XMapEvent& e) {}


void WindowManager::OnUnmapNotify(const XUnmapEvent& e) {

  if (!_clients.count(e.window)) {
    LOG(INFO) << "Ignore UnmapNotify for non-client window " << e.window;
    return;
  }

  if (e.event == _root) {
    LOG(INFO) << "Ignore UnmapNotify for reparented pre-existing window " << e.window;
    return;
  }

  Unframe(e.window);

}

void WindowManager::Unframe(Window w) {

  const Window frame = _clients[w];

  XUnmapWindow(_display, frame);

  XReparentWindow(
      _display,
      w,
      _root,
      0, 0);

  XRemoveFromSaveSet(_display, w);

  XDestroyWindow(_display, frame);

  _clients.erase(w);

  LOG(INFO) << "Unframed window " << w << " [" << frame << "]";
}
