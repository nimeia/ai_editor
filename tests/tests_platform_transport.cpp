#include "bridge/platform/runtime.hpp"
#include "bridge/transport/transport.hpp"
#include <cassert>
#include <iostream>

int main() {
  const auto platform = bridge::platform::platform_family();
  const auto transport = bridge::transport::transport_family();
  assert(!platform.empty());
  assert(!transport.empty());
#if defined(_WIN32)
  assert(platform == "windows");
  assert(transport == "windows-named-pipe");
#else
  assert(platform == "posix");
  assert(transport == "posix-unix-socket");
#endif
  std::cout << "platform/transport family ok\n";
  return 0;
}
