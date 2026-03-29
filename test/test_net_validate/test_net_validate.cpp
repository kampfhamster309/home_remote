#include <unity.h>
#include "net_validate.h"

// ============================================================================
// net_ssid_valid
// ============================================================================

void test_ssid_valid_typical()
{
    TEST_ASSERT_TRUE(net_ssid_valid("MyNetwork"));
}

void test_ssid_valid_single_char()
{
    TEST_ASSERT_TRUE(net_ssid_valid("X"));
}

void test_ssid_valid_exactly_32_chars()
{
    TEST_ASSERT_TRUE(net_ssid_valid("12345678901234567890123456789012"));  // 32 chars
}

void test_ssid_invalid_empty()
{
    TEST_ASSERT_FALSE(net_ssid_valid(""));
}

void test_ssid_invalid_null()
{
    TEST_ASSERT_FALSE(net_ssid_valid(nullptr));
}

void test_ssid_invalid_33_chars()
{
    TEST_ASSERT_FALSE(net_ssid_valid("123456789012345678901234567890123"));  // 33 chars
}

void test_ssid_valid_with_spaces()
{
    TEST_ASSERT_TRUE(net_ssid_valid("My Home Network"));
}

// ============================================================================
// net_url_valid
// ============================================================================

void test_url_valid_http_ip()
{
    TEST_ASSERT_TRUE(net_url_valid("http://192.168.1.100:8123"));
}

void test_url_valid_https_ip()
{
    TEST_ASSERT_TRUE(net_url_valid("https://192.168.1.100:8123"));
}

void test_url_valid_http_hostname()
{
    TEST_ASSERT_TRUE(net_url_valid("http://homeassistant.local:8123"));
}

void test_url_valid_https_hostname()
{
    TEST_ASSERT_TRUE(net_url_valid("https://homeassistant.example.com"));
}

void test_url_invalid_empty()
{
    TEST_ASSERT_FALSE(net_url_valid(""));
}

void test_url_invalid_null()
{
    TEST_ASSERT_FALSE(net_url_valid(nullptr));
}

void test_url_invalid_no_scheme()
{
    TEST_ASSERT_FALSE(net_url_valid("192.168.1.100:8123"));
}

void test_url_invalid_ftp_scheme()
{
    TEST_ASSERT_FALSE(net_url_valid("ftp://192.168.1.100"));
}

void test_url_invalid_http_no_host()
{
    // "http://" with nothing after — host is empty
    TEST_ASSERT_FALSE(net_url_valid("http://"));
}

void test_url_invalid_https_no_host()
{
    TEST_ASSERT_FALSE(net_url_valid("https://"));
}

void test_url_invalid_partial_scheme()
{
    TEST_ASSERT_FALSE(net_url_valid("http:/192.168.1.100"));  // missing one slash
}

// ============================================================================
// net_token_valid
// ============================================================================

void test_token_valid_typical_jwt()
{
    // Realistic HA long-lived token length
    TEST_ASSERT_TRUE(net_token_valid(
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"
        ".eyJpc3MiOiJhYmNkZWZnaGlqazEyMzQ1Njc4OTAiLCJpYXQiOjE2MDAwMDAwMDB9"
        ".SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c"));
}

void test_token_valid_exactly_20_chars()
{
    TEST_ASSERT_TRUE(net_token_valid("12345678901234567890"));  // 20 chars
}

void test_token_invalid_empty()
{
    TEST_ASSERT_FALSE(net_token_valid(""));
}

void test_token_invalid_null()
{
    TEST_ASSERT_FALSE(net_token_valid(nullptr));
}

void test_token_invalid_too_short()
{
    TEST_ASSERT_FALSE(net_token_valid("shorttoken"));  // 10 chars
}

void test_token_invalid_19_chars()
{
    TEST_ASSERT_FALSE(net_token_valid("1234567890123456789"));  // 19 chars
}

// ============================================================================
// net_password_valid — always true
// ============================================================================

void test_password_valid_empty()
{
    TEST_ASSERT_TRUE(net_password_valid(""));
}

void test_password_valid_null()
{
    TEST_ASSERT_TRUE(net_password_valid(nullptr));
}

void test_password_valid_typical()
{
    TEST_ASSERT_TRUE(net_password_valid("MySecurePassword123!"));
}

// ============================================================================
// Runner
// ============================================================================

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(test_ssid_valid_typical);
    RUN_TEST(test_ssid_valid_single_char);
    RUN_TEST(test_ssid_valid_exactly_32_chars);
    RUN_TEST(test_ssid_invalid_empty);
    RUN_TEST(test_ssid_invalid_null);
    RUN_TEST(test_ssid_invalid_33_chars);
    RUN_TEST(test_ssid_valid_with_spaces);

    RUN_TEST(test_url_valid_http_ip);
    RUN_TEST(test_url_valid_https_ip);
    RUN_TEST(test_url_valid_http_hostname);
    RUN_TEST(test_url_valid_https_hostname);
    RUN_TEST(test_url_invalid_empty);
    RUN_TEST(test_url_invalid_null);
    RUN_TEST(test_url_invalid_no_scheme);
    RUN_TEST(test_url_invalid_ftp_scheme);
    RUN_TEST(test_url_invalid_http_no_host);
    RUN_TEST(test_url_invalid_https_no_host);
    RUN_TEST(test_url_invalid_partial_scheme);

    RUN_TEST(test_token_valid_typical_jwt);
    RUN_TEST(test_token_valid_exactly_20_chars);
    RUN_TEST(test_token_invalid_empty);
    RUN_TEST(test_token_invalid_null);
    RUN_TEST(test_token_invalid_too_short);
    RUN_TEST(test_token_invalid_19_chars);

    RUN_TEST(test_password_valid_empty);
    RUN_TEST(test_password_valid_null);
    RUN_TEST(test_password_valid_typical);

    return UNITY_END();
}
