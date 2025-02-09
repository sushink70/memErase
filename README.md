# Memerase

**Memerase** is a secure data erasure tool that overwrites all data on a target storage device (e.g., removable drives, SD cards, HDDs, SSDs) so that the original data cannot be recovered. This tool supports Windows, macOS, and Linux.

## Features

- **Multiple Fill Modes:** Overwrite with zeros, ones, or a mix (alternating) fill.
- **Multiple Iterations:** Perform the overwrite process multiple times for extra security.
- **Cross-Platform:** Supports Windows (using Windows API), macOS (mount points under `/Volumes`), and Linux (mount points under `/media/<user>`).
- **Safety Checks:** Confirms with the user before executing destructive operations.



## Building

Ensure you have a C++17–compliant compiler installed.

### On Linux/macOS

From the repository root:

```bash
make

./memerase -d /media/username/DEVICE_NAME -i 2 -p mix

## On Windows

g++ -std=c++17 -O2 -Wall -o memerase.exe src/main.cpp src/utility.cpp src/overwrite.cpp

memerase.exe -d D:\ -i 2 -p one

Usage: memerase -d <device_path> -i <iterations> -p <fill_mode>
  -d <device_path>   The mount point or drive letter of the device to erase.
                     (e.g., /media/username/DEVICE_NAME on Linux,
                     /Volumes/DeviceName on macOS, or D:\ on Windows)
  -i <iterations>    Number of overwrite iterations (e.g., 2)
  -p <fill_mode>     Fill mode: "zero", "one", or "mix" (default: zero)

WARNING: This tool will overwrite all data on the target device and the data cannot be recovered.

Disclaimer

WARNING: This tool permanently erases data. Use with extreme caution. The author is not responsible for any data loss.


---

### Makefile

```makefile
# Makefile for Memerase

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall
SRC_DIR  := src
SOURCES  := $(wildcard $(SRC_DIR)/*.cpp)
TARGET   := memerase

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)
