/**
 * @file test_framework.h
 *
 * @version 1.0.0
 * @license MIT
 *
 * @summary Header-only unit test framework with auto-registration and a minimal assert set
 *
 * @description
 * TEST(suite, name) defines and registers a test case; EXPECT_TRUE/FALSE, EXPECT_EQ/NE,
 * EXPECT_NEAR and FAIL record failures against the running case without aborting it, so one run
 * reports every broken expectation. main() from test_main.cpp executes all registered cases and
 * returns a non-zero exit code when any of them fails.
 */
#pragma once
// ---------- IMPORTS
#include <cmath>
#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>
// ---------- TYPES
namespace testing {
struct TestCase {
    std::string suite;
    std::string name;
    std::function<void()> fn;
};

class Registry {
public:
    static Registry& instance() {
        static Registry registry;
        return registry;
    }
    void add(TestCase tc) { m_cases.push_back(std::move(tc)); }
    const std::vector<TestCase>& cases() const noexcept { return m_cases; }
    int failures() const noexcept { return m_failures; }
    void recordFailure(const char* file, int line, const std::string& message) {
        ++m_failures;
        std::printf("    FAIL %s:%d\n      %s\n", file, line, message.c_str());
    }
private:
    std::vector<TestCase> m_cases;
    int m_failures = 0;
};

struct Registrar {
    Registrar(const char* suite, const char* name, std::function<void()> fn) {
        Registry::instance().add({suite, name, std::move(fn)});
    }
};

template <typename T>
std::string describe(const T& value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

inline std::string describe(bool value) { return value ? "true" : "false"; }

inline std::string describe(const std::string& value) {
    return "\"" + value + "\"";
}
}
// ---------- MACROS
#define TEST(suite, name)                                                                                \
    static void test_##suite##_##name();                                                                 \
    static ::testing::Registrar registrar_##suite##_##name(#suite, #name, test_##suite##_##name);        \
    static void test_##suite##_##name()

#define NOVA_CHECK(condString, expectedTrue, expression)                                                 \
    do {                                                                                                 \
        const bool result = static_cast<bool>(expression);                                               \
        if (result != (expectedTrue)) {                                                                  \
            ::testing::Registry::instance().recordFailure(__FILE__, __LINE__,                            \
                                                          std::string(condString) + " -> " +             \
                                                          (::testing::describe(expression)));            \
        }                                                                                                \
    } while (false)

#define EXPECT_TRUE(expr) NOVA_CHECK("EXPECT_TRUE(" #expr ")", true, expr)
#define EXPECT_FALSE(expr) NOVA_CHECK("EXPECT_FALSE(" #expr ")", false, expr)

#define NOVA_COMPARE(opString, cmp, lhs, rhs)                                                            \
    do {                                                                                                 \
        const auto& novaLhs = (lhs);                                                                     \
        const auto& novaRhs = (rhs);                                                                     \
        if (!(novaLhs cmp novaRhs)) {                                                                    \
            ::testing::Registry::instance().recordFailure(                                               \
                __FILE__, __LINE__,                                                                      \
                std::string(opString) + "\n        lhs: " + ::testing::describe(novaLhs) +               \
                    "\n        rhs: " + ::testing::describe(novaRhs));                                   \
        }                                                                                                \
    } while (false)

#define EXPECT_EQ(lhs, rhs) NOVA_COMPARE("EXPECT_EQ(" #lhs ", " #rhs ")", ==, lhs, rhs)
#define EXPECT_NE(lhs, rhs) NOVA_COMPARE("EXPECT_NE(" #lhs ", " #rhs ")", !=, lhs, rhs)
#define EXPECT_LT(lhs, rhs) NOVA_COMPARE("EXPECT_LT(" #lhs ", " #rhs ")", <, lhs, rhs)
#define EXPECT_LE(lhs, rhs) NOVA_COMPARE("EXPECT_LE(" #lhs ", " #rhs ")", <=, lhs, rhs)
#define EXPECT_GT(lhs, rhs) NOVA_COMPARE("EXPECT_GT(" #lhs ", " #rhs ")", >, lhs, rhs)
#define EXPECT_GE(lhs, rhs) NOVA_COMPARE("EXPECT_GE(" #lhs ", " #rhs ")", >=, lhs, rhs)

#define EXPECT_NEAR(lhs, rhs, tolerance)                                                                 \
    do {                                                                                                 \
        const double novaL = static_cast<double>(lhs);                                                   \
        const double novaR = static_cast<double>(rhs);                                                   \
        const double novaT = static_cast<double>(tolerance);                                             \
        if (std::fabs(novaL - novaR) > novaT) {                                                          \
            ::testing::Registry::instance().recordFailure(                                               \
                __FILE__, __LINE__,                                                                      \
                std::string("EXPECT_NEAR(" #lhs ", " #rhs ")") +                                         \
                    "\n        actual: " + ::testing::describe(novaL) +                                  \
                    "\n        expected within " + ::testing::describe(novaT) +                          \
                    " of " + ::testing::describe(novaR));                                                \
        }                                                                                                \
    } while (false)

#define FAIL(message)                                                                                    \
    ::testing::Registry::instance().recordFailure(__FILE__, __LINE__, std::string("FAIL: ") + message)
