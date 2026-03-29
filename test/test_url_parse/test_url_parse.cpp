#include <unity.h>
#include "url_parse.h"

// ----------------------------------------------------------------------------
// Valid HTTP URLs
// ----------------------------------------------------------------------------

void test_http_default_port()
{
    ParsedUrl r = parse_ha_url("http://192.168.1.100");
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_FALSE(r.secure);
    TEST_ASSERT_EQUAL_UINT16(80, r.port);
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", r.host);
}

void test_http_explicit_port()
{
    ParsedUrl r = parse_ha_url("http://192.168.1.100:8123");
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_FALSE(r.secure);
    TEST_ASSERT_EQUAL_UINT16(8123, r.port);
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", r.host);
}

void test_http_hostname()
{
    ParsedUrl r = parse_ha_url("http://homeassistant.local:8123");
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_FALSE(r.secure);
    TEST_ASSERT_EQUAL_UINT16(8123, r.port);
    TEST_ASSERT_EQUAL_STRING("homeassistant.local", r.host);
}

void test_http_with_path()
{
    ParsedUrl r = parse_ha_url("http://ha.local:8123/");
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_UINT16(8123, r.port);
    TEST_ASSERT_EQUAL_STRING("ha.local", r.host);
}

void test_http_with_long_path()
{
    ParsedUrl r = parse_ha_url("http://ha.local:8123/api/states");
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_UINT16(8123, r.port);
    TEST_ASSERT_EQUAL_STRING("ha.local", r.host);
}

// ----------------------------------------------------------------------------
// Valid HTTPS URLs
// ----------------------------------------------------------------------------

void test_https_default_port()
{
    ParsedUrl r = parse_ha_url("https://ha.example.com");
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_TRUE(r.secure);
    TEST_ASSERT_EQUAL_UINT16(443, r.port);
    TEST_ASSERT_EQUAL_STRING("ha.example.com", r.host);
}

void test_https_explicit_port()
{
    ParsedUrl r = parse_ha_url("https://ha.example.com:8443");
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_TRUE(r.secure);
    TEST_ASSERT_EQUAL_UINT16(8443, r.port);
    TEST_ASSERT_EQUAL_STRING("ha.example.com", r.host);
}

void test_https_port_1()
{
    ParsedUrl r = parse_ha_url("https://10.0.0.1:1");
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_UINT16(1, r.port);
}

void test_https_port_65535()
{
    ParsedUrl r = parse_ha_url("https://10.0.0.1:65535");
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_UINT16(65535, r.port);
}

// ----------------------------------------------------------------------------
// Edge cases — still valid
// ----------------------------------------------------------------------------

void test_http_no_port_no_path()
{
    ParsedUrl r = parse_ha_url("http://ha.local");
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_UINT16(80, r.port);
    TEST_ASSERT_EQUAL_STRING("ha.local", r.host);
}

// ----------------------------------------------------------------------------
// Invalid inputs
// ----------------------------------------------------------------------------

void test_null_url()
{
    ParsedUrl r = parse_ha_url(nullptr);
    TEST_ASSERT_FALSE(r.valid);
}

void test_empty_url()
{
    ParsedUrl r = parse_ha_url("");
    TEST_ASSERT_FALSE(r.valid);
}

void test_unknown_scheme()
{
    ParsedUrl r = parse_ha_url("ftp://192.168.1.1:21");
    TEST_ASSERT_FALSE(r.valid);
}

void test_no_scheme()
{
    ParsedUrl r = parse_ha_url("192.168.1.100:8123");
    TEST_ASSERT_FALSE(r.valid);
}

void test_http_no_host()
{
    ParsedUrl r = parse_ha_url("http://");
    TEST_ASSERT_FALSE(r.valid);
}

void test_https_no_host()
{
    ParsedUrl r = parse_ha_url("https://");
    TEST_ASSERT_FALSE(r.valid);
}

void test_port_zero()
{
    ParsedUrl r = parse_ha_url("http://ha.local:0");
    TEST_ASSERT_FALSE(r.valid);
}

void test_port_too_large()
{
    ParsedUrl r = parse_ha_url("http://ha.local:65536");
    TEST_ASSERT_FALSE(r.valid);
}

void test_port_empty()
{
    // "http://ha.local:" — colon with no digits
    ParsedUrl r = parse_ha_url("http://ha.local:");
    TEST_ASSERT_FALSE(r.valid);
}

void test_host_too_long()
{
    // 128 chars of host — one over the buffer limit (127 usable + null)
    const char* url =
        "http://"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"  // 128 a's
        ":8123";
    ParsedUrl r = parse_ha_url(url);
    TEST_ASSERT_FALSE(r.valid);
}

// ----------------------------------------------------------------------------
// Runner
// ----------------------------------------------------------------------------

int main(int /*argc*/, char** /*argv*/)
{
    UNITY_BEGIN();

    RUN_TEST(test_http_default_port);
    RUN_TEST(test_http_explicit_port);
    RUN_TEST(test_http_hostname);
    RUN_TEST(test_http_with_path);
    RUN_TEST(test_http_with_long_path);
    RUN_TEST(test_https_default_port);
    RUN_TEST(test_https_explicit_port);
    RUN_TEST(test_https_port_1);
    RUN_TEST(test_https_port_65535);
    RUN_TEST(test_http_no_port_no_path);
    RUN_TEST(test_null_url);
    RUN_TEST(test_empty_url);
    RUN_TEST(test_unknown_scheme);
    RUN_TEST(test_no_scheme);
    RUN_TEST(test_http_no_host);
    RUN_TEST(test_https_no_host);
    RUN_TEST(test_port_zero);
    RUN_TEST(test_port_too_large);
    RUN_TEST(test_port_empty);
    RUN_TEST(test_host_too_long);

    return UNITY_END();
}
