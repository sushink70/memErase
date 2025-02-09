# Memerase

**Memerase** is a secure data erasure tool that overwrites all data on a target storage device (e.g., removable drives, SD cards, HDDs, SSDs) so that the original data cannot be recovered. This tool supports Windows, macOS, and Linux.

## Features

- **Multiple Fill Modes:** Overwrite with zeros, ones, or a mix (alternating) fill.
- **Multiple Iterations:** Perform the overwrite process multiple times for extra security.
- **Cross-Platform:** Supports Windows (using Windows API), macOS (mount points under `/Volumes`), and Linux (mount points under `/media/<user>`).
- **Safety Checks:** Confirms with the user before executing destructive operations.

## Directory Structure

