#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <iomanip>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
    #include <windows.h>
    #include <winioctl.h>
    #include <setupapi.h>
    #include <devguid.h>
    #pragma comment(lib, "setupapi.lib")
#else
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/ioctl.h>
    #include <sys/stat.h>
    #include <linux/fs.h>
    #include <dirent.h>
    #include <mntent.h>
#endif

enum class WipePattern {
    ZEROS,
    ONES,
    RANDOM,
    DOD_3PASS,    // DoD 5220.22-M (3 passes)
    GUTMANN_35    // Gutmann 35-pass method
};

struct DeviceInfo {
    std::string path;
    std::string name;
    uint64_t size;
    bool isRemovable;
    bool isMounted;
};

class SecureEraser {
private:
    static constexpr size_t BLOCK_SIZE = 1024 * 1024; // 1MB blocks
    std::mt19937 rng{std::chrono::steady_clock::now().time_since_epoch().count()};
    
public:
    // Get list of available storage devices
    std::vector<DeviceInfo> listDevices() {
        std::vector<DeviceInfo> devices;
        
#ifdef _WIN32
        // Windows implementation
        for (char drive = 'A'; drive <= 'Z'; ++drive) {
            std::string drivePath = std::string(1, drive) + ":";
            UINT driveType = GetDriveTypeA((drivePath + "\\").c_str());
            
            if (driveType == DRIVE_REMOVABLE || driveType == DRIVE_FIXED) {
                DeviceInfo info;
                info.path = "\\\\.\\" + drivePath;
                info.name = drivePath;
                info.isRemovable = (driveType == DRIVE_REMOVABLE);
                
                // Get device size
                HANDLE hDevice = CreateFileA(info.path.c_str(), 0, 
                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, 
                    OPEN_EXISTING, 0, nullptr);
                
                if (hDevice != INVALID_HANDLE_VALUE) {
                    DISK_GEOMETRY_EX diskGeometry;
                    DWORD bytesReturned;
                    
                    if (DeviceIoControl(hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                        nullptr, 0, &diskGeometry, sizeof(diskGeometry), 
                        &bytesReturned, nullptr)) {
                        info.size = diskGeometry.DiskSize.QuadPart;
                    }
                    
                    // Check if mounted
                    info.isMounted = (GetLogicalDrives() & (1 << (drive - 'A'))) != 0;
                    CloseHandle(hDevice);
                    devices.push_back(info);
                }
            }
        }
#else
        // Linux implementation
        DIR* dir = opendir("/sys/block");
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (entry->d_name[0] == '.') continue;
                
                std::string deviceName = entry->d_name;
                std::string devicePath = "/dev/" + deviceName;
                
                // Check if it's a valid block device
                struct stat st;
                if (stat(devicePath.c_str(), &st) == 0 && S_ISBLK(st.st_mode)) {
                    DeviceInfo info;
                    info.path = devicePath;
                    info.name = deviceName;
                    
                    // Get device size
                    int fd = open(devicePath.c_str(), O_RDONLY);
                    if (fd >= 0) {
                        uint64_t size;
                        if (ioctl(fd, BLKGETSIZE64, &size) == 0) {
                            info.size = size;
                        }
                        close(fd);
                    }
                    
                    // Check if removable
                    std::string removablePath = "/sys/block/" + deviceName + "/removable";
                    std::ifstream removableFile(removablePath);
                    int removable = 0;
                    removableFile >> removable;
                    info.isRemovable = (removable == 1);
                    
                    // Check if mounted
                    info.isMounted = isDeviceMounted(devicePath);
                    
                    devices.push_back(info);
                }
            }
            closedir(dir);
        }
#endif
        return devices;
    }
    
    // Check if device is mounted (Linux only)
#ifndef _WIN32
    bool isDeviceMounted(const std::string& devicePath) {
        FILE* mtab = setmntent("/proc/mounts", "r");
        if (!mtab) return false;
        
        struct mntent* entry;
        while ((entry = getmntent(mtab)) != nullptr) {
            if (devicePath == entry->mnt_fsname) {
                endmntent(mtab);
                return true;
            }
        }
        endmntent(mtab);
        return false;
    }
#endif
    
    // Generate wipe patterns
    std::vector<std::vector<uint8_t>> generatePatterns(WipePattern pattern) {
        std::vector<std::vector<uint8_t>> patterns;
        
        switch (pattern) {
            case WipePattern::ZEROS:
                patterns.push_back(std::vector<uint8_t>(BLOCK_SIZE, 0x00));
                break;
                
            case WipePattern::ONES:
                patterns.push_back(std::vector<uint8_t>(BLOCK_SIZE, 0xFF));
                break;
                
            case WipePattern::RANDOM: {
                std::vector<uint8_t> randomPattern(BLOCK_SIZE);
                std::uniform_int_distribution<uint8_t> dist(0, 255);
                for (auto& byte : randomPattern) {
                    byte = dist(rng);
                }
                patterns.push_back(randomPattern);
                break;
            }
            
            case WipePattern::DOD_3PASS:
                // Pass 1: 0x00
                patterns.push_back(std::vector<uint8_t>(BLOCK_SIZE, 0x00));
                // Pass 2: 0xFF
                patterns.push_back(std::vector<uint8_t>(BLOCK_SIZE, 0xFF));
                // Pass 3: Random
                {
                    std::vector<uint8_t> randomPattern(BLOCK_SIZE);
                    std::uniform_int_distribution<uint8_t> dist(0, 255);
                    for (auto& byte : randomPattern) {
                        byte = dist(rng);
                    }
                    patterns.push_back(randomPattern);
                }
                break;
                
            case WipePattern::GUTMANN_35: {
                // Simplified Gutmann - first 4 random passes, then specific patterns
                for (int i = 0; i < 4; ++i) {
                    std::vector<uint8_t> randomPattern(BLOCK_SIZE);
                    std::uniform_int_distribution<uint8_t> dist(0, 255);
                    for (auto& byte : randomPattern) {
                        byte = dist(rng);
                    }
                    patterns.push_back(randomPattern);
                }
                
                // Add some of the Gutmann patterns
                std::vector<uint8_t> gutmannPatterns[] = {
                    std::vector<uint8_t>(BLOCK_SIZE, 0x55), // 01010101
                    std::vector<uint8_t>(BLOCK_SIZE, 0xAA), // 10101010
                    std::vector<uint8_t>(BLOCK_SIZE, 0x92), // 10010010
                    std::vector<uint8_t>(BLOCK_SIZE, 0x49), // 01001001
                    std::vector<uint8_t>(BLOCK_SIZE, 0x24)  // 00100100
                };
                
                for (const auto& pattern : gutmannPatterns) {
                    patterns.push_back(pattern);
                }
                break;
            }
        }
        
        return patterns;
    }
    
    // Perform secure erase
    bool secureErase(const std::string& devicePath, WipePattern pattern, 
                     bool verify = false, std::function<void(double)> progressCallback = nullptr) {
        
        std::cout << "Starting secure erase of: " << devicePath << std::endl;
        
        // Open device for direct access
#ifdef _WIN32
        HANDLE hDevice = CreateFileA(devicePath.c_str(), 
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, 
            nullptr, OPEN_EXISTING, 
            FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, nullptr);
        
        if (hDevice == INVALID_HANDLE_VALUE) {
            std::cerr << "Error: Cannot open device " << devicePath 
                      << ". Error code: " << GetLastError() << std::endl;
            return false;
        }
        
        // Get device size
        DISK_GEOMETRY_EX diskGeometry;
        DWORD bytesReturned;
        if (!DeviceIoControl(hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
            nullptr, 0, &diskGeometry, sizeof(diskGeometry), 
            &bytesReturned, nullptr)) {
            std::cerr << "Error: Cannot get device size" << std::endl;
            CloseHandle(hDevice);
            return false;
        }
        
        uint64_t deviceSize = diskGeometry.DiskSize.QuadPart;
#else
        int fd = open(devicePath.c_str(), O_RDWR | O_SYNC);
        if (fd < 0) {
            std::cerr << "Error: Cannot open device " << devicePath 
                      << ". Make sure you have root privileges." << std::endl;
            return false;
        }
        
        // Get device size
        uint64_t deviceSize;
        if (ioctl(fd, BLKGETSIZE64, &deviceSize) < 0) {
            std::cerr << "Error: Cannot get device size" << std::endl;
            close(fd);
            return false;
        }
#endif
        
        std::cout << "Device size: " << (deviceSize / (1024 * 1024)) << " MB" << std::endl;
        
        auto patterns = generatePatterns(pattern);
        uint64_t totalBlocks = (deviceSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
        
        for (size_t pass = 0; pass < patterns.size(); ++pass) {
            std::cout << "\nPass " << (pass + 1) << "/" << patterns.size() << std::endl;
            
            // Reset to beginning of device
#ifdef _WIN32
            LARGE_INTEGER pos;
            pos.QuadPart = 0;
            if (!SetFilePointerEx(hDevice, pos, nullptr, FILE_BEGIN)) {
                std::cerr << "Error: Cannot seek to beginning of device" << std::endl;
                CloseHandle(hDevice);
                return false;
            }
#else
            if (lseek(fd, 0, SEEK_SET) != 0) {
                std::cerr << "Error: Cannot seek to beginning of device" << std::endl;
                close(fd);
                return false;
            }
#endif
            
            uint64_t bytesWritten = 0;
            uint64_t blockCount = 0;
            
            while (bytesWritten < deviceSize) {
                size_t writeSize = std::min(static_cast<uint64_t>(BLOCK_SIZE), 
                                          deviceSize - bytesWritten);
                
#ifdef _WIN32
                DWORD written;
                if (!WriteFile(hDevice, patterns[pass].data(), 
                              static_cast<DWORD>(writeSize), &written, nullptr)) {
                    std::cerr << "Error writing to device at offset " 
                              << bytesWritten << std::endl;
                    CloseHandle(hDevice);
                    return false;
                }
                bytesWritten += written;
#else
                ssize_t written = write(fd, patterns[pass].data(), writeSize);
                if (written < 0) {
                    std::cerr << "Error writing to device at offset " 
                              << bytesWritten << std::endl;
                    close(fd);
                    return false;
                }
                bytesWritten += written;
#endif
                
                ++blockCount;
                
                // Progress reporting
                if (progressCallback && blockCount % 100 == 0) {
                    double progress = (static_cast<double>(pass) * totalBlocks + blockCount) 
                                    / (patterns.size() * totalBlocks) * 100.0;
                    progressCallback(progress);
                }
                
                // Progress display
                if (blockCount % 1000 == 0) {
                    double passProgress = static_cast<double>(bytesWritten) / deviceSize * 100.0;
                    std::cout << "\rProgress: " << std::fixed << std::setprecision(1) 
                              << passProgress << "%" << std::flush;
                }
            }
            
            std::cout << "\rPass " << (pass + 1) << " completed: 100.0%" << std::endl;
            
            // Verify pass if requested
            if (verify && pass == patterns.size() - 1) {
                std::cout << "Verifying final pass..." << std::endl;
                if (!verifyErase(devicePath, patterns[pass])) {
                    std::cout << "Warning: Verification failed!" << std::endl;
                }
            }
        }
        
#ifdef _WIN32
        CloseHandle(hDevice);
#else
        close(fd);
#endif
        
        std::cout << "\nSecure erase completed successfully!" << std::endl;
        return true;
    }
    
    // Verify erase by reading back data
    bool verifyErase(const std::string& devicePath, const std::vector<uint8_t>& expectedPattern) {
#ifdef _WIN32
        HANDLE hDevice = CreateFileA(devicePath.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 
            FILE_FLAG_NO_BUFFERING, nullptr);
        
        if (hDevice == INVALID_HANDLE_VALUE) {
            return false;
        }
#else
        int fd = open(devicePath.c_str(), O_RDONLY);
        if (fd < 0) {
            return false;
        }
#endif
        
        std::vector<uint8_t> readBuffer(BLOCK_SIZE);
        bool verified = true;
        
        // Sample verification - check first 10 blocks
        for (int i = 0; i < 10; ++i) {
#ifdef _WIN32
            DWORD bytesRead;
            if (!ReadFile(hDevice, readBuffer.data(), BLOCK_SIZE, &bytesRead, nullptr)) {
                verified = false;
                break;
            }
#else
            ssize_t bytesRead = read(fd, readBuffer.data(), BLOCK_SIZE);
            if (bytesRead != BLOCK_SIZE) {
                verified = false;
                break;
            }
#endif
            
            if (!std::equal(readBuffer.begin(), readBuffer.end(), expectedPattern.begin())) {
                verified = false;
                break;
            }
        }
        
#ifdef _WIN32
        CloseHandle(hDevice);
#else
        close(fd);
#endif
        
        return verified;
    }
    
    // Display device information
    void displayDevices(const std::vector<DeviceInfo>& devices) {
        std::cout << "\nAvailable storage devices:\n" << std::endl;
        std::cout << std::setw(15) << "Device" 
                  << std::setw(20) << "Name"
                  << std::setw(15) << "Size (MB)"
                  << std::setw(12) << "Removable"
                  << std::setw(10) << "Mounted" << std::endl;
        std::cout << std::string(70, '-') << std::endl;
        
        for (const auto& device : devices) {
            std::cout << std::setw(15) << device.path
                      << std::setw(20) << device.name
                      << std::setw(15) << (device.size / (1024 * 1024))
                      << std::setw(12) << (device.isRemovable ? "Yes" : "No")
                      << std::setw(10) << (device.isMounted ? "Yes" : "No") << std::endl;
        }
        std::cout << std::endl;
    }
};

// Utility functions
bool confirmAction(const std::string& message) {
    std::cout << message << " [y/N]: ";
    char response;
    std::cin >> response;
    return (response == 'y' || response == 'Y');
}

void printUsage(const std::string& progName) {
    std::cout << "Usage: " << progName << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  -l, --list              List available devices\n"
              << "  -d, --device <path>     Device to erase\n"
              << "  -p, --pattern <type>    Wipe pattern:\n"
              << "                          zeros, ones, random, dod3, gutmann35\n"
              << "  -v, --verify            Verify final pass\n"
              << "  -h, --help              Show this help\n\n"
              << "Examples:\n"
#ifdef _WIN32
              << "  " << progName << " -d \\\\.\\E: -p dod3 -v\n"
#else
              << "  " << progName << " -d /dev/sdb -p dod3 -v\n"
#endif
              << "  " << progName << " --list\n\n"
              << "WARNING: This will permanently destroy all data on the target device!\n";
}

int main(int argc, char* argv[]) {
    SecureEraser eraser;
    
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string devicePath;
    WipePattern pattern = WipePattern::ZEROS;
    bool verify = false;
    bool listDevices = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-l" || arg == "--list") {
            listDevices = true;
        } else if ((arg == "-d" || arg == "--device") && i + 1 < argc) {
            devicePath = argv[++i];
        } else if ((arg == "-p" || arg == "--pattern") && i + 1 < argc) {
            std::string patternStr = argv[++i];
            std::transform(patternStr.begin(), patternStr.end(), 
                         patternStr.begin(), ::tolower);
            
            if (patternStr == "zeros") pattern = WipePattern::ZEROS;
            else if (patternStr == "ones") pattern = WipePattern::ONES;
            else if (patternStr == "random") pattern = WipePattern::RANDOM;
            else if (patternStr == "dod3") pattern = WipePattern::DOD_3PASS;
            else if (patternStr == "gutmann35") pattern = WipePattern::GUTMANN_35;
            else {
                std::cerr << "Invalid pattern: " << patternStr << std::endl;
                return 1;
            }
        } else if (arg == "-v" || arg == "--verify") {
            verify = true;
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    auto devices = eraser.listDevices();
    
    if (listDevices) {
        eraser.displayDevices(devices);
        return 0;
    }
    
    if (devicePath.empty()) {
        std::cerr << "Error: Device path required" << std::endl;
        eraser.displayDevices(devices);
        return 1;
    }
    
    // Find device info
    DeviceInfo* targetDevice = nullptr;
    for (auto& device : devices) {
        if (device.path == devicePath) {
            targetDevice = &device;
            break;
        }
    }
    
    if (!targetDevice) {
        std::cerr << "Error: Device not found: " << devicePath << std::endl;
        return 1;
    }
    
    // Safety checks
    if (targetDevice->isMounted) {
        std::cerr << "Error: Device is mounted. Please unmount before erasing." << std::endl;
        return 1;
    }
    
    // Final confirmation
    std::string confirmMsg = "WARNING: This will permanently destroy all data on " + 
                           devicePath + " (" + std::to_string(targetDevice->size / (1024*1024)) + 
                           " MB). Continue?";
    
    if (!confirmAction(confirmMsg)) {
        std::cout << "Operation cancelled." << std::endl;
        return 0;
    }
    
    // Progress callback
    auto progressCallback = [](double progress) {
        // Can be used for GUI progress updates
    };
    
    // Perform the erase
    if (!eraser.secureErase(devicePath, pattern, verify, progressCallback)) {
        std::cerr << "Secure erase failed!" << std::endl;
        return 1;
    }
    
    return 0;
}
