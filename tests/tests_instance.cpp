#include "bridge/core/instance.hpp"
#include "bridge/platform/runtime.hpp"
#include <cassert>
#include <string>

int main() {
  bridge::core::InstanceScope a{"1000", "/tmp/ws", "default", "default"};
  bridge::core::InstanceScope b{"1000", "/tmp/ws", "default", "default"};
  bridge::core::InstanceScope c{"1000", "/tmp/ws2", "default", "default"};
  auto ka = bridge::core::make_instance_key(a);
  auto kb = bridge::core::make_instance_key(b);
  auto kc = bridge::core::make_instance_key(c);
  assert(ka == kb);
  assert(ka != kc);
  auto runtime = bridge::platform::make_runtime_paths(ka);
  assert(!runtime.runtime_dir.empty());
  assert(!runtime.endpoint.empty());
  assert(!runtime.lock_file.empty());

  bridge::platform::InstanceLock first;
  std::string err1;
  assert(first.acquire(runtime.lock_file, &err1));

  bridge::platform::InstanceLock second;
  std::string err2;
  assert(!second.acquire(runtime.lock_file, &err2));
  assert(err2 == "instance already locked");

  first.release();
  std::string err3;
  assert(second.acquire(runtime.lock_file, &err3));
  second.release();
  return 0;
}
