#include <cstdlib>
#include <glog/logging.h>
#include "window_manager.cpp"


int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);

  WindowManager window_manager = WindowManager();
  if (window_manager.no_display) {
    LOG(ERROR) << "Failed to initialize window manager.";
    return EXIT_FAILURE;
  }

  window_manager.Run_Check();
  window_manager.Run();

  return EXIT_SUCCESS;
}
