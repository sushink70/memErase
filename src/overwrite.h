#ifndef OVERWRITE_H
#define OVERWRITE_H

#include <string>
#include <cstddef>

// Enumeration for fill modes.
enum class FillMode {
    ZERO,
    ONE,
    MIX
};

// Overwrite the device by writing temporary files filled with the selected pattern
// until free space is exhausted, then deleting them.
// Parameters:
//   targetPath: The path to the device (directory).
//   tempFileNameBase: Base name for temporary files.
//   iterations: Number of overwrite iterations.
//   mode: The fill mode to use.
bool overwriteDevice(const std::string &targetPath,
                     const std::string &tempFileNameBase,
                     size_t iterations,
                     FillMode mode);

#endif // OVERWRITE_H
