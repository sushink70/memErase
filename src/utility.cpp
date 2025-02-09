#include "utility.h"
#include <iostream>
#include <sstream>
#include <system_error>
#include <algorithm>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <pwd.h>
#endif

namespace fs = std::filesystem;

std::string getCurrentUser() {
#ifdef _WIN32
    char username[256];
    DWORD username_len = 256;
    if (GetUserNameA(username, &username_len))
        return std::string(username);
    return "UnknownUser";
#else
    if (auto* pw = getpwuid(getuid()))
        return std::string(pw->pw_name);
    return "";
#endif
}

std::vector<std::string> listDrives() {
    std::vector<std::string> drives;
#ifdef _WIN32
    DWORD driveMask = GetLogicalDrives();
    if (driveMask == 0) {
        std::cerr << "Error: Could not get logical drives." << std::endl;
        return drives;
    }
    for (char letter = 'A'; letter <= 'Z'; ++letter) {
        if (driveMask & (1 << (letter - 'A'))) {
            std::string driveName = std::string(1, letter) + ":\\";
            UINT type = GetDriveTypeA(driveName.c_str());
            // Consider removable drives. (You could also include fixed drives if desired.)
            if (type == DRIVE_REMOVABLE)
                drives.push_back(driveName);
        }
    }
#elif defined(__APPLE__)
    std::string volumesPath = "/Volumes";
    if (fs::exists(volumesPath) && fs::is_directory(volumesPath)) {
        for (const auto &entry : fs::directory_iterator(volumesPath)) {
            if (entry.is_directory())
                drives.push_back(entry.path().string());
        }
    } else {
        std::cerr << "Volumes path " << volumesPath << " does not exist or is not accessible." << std::endl;
    }
#else
    // Linux: Assume removable drives are mounted under /media/<user>
    std::string mediaPath = "/media/" + getCurrentUser();
    if (fs::exists(mediaPath) && fs::is_directory(mediaPath)) {
        for (const auto &entry : fs::directory_iterator(mediaPath)) {
            if (entry.is_directory())
                drives.push_back(entry.path().string());
        }
    } else {
        std::cerr << "Media path " << mediaPath << " does not exist or is not accessible." << std::endl;
    }
#endif
    return drives;
}

uintmax_t getFreeSpace(const std::string &targetPath) {
    std::error_code ec;
    fs::space_info spaceInfo = fs::space(targetPath, ec);
    if (ec) {
        std::cerr << "Error getting free space for " << targetPath 
                  << ": " << ec.message() << std::endl;
        return 0;
    }
    return spaceInfo.available;
}

void printUsage(const std::string &progName) {
#ifdef _WIN32
    std::cout << "Usage: " << progName << " -d <device_path> -i <iterations> -p <fill_mode>\n"
              << "  -d <device_path>   The drive letter of the device to erase (e.g., D:\\)\n";
#elif defined(__APPLE__)
    std::cout << "Usage: " << progName << " -d <device_path> -i <iterations> -p <fill_mode>\n"
              << "  -d <device_path>   The mount point of the device to erase (e.g., /Volumes/DeviceName)\n";
#else
    std::cout << "Usage: " << progName << " -d <device_path> -i <iterations> -p <fill_mode>\n"
              << "  -d <device_path>   The mount point of the device to erase (e.g., /media/username/DEVICE_NAME)\n";
#endif
    std::cout << "  -i <iterations>    Number of overwrite iterations (e.g., 2)\n"
              << "  -p <fill_mode>     Fill mode: \"zero\", \"one\", or \"mix\" (default: zero)\n\n"
              << "WARNING: This tool will overwrite all data on the target device and the data cannot be recovered.\n";
}

bool confirmAction(const std::string &message) {
    std::cout << message << " [y/N]: ";
    char response = 'n';
    std::cin >> response;
    return (response == 'y' || response == 'Y');
}
