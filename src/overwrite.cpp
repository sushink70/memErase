#include "overwrite.h"
#include "utility.h" // for getFreeSpace
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

bool overwriteDevice(const std::string &targetPath,
                     const std::string &tempFileNameBase,
                     size_t iterations,
                     FillMode mode) {
    const size_t blockSize = 1024 * 1024; // 1 MB block

    for (size_t iter = 0; iter < iterations; ++iter) {
        std::cout << "\n=== Iteration " << (iter + 1) << " of " << iterations << " ===\n";

        // Determine the fill character for this iteration.
        char fillChar = '0';
        if (mode == FillMode::ONE) {
            fillChar = '1';
        } else if (mode == FillMode::MIX) {
            // Alternate: even iterations use '0', odd iterations use '1'.
            fillChar = ((iter % 2) == 0) ? '0' : '1';
        }
        std::vector<char> writeBlock(blockSize, fillChar);

        // Create a unique temporary file name for this iteration.
        std::ostringstream oss;
        oss << tempFileNameBase << "_" << iter;
        std::string tempFilePath = (fs::path(targetPath) / oss.str()).string();

        // Open temporary file for binary writing.
        std::ofstream outFile(tempFilePath, std::ios::binary | std::ios::out);
        if (!outFile) {
            std::cerr << "Error: Could not open " << tempFilePath << " for writing." << std::endl;
            return false;
        }

        uintmax_t freeSpace = getFreeSpace(targetPath);
        std::cout << "Free space before writing: " << freeSpace << " bytes" << std::endl;
        size_t blockCount = 0;

        // Write blocks until free space falls below blockSize.
        while (freeSpace > blockSize) {
            outFile.write(writeBlock.data(), blockSize);
            if (!outFile) {
                std::cerr << "\nError writing to " << tempFilePath << std::endl;
                break;
            }
            ++blockCount;

            // Optionally flush periodically.
            if (blockCount % 100 == 0) {
                outFile.flush();
            }
            freeSpace = getFreeSpace(targetPath);
            std::cout << "\rRemaining free space: " << freeSpace << " bytes" << std::flush;
        }
        std::cout << "\nCompleted writing temporary file: " << tempFilePath << std::endl;
        outFile.close();

        // Delete the temporary file.
        std::error_code ec;
        fs::remove(tempFilePath, ec);
        if (ec) {
            std::cerr << "Error deleting temporary file " << tempFilePath 
                      << ": " << ec.message() << std::endl;
        } else {
            std::cout << "Temporary file deleted successfully." << std::endl;
        }

        // Pause between iterations.
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return true;
}
