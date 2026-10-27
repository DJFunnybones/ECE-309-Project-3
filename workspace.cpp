// src/tools/workspace.cpp — Workspace::resolve (spec §4.4).
//
// A purely lexical check: no exists(), canonical(), create_directories(), ...
// The constructor, root() and check_symlinks() live in workspace_provided.cpp.

#include "tools/workspace.h"

namespace fs = std::filesystem;

bool Workspace::resolve(std::string_view user_path, fs::path* out,
                        std::string* reason) const {
    try {
        // Step 1: empty path.
        if (user_path.empty()) {
            *reason = "empty path";
            return false;
        }
        // Step 2: embedded NUL. The OS would silently cut the path at it.
        if (user_path.find('\0') != std::string_view::npos) {
            *reason = "path contains a NUL byte";
            return false;
        }
        // Step 3: only workspace-relative paths are allowed.
        fs::path p{std::string(user_path)};
        if (p.is_absolute() || p.has_root_path()) {
            *reason = "absolute paths are not allowed: " + std::string(user_path);
            return false;
        }
        // Step 4: fold ".", duplicate slashes and "x/.." pairs.
        fs::path n = p.lexically_normal();
        // Step 5: a leading ".." after normalizing means the path climbs above root.
        if (!n.empty() && *n.begin() == "..") {
            *reason = "path escapes the workspace: " + std::string(user_path);
            return false;
        }
        // Step 6: build the absolute path.
        if (n.empty() || n == ".") {
            *out = root();
        } else {
            *out = root() / n;
        }
        return true;
    } catch (...) {
        *reason = "invalid path";
        return false;
    }
}
