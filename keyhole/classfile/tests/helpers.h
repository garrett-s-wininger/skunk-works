#ifndef HELPERS_H
#define HELPERS_H

#include "gmock/gmock.h"

MATCHER_P(EqualsBinary, expected, "Binary elements are equal in size and value") {
    if (arg.size() != expected.size()) {
        *result_listener << "Size of spans did not match: "
            << expected.size() << " expected, " << arg.size() << " actual";

        return false;
    }

    for (auto i = 0uz; i < expected.size(); ++i) {
        if (expected[i] != arg[i]) {
            *result_listener << "Encountered mismatch at byte index " << i
                << ", expected " 
                << std::format("0x{:02X}", std::to_integer<unsigned>(expected[i]))
                << " but got "
                << std::format("0x{:02X}", std::to_integer<unsigned>(arg[i]));

            return false;
        }
    }

    return true;
}

#endif // HELPERS_H
