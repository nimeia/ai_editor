#pragma once
#include <cstddef>
#include <string>

namespace bridge::transport {

using OnFrame = bool (*)(const std::string&, void*);
using StreamHandler = void (*)(const std::string&, OnFrame, void*, void*);

std::string transport_family();

bool send_request(const std::string& endpoint, const std::string& hello, const std::string& request, std::string* response, std::string* error_message, std::size_t timeout_ms = 0);
bool send_request_stream(const std::string& endpoint,
                         const std::string& hello,
                         const std::string& request,
                         OnFrame on_frame,
                         void* on_frame_ctx,
                         std::string* error_message,
                         std::size_t timeout_ms = 0);
int run_server(const std::string& endpoint,
               const std::string& hello_ack,
               std::string (*handle)(const std::string&, void*),
               void* context,
               std::string* error_message);
int run_server_stream(const std::string& endpoint,
                      const std::string& hello_ack,
                      StreamHandler handle,
                      void* context,
                      std::string* error_message);

} // namespace bridge::transport
