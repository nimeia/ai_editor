#include "bridge/core/error_codes.hpp"
#include <cassert>
#include <string>

int main() {
  using bridge::core::classify_common_error;
  using bridge::core::classify_patch_error;
  using bridge::core::classify_search_error;

  {
    const auto err = classify_common_error("path is outside workspace");
    assert(err.code == "PATH_OUTSIDE_WORKSPACE");
    assert(err.message == "path is outside workspace");
  }
  {
    const auto err = classify_common_error("path denied by policy");
    assert(err.code == "DENIED_BY_POLICY");
  }
  {
    const auto err = classify_common_error("path not found");
    assert(err.code == "FILE_NOT_FOUND");
  }
  {
    const auto err = classify_common_error("binary file");
    assert(err.code == "BINARY_FILE");
  }
  {
    const auto err = classify_common_error("request timeout");
    assert(err.code == "REQUEST_TIMEOUT");
  }
  {
    const auto err = classify_common_error("invalid line range");
    assert(err.code == "INVALID_PARAMS");
  }
  {
    const auto err = classify_common_error("path is not a directory");
    assert(err.code == "INVALID_PARAMS");
  }
  {
    const auto err = classify_common_error("failed to open file: /tmp/secret.txt");
    assert(err.code == "ACCESS_DENIED");
  }
  {
    const auto err = classify_common_error("some unknown error", "FS_FALLBACK");
    assert(err.code == "FS_FALLBACK");
  }

  {
    const auto err = classify_search_error("request cancelled", false, true);
    assert(err.code == "REQUEST_CANCELLED");
  }
  {
    const auto err = classify_search_error("search timeout", true, false);
    assert(err.code == "SEARCH_TIMEOUT");
  }
  {
    const auto err = classify_search_error("Unexpected character within '[...]' in regular expression", false, false);
    assert(err.code == "INVALID_PARAMS");
  }
  {
    const auto err = classify_search_error("regular expression bracket mismatch", false, false);
    assert(err.code == "INVALID_PARAMS");
  }
  {
    const auto err = classify_search_error("path not found", false, false);
    assert(err.code == "FILE_NOT_FOUND");
  }
  {
    const auto err = classify_search_error("custom search backend failure", false, false);
    assert(err.code == "SEARCH_FAILED");
  }

  {
    const auto err = classify_patch_error("request timeout");
    assert(err.code == "REQUEST_TIMEOUT");
  }
  {
    const auto err = classify_patch_error("patch conflict: base hash mismatch");
    assert(err.code == "PATCH_CONFLICT");
  }
  {
    const auto err = classify_patch_error("backup not found");
    assert(err.code == "ROLLBACK_NOT_FOUND");
  }
  {
    const auto err = classify_patch_error("preview not found");
    assert(err.code == "PREVIEW_NOT_FOUND");
  }
  {
    const auto err = classify_patch_error("preview expired");
    assert(err.code == "PREVIEW_EXPIRED");
  }
  {
    const auto err = classify_patch_error("preview already applied");
    assert(err.code == "PREVIEW_CONSUMED");
  }
  {
    const auto err = classify_patch_error("preview metadata corrupted");
    assert(err.code == "PREVIEW_INVALID");
  }
  {
    const auto err = classify_patch_error("preview path mismatch");
    assert(err.code == "PREVIEW_MISMATCH");
  }
  {
    const auto err = classify_patch_error("path denied by policy");
    assert(err.code == "DENIED_BY_POLICY");
  }
  {
    const auto err = classify_patch_error("some other patch failure");
    assert(err.code == "PATCH_APPLY_FAILED");
  }

  return 0;
}
