#ifndef UTILITY_H
#define UTILITY_H

#include <string>
#include <vector>
#include <filesystem>
#include <cstdint> // for uintmax_t

// Get the current system user name.
std::string getCurrentUser();

// List removable drives.
// - On Windows: uses Windows API calls.
// - On macOS: lists directories under "/Volumes".
// - On Linux: lists directories under "/media/<user>".
std::vector<std::string> listDrives();

// Get free space (in bytes) on a given path.
uintmax_t getFreeSpace(const std::string &targetPath);

// Print usage information.
void printUsage(const std::string &progName);

// Confirm action with the user.
bool confirmAction(const std::string &message);

#endif // UTILITY_H
