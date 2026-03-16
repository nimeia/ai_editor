#pragma once

#include <string>
#include "bridge/core/session_service.hpp"

namespace bridge::core {

SessionMutationResult markdown_replace_section(const WorkspaceConfig& workspace,
                                              const std::string& session_id,
                                              const std::string& path,
                                              const std::string& heading,
                                              std::size_t heading_level,
                                              const std::string& new_content);

SessionMutationResult markdown_insert_after_heading(const WorkspaceConfig& workspace,
                                                    const std::string& session_id,
                                                    const std::string& path,
                                                    const std::string& heading,
                                                    std::size_t heading_level,
                                                    const std::string& new_content);

SessionMutationResult markdown_upsert_section(const WorkspaceConfig& workspace,
                                              const std::string& session_id,
                                              const std::string& path,
                                              const std::string& heading,
                                              std::size_t heading_level,
                                              const std::string& new_content);

SessionMutationResult json_replace_value(const WorkspaceConfig& workspace,
                                         const std::string& session_id,
                                         const std::string& path,
                                         const std::string& key_path,
                                         const std::string& new_json_value);

SessionMutationResult json_upsert_key(const WorkspaceConfig& workspace,
                                      const std::string& session_id,
                                      const std::string& path,
                                      const std::string& key_path,
                                      const std::string& new_json_value);

SessionMutationResult json_append_array_item(const WorkspaceConfig& workspace,
                                             const std::string& session_id,
                                             const std::string& path,
                                             const std::string& key_path,
                                             const std::string& new_json_value);

SessionMutationResult yaml_replace_value(const WorkspaceConfig& workspace,
                                         const std::string& session_id,
                                         const std::string& path,
                                         const std::string& key_path,
                                         const std::string& new_yaml_value);

SessionMutationResult yaml_upsert_key(const WorkspaceConfig& workspace,
                                      const std::string& session_id,
                                      const std::string& path,
                                      const std::string& key_path,
                                      const std::string& new_yaml_value);

SessionMutationResult yaml_append_item(const WorkspaceConfig& workspace,
                                       const std::string& session_id,
                                       const std::string& path,
                                       const std::string& key_path,
                                       const std::string& new_yaml_value);

SessionMutationResult html_replace_node(const WorkspaceConfig& workspace,
                                        const std::string& session_id,
                                        const std::string& path,
                                        const std::string& selector,
                                        const std::string& new_html);

SessionMutationResult html_insert_after_node(const WorkspaceConfig& workspace,
                                             const std::string& session_id,
                                             const std::string& path,
                                             const std::string& selector,
                                             const std::string& new_html);

SessionMutationResult html_set_attribute(const WorkspaceConfig& workspace,
                                         const std::string& session_id,
                                         const std::string& path,
                                         const std::string& selector,
                                         const std::string& attribute_name,
                                         const std::string& attribute_value);

} // namespace bridge::core
