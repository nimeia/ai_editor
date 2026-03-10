#include "bridge/transport/transport.hpp"
#ifdef _WIN32
#include <windows.h>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace bridge::transport {
namespace {

static std::string last_error_message(const std::string& prefix) {
  DWORD code = GetLastError();
  LPSTR buffer = nullptr;
  DWORD size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                              nullptr,
                              code,
                              MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                              reinterpret_cast<LPSTR>(&buffer),
                              0,
                              nullptr);
  std::string detail = size && buffer ? std::string(buffer, size) : "unknown error";
  if (buffer) LocalFree(buffer);
  while (!detail.empty() && (detail.back() == '\r' || detail.back() == '\n')) detail.pop_back();
  return prefix + " (code=" + std::to_string(code) + "): " + detail;
}

static bool write_all(HANDLE h, const void* data, DWORD size, std::string* err) {
  const char* p = static_cast<const char*>(data);
  DWORD remaining = size;
  while (remaining > 0) {
    DWORD written = 0;
    if (!WriteFile(h, p, remaining, &written, nullptr)) {
      if (err) *err = last_error_message("WriteFile failed");
      return false;
    }
    if (written == 0) {
      if (err) *err = "WriteFile wrote zero bytes";
      return false;
    }
    p += written;
    remaining -= written;
  }
  return true;
}

static bool read_all(HANDLE h, void* data, DWORD size, std::string* err) {
  char* p = static_cast<char*>(data);
  DWORD remaining = size;
  while (remaining > 0) {
    DWORD read = 0;
    if (!ReadFile(h, p, remaining, &read, nullptr)) {
      if (err) *err = last_error_message("ReadFile failed");
      return false;
    }
    if (read == 0) {
      if (err) *err = "ReadFile read zero bytes";
      return false;
    }
    p += read;
    remaining -= read;
  }
  return true;
}

static bool send_frame(HANDLE h, const std::string& body, std::string* err) {
  DWORD len = static_cast<DWORD>(body.size());
  return write_all(h, &len, sizeof(len), err) && write_all(h, body.data(), len, err);
}

static bool recv_frame(HANDLE h, std::string* body, std::string* err) {
  DWORD len = 0;
  if (!read_all(h, &len, sizeof(len), err)) return false;
  std::vector<char> buf(len);
  if (len > 0 && !read_all(h, buf.data(), len, err)) return false;
  body->assign(buf.begin(), buf.end());
  return true;
}

struct EmitCtx { HANDLE pipe = INVALID_HANDLE_VALUE; };

static bool emit_frame_cb(const std::string& body, void* ctx_void) {
  auto* ctx = static_cast<EmitCtx*>(ctx_void);
  std::string err;
  return send_frame(ctx->pipe, body, &err);
}

static void handle_client(HANDLE pipe,
                          const std::string hello_ack,
                          StreamHandler handle,
                          void* context) {
  std::string first;
  std::string io_error;
  if (recv_frame(pipe, &first, &io_error) && first.find("\"type\":\"hello\"") != std::string::npos) {
    if (send_frame(pipe, hello_ack, &io_error)) {
      std::string req;
      if (recv_frame(pipe, &req, &io_error)) {
        EmitCtx emit{pipe};
        handle(req, &emit_frame_cb, &emit, context);
      }
    }
  }
  FlushFileBuffers(pipe);
  DisconnectNamedPipe(pipe);
  CloseHandle(pipe);
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

std::string transport_family() { return "windows-named-pipe"; }

bool send_request_stream(const std::string& endpoint,
                         const std::string& hello,
                         const std::string& request,
                         OnFrame on_frame,
                         void* on_frame_ctx,
                         std::string* error_message,
                         std::size_t timeout_ms) {
  DWORD wait_ms = timeout_ms == 0 ? NMPWAIT_WAIT_FOREVER : static_cast<DWORD>(timeout_ms);
  if (!WaitNamedPipeA(endpoint.c_str(), wait_ms)) {
    if (error_message) *error_message = last_error_message("WaitNamedPipe failed");
    return false;
  }
  HANDLE pipe = CreateFileA(endpoint.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
  if (pipe == INVALID_HANDLE_VALUE) {
    if (error_message) *error_message = last_error_message("CreateFile failed");
    return false;
  }
  std::string hello_ack;
  bool ok = send_frame(pipe, hello, error_message) && recv_frame(pipe, &hello_ack, error_message);
  if (ok && hello_ack.find("\"type\":\"hello_ack\"") == std::string::npos) {
    if (error_message) *error_message = "invalid hello_ack";
    ok = false;
  }
  if (ok) ok = send_frame(pipe, request, error_message);
  bool seen = false;
  while (ok) {
    std::string frame;
    std::string frame_err;
    if (!recv_frame(pipe, &frame, &frame_err)) break;
    seen = true;
    if (on_frame && !on_frame(frame, on_frame_ctx)) break;
  }
  FlushFileBuffers(pipe);
  CloseHandle(pipe);
  if (!ok || !seen) {
    if (!ok && error_message && error_message->empty()) *error_message = "io failed";
    if (ok && !seen && error_message) *error_message = "io failed";
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
  while (true) {
    HANDLE pipe = CreateNamedPipeA(endpoint.c_str(),
                                   PIPE_ACCESS_DUPLEX,
                                   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT
#if defined(PIPE_REJECT_REMOTE_CLIENTS)
                                   | PIPE_REJECT_REMOTE_CLIENTS
#endif
                                   ,
                                   PIPE_UNLIMITED_INSTANCES,
                                   64 * 1024,
                                   64 * 1024,
                                   0,
                                   nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
      if (error_message) *error_message = last_error_message("CreateNamedPipe failed");
      return 1;
    }

    BOOL connected = ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
    if (!connected) {
      CloseHandle(pipe);
      continue;
    }
    std::thread(handle_client, pipe, hello_ack, handle, context).detach();
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
#else
namespace bridge::transport {
std::string transport_family() { return "windows-named-pipe"; }
bool send_request_stream(const std::string&, const std::string&, const std::string&, OnFrame, void*, std::string* error_message, std::size_t) {
  if (error_message) *error_message = "windows named pipe transport unavailable on this platform";
  return false;
}
bool send_request(const std::string&, const std::string&, const std::string&, std::string*, std::string* error_message, std::size_t) {
  if (error_message) *error_message = "windows named pipe transport unavailable on this platform";
  return false;
}
int run_server_stream(const std::string&, const std::string&, StreamHandler, void*, std::string* error_message) {
  if (error_message) *error_message = "windows named pipe transport unavailable on this platform";
  return 1;
}
int run_server(const std::string&, const std::string&, std::string (*)(const std::string&, void*), void*, std::string* error_message) {
  if (error_message) *error_message = "windows named pipe transport unavailable on this platform";
  return 1;
}
} // namespace bridge::transport
#endif
