#pragma once

#include <cstdlib>
#include <cstring>

// ----------------------------------------------------------------------------
// Minimal semantic version parser and comparator.
//
// Parses "MAJOR.MINOR.PATCH" strings.  Pre-release and build metadata
// suffixes (e.g. "-rc1", "+sha") are ignored — only the numeric triple
// is compared.
//
// Usage:
//   bool newer = semver_newer("1.2.0", "1.1.5");  // → true
//   bool older = semver_newer("1.1.5", "1.2.0");  // → false
// ----------------------------------------------------------------------------

struct SemVer {
    int major;
    int minor;
    int patch;
};

// Parse "major.minor.patch" into a SemVer.  Missing components default to 0.
inline SemVer semver_parse(const char* s)
{
    SemVer v{0, 0, 0};
    if (!s || !*s) return v;
    v.major = atoi(s);
    const char* p = strchr(s, '.');
    if (!p) return v;
    v.minor = atoi(p + 1);
    p = strchr(p + 1, '.');
    if (!p) return v;
    v.patch = atoi(p + 1);
    return v;
}

// Returns true if `candidate` is strictly newer than `current`.
inline bool semver_newer(const char* candidate, const char* current)
{
    const SemVer c = semver_parse(candidate);
    const SemVer r = semver_parse(current);
    if (c.major != r.major) return c.major > r.major;
    if (c.minor != r.minor) return c.minor > r.minor;
    return c.patch > r.patch;
}
