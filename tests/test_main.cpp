/**
 * @file test_main.cpp
 *
 * @version 1.0.0
 * @license MIT
 *
 * @summary Entry point that executes every auto-registered unit test case
 *
 * @description
 * Iterates the registry, runs each case, prints a per-suite summary and returns the number of
 * failed expectations (capped at 125) so CI harnesses detect failure through the exit code.
 */
// ---------- IMPORTS
#include "test_framework.h"
#include <cstdio>
// ---------- IMPLEMENTATION
int main() {
    auto& registry = testing::Registry::instance();
    const auto& cases = registry.cases();
    std::printf("Running %zu unit test(s)\n", cases.size());
    for (const auto& test : cases) {
        const int before = registry.failures();
        std::printf("[ RUN  ] %s.%s\n", test.suite.c_str(), test.name.c_str());
        test.fn();
        const int failed = registry.failures() - before;
        if (failed == 0) {
            std::printf("[  OK  ] %s.%s\n", test.suite.c_str(), test.name.c_str());
        } else {
            std::printf("[ FAIL ] %s.%s (%d expectation(s))\n", test.suite.c_str(),
                        test.name.c_str(), failed);
        }
    }
    std::printf("Done: %zu test(s), %d failed expectation(s)\n", cases.size(),
                registry.failures());
    if (registry.failures() != 0) {
        return 1;
    }
    return 0;
}
