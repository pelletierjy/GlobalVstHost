// src/tray-app/updates/update_check.h
//
// T122 — Explicit user-initiated update check.
//
// HTTPS GET against the configured endpoint (from roaming settings).
// Parses JSON response per contracts/update-manifest-schema.json.
// Compares semantic versions; reports result via callback on the worker thread.
// Caller is responsible for dispatching to the UI thread if needed.

#pragma once

#include <functional>
#include <string>

namespace jyglobalvst::tray {

struct UpdateCheckResult
{
    bool success {false};
    bool update_available {false};
    std::string installed_version;
    std::string latest_version;
    std::string release_notes_url;
    std::string download_url;
    std::string error_message;
};

class UpdateCheck
{
public:
    using Callback = std::function<void(UpdateCheckResult)>;

    // Initiates an asynchronous check on a background thread.
    // Callback is invoked from that thread — caller must dispatch to UI thread.
    void check(const std::string& endpoint_url,
               const std::string& current_version,
               Callback callback);

    // Exposed for unit testing (no network).
    static UpdateCheckResult parseResponse(const std::string& body,
                                           const std::string& current_version);

private:
    static int compareSemver(const std::string& a, const std::string& b);
};

}  // namespace jyglobalvst::tray
