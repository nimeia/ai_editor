#pragma once
#include <functional>
#include <vector>
#include <string>
#include "bridge/core/path_policy.hpp"
#include "bridge/core/workspace.hpp"

namespace bridge::core {

using StreamEmit = std::function<bool(const std::string&)>;

std::string json_escape(const std::string& value);
std::string json_get_string(const std::string& text, const std::string& key);

std::string make_hello_request(const std::string& instance_key,
                               const std::string& workspace_root,
                               const std::string& profile,
                               const std::string& client_id);

std::string make_hello_ack(const std::string& instance_key,
                           const std::string& workspace_root,
                           const std::string& profile,
                           const std::string& endpoint);

std::string make_request(const std::string& instance_key,
                         const std::string& client_id,
                         const std::string& session_id,
                         const std::string& request_id,
                         const std::string& method,
                         const std::string& workspace_root,
                         const std::string& path = "");

std::string make_ok_response(const std::string& request_id, const std::string& result_json);
std::string make_error_response(const std::string& request_id, const std::string& code, const std::string& message);

std::string handle_request(const std::string& request_json,
                           const WorkspaceConfig& workspace,
                           const std::string& instance_key,
                           const std::string& endpoint,
                           const std::string& runtime_dir,
                           const std::string& platform,
                           const std::string& transport,
                           const std::function<bool(const std::string&)>& is_cancelled = {});

void handle_request_stream(const std::string& request_json,
                           const WorkspaceConfig& workspace,
                           const std::string& instance_key,
                           const std::string& endpoint,
                           const std::string& runtime_dir,
                           const std::string& platform,
                           const std::string& transport,
                           const StreamEmit& emit,
                           const std::function<bool(const std::string&)>& is_cancelled = {});

} // namespace bridge::core
