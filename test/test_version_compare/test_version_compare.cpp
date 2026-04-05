// Tests for the semver_parse() and semver_newer() functions in include/semver.h.
// Pure C++ — no Arduino or hardware dependencies.

#include "semver.h"

#include <unity.h>

void setUp()    {}
void tearDown() {}

// ----------------------------------------------------------------------------
// semver_parse
// ----------------------------------------------------------------------------

void test_parse_simple()
{
    const SemVer v = semver_parse("1.2.3");
    TEST_ASSERT_EQUAL(1, v.major);
    TEST_ASSERT_EQUAL(2, v.minor);
    TEST_ASSERT_EQUAL(3, v.patch);
}

void test_parse_zeros()
{
    const SemVer v = semver_parse("0.0.0");
    TEST_ASSERT_EQUAL(0, v.major);
    TEST_ASSERT_EQUAL(0, v.minor);
    TEST_ASSERT_EQUAL(0, v.patch);
}

void test_parse_large_numbers()
{
    const SemVer v = semver_parse("10.20.30");
    TEST_ASSERT_EQUAL(10, v.major);
    TEST_ASSERT_EQUAL(20, v.minor);
    TEST_ASSERT_EQUAL(30, v.patch);
}

void test_parse_missing_minor_and_patch()
{
    // Only major present — minor and patch default to 0
    const SemVer v = semver_parse("3");
    TEST_ASSERT_EQUAL(3, v.major);
    TEST_ASSERT_EQUAL(0, v.minor);
    TEST_ASSERT_EQUAL(0, v.patch);
}

void test_parse_missing_patch()
{
    const SemVer v = semver_parse("2.5");
    TEST_ASSERT_EQUAL(2, v.major);
    TEST_ASSERT_EQUAL(5, v.minor);
    TEST_ASSERT_EQUAL(0, v.patch);
}

void test_parse_null_returns_zeros()
{
    const SemVer v = semver_parse(nullptr);
    TEST_ASSERT_EQUAL(0, v.major);
    TEST_ASSERT_EQUAL(0, v.minor);
    TEST_ASSERT_EQUAL(0, v.patch);
}

void test_parse_empty_string_returns_zeros()
{
    const SemVer v = semver_parse("");
    TEST_ASSERT_EQUAL(0, v.major);
    TEST_ASSERT_EQUAL(0, v.minor);
    TEST_ASSERT_EQUAL(0, v.patch);
}

// ----------------------------------------------------------------------------
// semver_newer — strictly newer
// ----------------------------------------------------------------------------

void test_newer_major_bump()
{
    TEST_ASSERT_TRUE(semver_newer("2.0.0", "1.9.9"));
}

void test_newer_minor_bump()
{
    TEST_ASSERT_TRUE(semver_newer("1.2.0", "1.1.9"));
}

void test_newer_patch_bump()
{
    TEST_ASSERT_TRUE(semver_newer("1.0.1", "1.0.0"));
}

void test_not_newer_when_equal()
{
    TEST_ASSERT_FALSE(semver_newer("1.2.3", "1.2.3"));
}

void test_not_newer_when_older_major()
{
    TEST_ASSERT_FALSE(semver_newer("1.9.9", "2.0.0"));
}

void test_not_newer_when_older_minor()
{
    TEST_ASSERT_FALSE(semver_newer("1.1.9", "1.2.0"));
}

void test_not_newer_when_older_patch()
{
    TEST_ASSERT_FALSE(semver_newer("1.0.0", "1.0.1"));
}

void test_newer_major_beats_minor()
{
    // Major bump always wins regardless of minor/patch
    TEST_ASSERT_TRUE(semver_newer("2.0.0", "1.99.99"));
}

void test_not_newer_same_major_lower_minor()
{
    TEST_ASSERT_FALSE(semver_newer("1.0.5", "1.1.0"));
}

void test_newer_zero_to_one()
{
    TEST_ASSERT_TRUE(semver_newer("0.0.1", "0.0.0"));
}

void test_not_newer_both_zero()
{
    TEST_ASSERT_FALSE(semver_newer("0.0.0", "0.0.0"));
}

// ----------------------------------------------------------------------------
// Entry point
// ----------------------------------------------------------------------------

int main()
{
    UNITY_BEGIN();

    // Parse tests
    RUN_TEST(test_parse_simple);
    RUN_TEST(test_parse_zeros);
    RUN_TEST(test_parse_large_numbers);
    RUN_TEST(test_parse_missing_minor_and_patch);
    RUN_TEST(test_parse_missing_patch);
    RUN_TEST(test_parse_null_returns_zeros);
    RUN_TEST(test_parse_empty_string_returns_zeros);

    // Newer comparison tests
    RUN_TEST(test_newer_major_bump);
    RUN_TEST(test_newer_minor_bump);
    RUN_TEST(test_newer_patch_bump);
    RUN_TEST(test_not_newer_when_equal);
    RUN_TEST(test_not_newer_when_older_major);
    RUN_TEST(test_not_newer_when_older_minor);
    RUN_TEST(test_not_newer_when_older_patch);
    RUN_TEST(test_newer_major_beats_minor);
    RUN_TEST(test_not_newer_same_major_lower_minor);
    RUN_TEST(test_newer_zero_to_one);
    RUN_TEST(test_not_newer_both_zero);

    return UNITY_END();
}
