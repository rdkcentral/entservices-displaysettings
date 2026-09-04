#pragma once

#include <string>

namespace WPEFramework {
namespace Plugin {

    class ConflictTestFile
    {
    public:
        ConflictTestFile() = default;
        ~ConflictTestFile() = default;

        int getValue() const
        {
            return 42;
        }

        std::string getLabel() const
        {
            return "original-label";
        }
    };

} // namespace Plugin
} // namespace WPEFramework
