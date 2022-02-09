extern "C" {
#include <X11/Xlib.h>
}
#include <memory>
#include <cstdlib>
#include <glog/logging.h>

// naming convention OnEventZ is used for function on taking action on event EventZ

class WindowManager{
  private:
    Display* _display;
    const Window _root;

    static bool _another_wm_detected;
    static int OnAnotherWMDetected(Display* display, XErrorEvent* e);
    static int OnXError(Display* display, XErrorEvent* e);

  public:

    bool no_display;
    WindowManager();
    ~WindowManager();
    // Run_Check checks if another window manager is already running
    void Run_Check();
    void Run();

    void OnCreateNotify(const XCreateWindowEvent& e);
    void OnDestroyNotify(const XDestroyWindowEvent& e);
    void OnReparentNotify(const XReparentEvent& e);
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

  for (;;) {
    XEvent e;
    XNextEvent(_display, &e);
    LOG(INFO) << "Received event: " << e.type;

    switch (e.type) {
      case CreateNotify:
        OnCreateNotify(e.xcreatewindow);
        break;
      case DestroyNotify:
        OnDestroyNotify(e.xdestroywindow);
        break;
      case ReparentNotify:
        OnReparentNotify(e.xreparent);
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


void WindowManager::OnCreateNotify(const XCreateWindowEvent& e) {}
void WindowManager::OnDestroyNotify(const XDestroyWindowEvent& e) {}
void WindowManager::OnReparentNotify(const XReparentEvent& e) {}
