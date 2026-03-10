#include "bridge/core/instance.hpp"
#include "bridge/core/logging.hpp"
#include "bridge/core/protocol.hpp"
#include "bridge/core/workspace.hpp"
#include "bridge/platform/runtime.hpp"
#include "bridge/transport/transport.hpp"
#include "bridge/core/version.hpp"
#include <chrono>
#include <iostream>
#include <mutex>
#include <sstream>
#include <unordered_set>

struct AppContext {
  bridge::core::WorkspaceConfig workspace;
  std::string instance_key;
  std::string endpoint;
  std::string runtime_dir;
  std::mutex cancel_mu;
  std::unordered_set<std::string> cancelled_request_ids;

  void cancel(const std::string& request_id) {
    std::lock_guard<std::mutex> lock(cancel_mu);
    cancelled_request_ids.insert(request_id);
  }

  bool is_cancelled(const std::string& request_id) {
    std::lock_guard<std::mutex> lock(cancel_mu);
    return cancelled_request_ids.find(request_id) != cancelled_request_ids.end();
  }

  void clear(const std::string& request_id) {
    std::lock_guard<std::mutex> lock(cancel_mu);
    cancelled_request_ids.erase(request_id);
  }
};

static void handle_request_adapter(const std::string& request,
                                   bridge::transport::OnFrame emit,
                                   void* emit_ctx,
                                   void* ctx_void) {
  auto* ctx = static_cast<AppContext*>(ctx_void);
  const auto request_id = bridge::core::json_get_string(request, "request_id");
  const auto client_id = bridge::core::json_get_string(request, "client_id");
  const auto session_id = bridge::core::json_get_string(request, "session_id");
  const auto method = bridge::core::json_get_string(request, "method");
  const auto path = bridge::core::json_get_string(request, "path");

  const auto started = std::chrono::steady_clock::now();
  std::size_t request_bytes = request.size();
  std::size_t response_bytes = 0;
  bool ok = true;
  bool truncated = false;
  std::string error_code;
  std::string error_message;

  auto tracked_emit = [&](const std::string& frame) -> bool {
    response_bytes += frame.size();
    if (frame.find("\"type\":\"final\"") != std::string::npos) {
      ok = frame.find("\"ok\":true") != std::string::npos;
      truncated = frame.find("\"truncated\":true") != std::string::npos;
      if (!ok) {
        error_code = bridge::core::json_get_string(frame, "code");
        error_message = bridge::core::json_get_string(frame, "message");
      }
    } else {
      truncated = truncated || frame.find("\"truncated\":true") != std::string::npos;
    }
    return emit(frame, emit_ctx);
  };

  if (method == "request.cancel") {
    const auto target_request_id = bridge::core::json_get_string(request, "target_request_id");
    std::string response;
    if (target_request_id.empty()) {
      response = bridge::core::make_error_response(request_id, "INVALID_PARAMS", "target_request_id is empty");
    } else {
      ctx->cancel(target_request_id);
      std::ostringstream result;
      result << "{" << "\"cancelled\":true," << "\"target_request_id\":\""
             << bridge::core::json_escape(target_request_id) << "\"}";
      response = bridge::core::make_ok_response(request_id, result.str());
    }
    tracked_emit(response);
  } else {
    bridge::core::handle_request_stream(
        request,
        ctx->workspace,
        ctx->instance_key,
        ctx->endpoint,
        ctx->runtime_dir,
        bridge::platform::platform_family(),
        bridge::transport::transport_family(),
        tracked_emit,
        [&](const std::string& active_request_id) { return ctx->is_cancelled(active_request_id); });
    ctx->clear(request_id);
  }

  const auto finished = std::chrono::steady_clock::now();
  const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(finished - started).count();

  std::ostringstream runtime_message;
  runtime_message << "request_id=" << request_id
                  << " method=" << method
                  << " path=" << path
                  << " duration_ms=" << duration_ms
                  << " request_bytes=" << request_bytes
                  << " response_bytes=" << response_bytes
                  << " ok=" << (ok ? "true" : "false")
                  << " truncated=" << (truncated ? "true" : "false");
  if (!ok && !error_code.empty()) runtime_message << " code=" << error_code;
  bridge::core::append_runtime_log(ctx->runtime_dir, ok ? "INFO" : "WARN", runtime_message.str());

  bridge::core::AuditRecord audit;
  audit.timestamp = bridge::core::current_timestamp_utc();
  audit.request_id = request_id;
  audit.client_id = client_id;
  audit.session_id = session_id;
  audit.method = method;
  audit.path = path;
  audit.endpoint = ctx->endpoint;
  audit.duration_ms = duration_ms;
  audit.request_bytes = request_bytes;
  audit.response_bytes = response_bytes;
  audit.ok = ok;
  audit.truncated = truncated;
  audit.error_code = error_code;
  audit.error_message = error_message;
  bridge::core::append_audit_log(ctx->workspace, audit);
}

int main(int argc, char** argv) {
  if (argc >= 2) {
    const std::string first = argv[1];
    if (first == "--version" || first == "version") {
      std::cout << "ai_bridge_daemon " << AI_BRIDGE_VERSION
                << " platform=" << bridge::platform::platform_family()
                << " transport=" << bridge::transport::transport_family() << "\n";
      return 0;
    }
  }

  std::string workspace = ".";
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--workspace" && i + 1 < argc) {
      workspace = argv[++i];
    }
  }

  auto cfg = bridge::core::make_default_workspace_config(workspace);
  const auto normalized_root = bridge::core::normalize_root_path(cfg.root);
  bridge::core::InstanceScope scope{bridge::platform::current_user_id(), normalized_root, cfg.profile_name, cfg.policy_name};
  const auto instance_key = bridge::core::make_instance_key(scope);
  const auto runtime = bridge::platform::make_runtime_paths(instance_key);

  bridge::platform::InstanceLock lock;
  std::string error;
  if (!lock.acquire(runtime.lock_file, &error)) {
    std::cerr << "failed to acquire instance lock: " << error << "\n";
    return 1;
  }

  AppContext ctx{cfg, instance_key, runtime.endpoint, runtime.runtime_dir};
  const auto ack = bridge::core::make_hello_ack(instance_key, normalized_root, cfg.profile_name, runtime.endpoint);
  bridge::core::append_runtime_log(runtime.runtime_dir, "INFO", "daemon started endpoint=" + runtime.endpoint + " workspace=" + normalized_root + " platform=" + bridge::platform::platform_family() + " transport=" + bridge::transport::transport_family());
  std::cout << "bridge_daemon listening on " << runtime.endpoint << " platform=" << bridge::platform::platform_family() << " transport=" << bridge::transport::transport_family() << std::endl;
  const int rc = bridge::transport::run_server_stream(runtime.endpoint, ack, &handle_request_adapter, &ctx, &error);
  if (!error.empty()) {
    bridge::core::append_runtime_log(runtime.runtime_dir, "ERROR", "server stopped error=" + error);
  }
  return rc;
}
