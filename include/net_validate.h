#pragma once

#include <cstring>

// ----------------------------------------------------------------------------
// Network configuration validators.
// Pure C++ — no Arduino dependencies. Safe to use in native unit tests.
// ----------------------------------------------------------------------------

// SSID: 1–32 characters, non-empty.
inline bool net_ssid_valid(const char* ssid)
{
    if (!ssid || ssid[0] == '\0') return false;
    return strlen(ssid) <= 32;
}

// URL: must begin with "http://" or "https://" and have at least one char after.
inline bool net_url_valid(const char* url)
{
    if (!url || url[0] == '\0') return false;
    const bool http  = strncmp(url, "http://",  7) == 0 && url[7]  != '\0';
    const bool https = strncmp(url, "https://", 8) == 0 && url[8] != '\0';
    return http || https;
}

// Token: non-empty, minimum 20 characters (HA JWTs are 150-250+ chars;
// 20 is a sanity lower bound that rejects obvious paste mistakes).
inline bool net_token_valid(const char* token)
{
    if (!token || token[0] == '\0') return false;
    return strlen(token) >= 20;
}

// Password: always valid — empty string = open network, long = WPA2.
inline bool net_password_valid(const char* /*password*/)
{
    return true;
}
