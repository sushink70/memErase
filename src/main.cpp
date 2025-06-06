#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include "utility.h"
#include "overwrite.h"

#ifdef _WIN32
    #include <windows.h>
#endif

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    // Check for minimal arguments.
    if (argc < 5) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    std::string targetPath;
    size_t iterations = 0;
    FillMode fillMode = FillMode::ZERO; // default fill mode

    // Simple command-line argument parsing.
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-d" && (i + 1) < argc) {
            targetPath = argv[++i];
        } else if (arg == "-i" && (i + 1) < argc) {
            try {
                iterations = std::stoul(argv[++i]);
            } catch (const std::exception &ex) {
                std::cerr << "Invalid iteration count: " << ex.what() << std::endl;
                return EXIT_FAILURE;
            }
        } else if (arg == "-p" && (i + 1) < argc) {
            std::string modeStr = argv[++i];
            std::transform(modeStr.begin(), modeStr.end(), modeStr.begin(), ::tolower);
            if (modeStr == "zero") {
                fillMode = FillMode::ZERO;
            } else if (modeStr == "one") {
                fillMode = FillMode::ONE;
            } else if (modeStr == "mix") {
                fillMode = FillMode::MIX;
            } else {
                std::cerr << "Invalid fill mode: " << modeStr << std::endl;
                printUsage(argv[0]);
                return EXIT_FAILURE;
            }
        } else {
            printUsage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    // On Windows, ensure that the target path ends with a backslash.
#ifdef _WIN32
    if (!targetPath.empty() && targetPath.back() != '\\')
        targetPath.push_back('\\');
#endif

    // Verify that the target path exists and is a directory.
    if (!fs::exists(targetPath) || !fs::is_directory(targetPath)) {
        std::cerr << "Error: Target path " << targetPath 
                  << " does not exist or is not a directory." << std::endl;
        return EXIT_FAILURE;
    }

    // Display connected drives for reference.
    std::cout << "Currently available removable drives:" << std::endl;
    std::vector<std::string> drives = listDrives();
    for (const auto &drive : drives) {
        std::cout << "  " << drive << std::endl;
    }

    // Confirm with the user.
    std::stringstream confirmMsg;
    confirmMsg << "\nWARNING: This operation will permanently erase all data on "
               << targetPath << ". Are you sure you want to proceed?";
    if (!confirmAction(confirmMsg.str())) {
        std::cout << "Operation canceled by user." << std::endl;
        return EXIT_SUCCESS;
    }

    // Log the start of the operation.
    std::cout << "\nStarting secure erase on " << targetPath
              << " with " << iterations << " iteration(s) using fill mode: ";
    if (fillMode == FillMode::ZERO)
        std::cout << "ZERO";
    else if (fillMode == FillMode::ONE)
        std::cout << "ONE";
    else if (fillMode == FillMode::MIX)
        std::cout << "MIX";
    std::cout << std::endl;

    // Call the overwrite function.
    if (!overwriteDevice(targetPath, "temp_secure_erase_file", iterations, fillMode)) {
        std::cerr << "Error occurred during the overwrite operation." << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "\nSecure erase completed successfully." << std::endl;
    return EXIT_SUCCESS;
}
//make std
