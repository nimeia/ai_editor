#include "bridge/transport/transport.hpp"
#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <thread>
#include <signal.h>
#include <unistd.h>
#include <utility>

namespace bridge::transport {
namespace {

static void apply_timeouts(int fd, std::size_t timeout_ms) {
  if (timeout_ms == 0) return;
  timeval tv{};
  tv.tv_sec = static_cast<long>(timeout_ms / 1000);
  tv.tv_usec = static_cast<long>((timeout_ms % 1000) * 1000);
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

static bool write_all(int fd, const void* data, size_t size) {
  const char* p = static_cast<const char*>(data);
  while (size > 0) {
#if defined(MSG_NOSIGNAL)
    const ssize_t n = ::send(fd, p, size, MSG_NOSIGNAL);
#else
    const ssize_t n = ::send(fd, p, size, 0);
#endif
    if (n <= 0) return false;
    p += n;
    size -= static_cast<size_t>(n);
  }
  return true;
}

static bool read_all(int fd, void* data, size_t size) {
  char* p = static_cast<char*>(data);
  while (size > 0) {
    ssize_t n = ::read(fd, p, size);
    if (n <= 0) return false;
    p += n;
    size -= static_cast<size_t>(n);
  }
  return true;
}

static bool send_frame(int fd, const std::string& body) {
  uint32_t len = htonl(static_cast<uint32_t>(body.size()));
  return write_all(fd, &len, sizeof(len)) && write_all(fd, body.data(), body.size());
}

static bool recv_frame(int fd, std::string* body) {
  uint32_t len_be = 0;
  if (!read_all(fd, &len_be, sizeof(len_be))) return false;
  uint32_t len = ntohl(len_be);
  std::string tmp(len, '\0');
  if (len > 0 && !read_all(fd, tmp.data(), len)) return false;
  *body = std::move(tmp);
  return true;
}

struct EmitCtx { int fd = -1; };

static bool emit_frame_cb(const std::string& body, void* ctx_void) {
  auto* ctx = static_cast<EmitCtx*>(ctx_void);
  return send_frame(ctx->fd, body);
}

static void handle_client(int fd,
                          const std::string hello_ack,
                          StreamHandler handle,
                          void* context) {
  std::string first;
  if (!recv_frame(fd, &first)) {
    ::close(fd);
    return;
  }
  if (first.find("\"type\":\"hello\"") != std::string::npos) {
    if (send_frame(fd, hello_ack)) {
      std::string req;
      if (recv_frame(fd, &req)) {
        EmitCtx emit{fd};
        handle(req, &emit_frame_cb, &emit, context);
      }
    }
  }
  ::close(fd);
}

struct CollectCtx {
  std::string* response = nullptr;
  bool seen = false;
};

static bool collect_last_frame(const std::string& frame, void* ctx_void) {
  auto* ctx = static_cast<CollectCtx*>(ctx_void);
  if (ctx->response) *ctx->response = frame;
  ctx->seen = true;
  return true;
}

static void single_response_adapter(const std::string& request,
                                    OnFrame emit,
                                    void* emit_ctx,
                                    void* handler_ctx) {
  auto* fn = static_cast<std::pair<std::string (*)(const std::string&, void*), void*>*>(handler_ctx);
  const auto response = fn->first(request, fn->second);
  emit(response, emit_ctx);
}

} // namespace

std::string transport_family() { return "posix-unix-socket"; }

bool send_request_stream(const std::string& endpoint,
                         const std::string& hello,
                         const std::string& request,
                         OnFrame on_frame,
                         void* on_frame_ctx,
                         std::string* error_message,
                         std::size_t timeout_ms) {
  int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    if (error_message) *error_message = "socket create failed";
    return false;
  }
  apply_timeouts(fd, timeout_ms);
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", endpoint.c_str());
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    if (error_message) *error_message = timeout_ms > 0 ? "connect failed or timed out" : "connect failed";
    ::close(fd);
    return false;
  }
  std::string hello_ack;
  if (!send_frame(fd, hello) || !recv_frame(fd, &hello_ack)) {
    if (error_message) *error_message = timeout_ms > 0 ? "hello failed or timed out" : "hello failed";
    ::close(fd);
    return false;
  }
  if (!send_frame(fd, request)) {
    if (error_message) *error_message = timeout_ms > 0 ? "request write failed or timed out" : "request write failed";
    ::close(fd);
    return false;
  }
  bool seen = false;
  while (true) {
    std::string frame;
    if (!recv_frame(fd, &frame)) break;
    seen = true;
    if (on_frame && !on_frame(frame, on_frame_ctx)) break;
  }
  ::close(fd);
  if (!seen) {
    if (error_message) *error_message = timeout_ms > 0 ? "io failed or timed out" : "io failed";
    return false;
  }
  return true;
}

bool send_request(const std::string& endpoint,
                  const std::string& hello,
                  const std::string& request,
                  std::string* response,
                  std::string* error_message,
                  std::size_t timeout_ms) {
  CollectCtx ctx{response, false};
  if (!send_request_stream(endpoint, hello, request, &collect_last_frame, &ctx, error_message, timeout_ms)) return false;
  return ctx.seen;
}

int run_server_stream(const std::string& endpoint,
                      const std::string& hello_ack,
                      StreamHandler handle,
                      void* context,
                      std::string* error_message) {
  ::unlink(endpoint.c_str());
  int listen_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    if (error_message) *error_message = "socket create failed";
    return 1;
  }
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", endpoint.c_str());
  if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    if (error_message) *error_message = "bind failed";
    ::close(listen_fd);
    return 1;
  }
  if (::listen(listen_fd, 16) != 0) {
    if (error_message) *error_message = "listen failed";
    ::close(listen_fd);
    return 1;
  }
  while (true) {
    int fd = ::accept(listen_fd, nullptr, nullptr);
    if (fd < 0) continue;
    std::thread(handle_client, fd, hello_ack, handle, context).detach();
  }
  return 0;
}

int run_server(const std::string& endpoint,
               const std::string& hello_ack,
               std::string (*handle)(const std::string&, void*),
               void* context,
               std::string* error_message) {
  auto ctx = std::make_pair(handle, context);
  return run_server_stream(endpoint, hello_ack, &single_response_adapter, &ctx, error_message);
}

} // namespace bridge::transport
