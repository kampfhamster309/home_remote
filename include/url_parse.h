#pragma once

#include <cstring>
#include <cstdlib>
#include <cstdint>

// ----------------------------------------------------------------------------
// Minimal URL parser for Home Assistant server URLs.
// Extracts host, port, and security flag from strings like:
//   http://192.168.1.100:8123
//   https://homeassistant.local:8123
//   http://ha.local          (port defaults to 80)
//   https://ha.example.com   (port defaults to 443)
//
// Pure C++ — no Arduino dependencies. Safe to use in native unit tests.
// ----------------------------------------------------------------------------

struct ParsedUrl {
    char     host[128]; // hostname or IP address, null-terminated
    uint16_t port;      // TCP port
    bool     secure;    // true for https (wss), false for http (ws)
    bool     valid;     // false if parsing failed
};

inline ParsedUrl parse_ha_url(const char* url)
{
    ParsedUrl result{};

    if (!url || url[0] == '\0') return result;

    // Identify scheme
    const char* rest;
    if (strncmp(url, "https://", 8) == 0) {
        result.secure = true;
        result.port   = 443;
        rest = url + 8;
    } else if (strncmp(url, "http://", 7) == 0) {
        result.secure = false;
        result.port   = 80;
        rest = url + 7;
    } else {
        return result; // unknown scheme → invalid
    }

    if (rest[0] == '\0') return result; // nothing after scheme → invalid

    // Locate optional path separator
    const char* path_sep = strchr(rest, '/');

    // Locate optional port separator — only valid before the path
    const char* colon = strchr(rest, ':');
    if (colon && path_sep && colon > path_sep) {
        colon = nullptr; // colon is inside the path, not a port separator
    }

    // Extract host length
    size_t host_len;
    if (colon) {
        host_len = static_cast<size_t>(colon - rest);
    } else if (path_sep) {
        host_len = static_cast<size_t>(path_sep - rest);
    } else {
        host_len = strlen(rest);
    }

    if (host_len == 0 || host_len >= sizeof(result.host)) return result;

    memcpy(result.host, rest, host_len);
    result.host[host_len] = '\0';

    // Extract port (if present)
    if (colon) {
        const char* port_start = colon + 1;
        // Port ends at '/' or end of string
        const char* port_end = path_sep ? path_sep : port_start + strlen(port_start);
        size_t port_len = static_cast<size_t>(port_end - port_start);

        if (port_len == 0 || port_len > 5) return result; // invalid port

        char port_str[6] = {};
        memcpy(port_str, port_start, port_len);
        int p = atoi(port_str);
        if (p <= 0 || p > 65535) return result;
        result.port = static_cast<uint16_t>(p);
    }

    result.valid = true;
    return result;
}
