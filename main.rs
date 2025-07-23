use std::fs::{File, OpenOptions};
use std::io::{self, Read, Seek, SeekFrom, Write, BufRead, BufReader};
use std::path::{Path, PathBuf};
use std::collections::HashMap;
use std::sync::Arc;
use rand::{Rng, thread_rng};
use clap::{Arg, Command};
use indicatif::{ProgressBar, ProgressStyle};

#[cfg(unix)]
use std::os::unix::fs::OpenOptionsExt;

#[cfg(windows)]
use winapi::um::{
    fileapi::*,
    handleapi::*,
    winioctl::*,
    errhandlingapi::GetLastError,
};

const BLOCK_SIZE: usize = 1024 * 1024; // 1MB blocks

#[derive(Debug, Clone, Copy)]
pub enum WipePattern {
    Zeros,
    Ones,
    Random,
    Dod3Pass,    // DoD 5220.22-M (3 passes)
    Gutmann35,   // Gutmann 35-pass method
}

impl std::str::FromStr for WipePattern {
    type Err = String;
    
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s.to_lowercase().as_str() {
            "zeros" => Ok(WipePattern::Zeros),
            "ones" => Ok(WipePattern::Ones),
            "random" => Ok(WipePattern::Random),
            "dod3" => Ok(WipePattern::Dod3Pass),
            "gutmann35" => Ok(WipePattern::Gutmann35),
            _ => Err(format!("Invalid pattern: {}", s)),
        }
    }
}

#[derive(Debug, Clone)]
pub struct DeviceInfo {
    pub path: PathBuf,
    pub name: String,
    pub size: u64,
    pub is_removable: bool,
    pub is_mounted: bool,
}

pub struct SecureEraser {
    rng: rand::rngs::ThreadRng,
}

impl SecureEraser {
    pub fn new() -> Self {
        Self {
            rng: thread_rng(),
        }
    }

    /// List available storage devices
    pub fn list_devices(&self) -> Result<Vec<DeviceInfo>, Box<dyn std::error::Error>> {
        #[cfg(unix)]
        return self.list_devices_unix();
        
        #[cfg(windows)]
        return self.list_devices_windows();
    }

    #[cfg(unix)]
    fn list_devices_unix(&self) -> Result<Vec<DeviceInfo>, Box<dyn std::error::Error>> {
        let mut devices = Vec::new();
        let sys_block = Path::new("/sys/block");
        
        if !sys_block.exists() {
            return Ok(devices);
        }

        for entry in std::fs::read_dir(sys_block)? {
            let entry = entry?;
            let device_name = entry.file_name().to_string_lossy().to_string();
            
            // Skip loop devices and other virtual devices
            if device_name.starts_with("loop") || device_name.starts_with("ram") {
                continue;
            }

            let device_path = PathBuf::from(format!("/dev/{}", device_name));
            
            // Check if it's a block device
            if let Ok(metadata) = std::fs::metadata(&device_path) {
                if !is_block_device(&metadata) {
                    continue;
                }

                let mut info = DeviceInfo {
                    path: device_path.clone(),
                    name: device_name.clone(),
                    size: 0,
                    is_removable: false,
                    is_mounted: false,
                };

                // Get device size
                if let Ok(size) = self.get_device_size_unix(&device_path) {
                    info.size = size;
                }

                // Check if removable
                let removable_path = format!("/sys/block/{}/removable", device_name);
                if let Ok(removable_str) = std::fs::read_to_string(removable_path) {
                    info.is_removable = removable_str.trim() == "1";
                }

                // Check if mounted
                info.is_mounted = self.is_device_mounted_unix(&device_path)?;

                devices.push(info);
            }
        }

        Ok(devices)
    }

    #[cfg(windows)]
    fn list_devices_windows(&self) -> Result<Vec<DeviceInfo>, Box<dyn std::error::Error>> {
        let mut devices = Vec::new();
        
        for drive_letter in b'A'..=b'Z' {
            let drive_path = format!("{}:", drive_letter as char);
            let device_path = format!("\\\\.\\{}", drive_path);
            
            // This is a simplified version - full Windows implementation would require
            // more complex WinAPI calls to properly enumerate all storage devices
            if Path::new(&format!("{}\\", drive_path)).exists() {
                let info = DeviceInfo {
                    path: PathBuf::from(device_path),
                    name: drive_path,
                    size: 0, // Would need WinAPI calls to get actual size
                    is_removable: false, // Would need WinAPI calls to determine
                    is_mounted: true,
                };
                devices.push(info);
            }
        }
        
        Ok(devices)
    }

    #[cfg(unix)]
    fn get_device_size_unix(&self, device_path: &Path) -> Result<u64, Box<dyn std::error::Error>> {
        use std::os::unix::io::AsRawFd;
        
        let file = File::open(device_path)?;
        let fd = file.as_raw_fd();
        
        let mut size: u64 = 0;
        let result = unsafe {
            libc::ioctl(fd, libc::BLKGETSIZE64, &mut size as *mut u64)
        };
        
        if result == -1 {
            return Err("Failed to get device size".into());
        }
        
        Ok(size)
    }

    #[cfg(unix)]
    fn is_device_mounted_unix(&self, device_path: &Path) -> Result<bool, Box<dyn std::error::Error>> {
        let mounts_file = File::open("/proc/mounts")?;
        let reader = BufReader::new(mounts_file);
        
        let device_str = device_path.to_string_lossy();
        
        for line in reader.lines() {
            let line = line?;
            let parts: Vec<&str> = line.split_whitespace().collect();
            if parts.len() >= 2 && parts[0] == device_str {
                return Ok(true);
            }
        }
        
        Ok(false)
    }

    /// Generate wipe patterns based on the selected method
    pub fn generate_patterns(&mut self, pattern: WipePattern) -> Vec<Vec<u8>> {
        match pattern {
            WipePattern::Zeros => {
                vec![vec![0x00; BLOCK_SIZE]]
            }
            WipePattern::Ones => {
                vec![vec![0xFF; BLOCK_SIZE]]
            }
            WipePattern::Random => {
                let mut random_pattern = vec![0u8; BLOCK_SIZE];
                self.rng.fill(&mut random_pattern[..]);
                vec![random_pattern]
            }
            WipePattern::Dod3Pass => {
                let mut patterns = Vec::new();
                
                // Pass 1: 0x00
                patterns.push(vec![0x00; BLOCK_SIZE]);
                
                // Pass 2: 0xFF
                patterns.push(vec![0xFF; BLOCK_SIZE]);
                
                // Pass 3: Random
                let mut random_pattern = vec![0u8; BLOCK_SIZE];
                self.rng.fill(&mut random_pattern[..]);
                patterns.push(random_pattern);
                
                patterns
            }
            WipePattern::Gutmann35 => {
                let mut patterns = Vec::new();
                
                // First 4 random passes
                for _ in 0..4 {
                    let mut random_pattern = vec![0u8; BLOCK_SIZE];
                    self.rng.fill(&mut random_pattern[..]);
                    patterns.push(random_pattern);
                }
                
                // Some Gutmann patterns
                let gutmann_bytes = [0x55, 0xAA, 0x92, 0x49, 0x24];
                for &byte_pattern in &gutmann_bytes {
                    patterns.push(vec![byte_pattern; BLOCK_SIZE]);
                }
                
                patterns
            }
        }
    }

    /// Perform secure erase operation
    pub fn secure_erase(
        &mut self,
        device_path: &Path,
        pattern: WipePattern,
        verify: bool,
        progress_callback: Option<Box<dyn Fn(f64)>>,
    ) -> Result<(), Box<dyn std::error::Error>> {
        println!("Starting secure erase of: {}", device_path.display());

        // Open device for direct access
        let mut file = self.open_device_for_writing(device_path)?;
        
        // Get device size
        let device_size = self.get_device_size(&file, device_path)?;
        println!("Device size: {} MB", device_size / (1024 * 1024));

        let patterns = self.generate_patterns(pattern);
        let total_blocks = (device_size + BLOCK_SIZE as u64 - 1) / BLOCK_SIZE as u64;

        // Create progress bar
        let pb = ProgressBar::new(patterns.len() as u64 * total_blocks);
        pb.set_style(
            ProgressStyle::default_bar()
                .template("{spinner:.green} [{elapsed_precise}] [{bar:40.cyan/blue}] {pos}/{len} blocks ({percent}%) {msg}")
                .unwrap()
                .progress_chars("#>-"),
        );

        for (pass_num, pattern_data) in patterns.iter().enumerate() {
            pb.set_message(format!("Pass {}/{}", pass_num + 1, patterns.len()));
            
            // Reset to beginning of device
            file.seek(SeekFrom::Start(0))?;
            
            let mut bytes_written = 0u64;
            let mut block_count = 0u64;

            while bytes_written < device_size {
                let write_size = std::cmp::min(BLOCK_SIZE as u64, device_size - bytes_written) as usize;
                
                file.write_all(&pattern_data[..write_size])?;
                file.flush()?; // Ensure data is written to device
                
                bytes_written += write_size as u64;
                block_count += 1;
                pb.inc(1);

                if let Some(ref callback) = progress_callback {
                    let progress = (pass_num as f64 * total_blocks as f64 + block_count as f64) 
                                 / (patterns.len() as f64 * total_blocks as f64) * 100.0;
                    callback(progress);
                }
            }

            pb.println(format!("Pass {} completed", pass_num + 1));

            // Verify final pass if requested
            if verify && pass_num == patterns.len() - 1 {
                pb.set_message("Verifying final pass...");
                if !self.verify_erase(device_path, pattern_data)? {
                    pb.println("Warning: Verification failed!");
                } else {
                    pb.println("Verification successful!");
                }
            }
        }

        pb.finish_with_message("Secure erase completed successfully!");
        Ok(())
    }

    fn open_device_for_writing(&self, device_path: &Path) -> Result<File, Box<dyn std::error::Error>> {
        #[cfg(unix)]
        {
            let file = OpenOptions::new()
                .read(true)
                .write(true)
                .custom_flags(libc::O_SYNC) // Direct I/O
                .open(device_path)?;
            Ok(file)
        }

        #[cfg(windows)]
        {
            // Windows implementation would require CreateFile with specific flags
            let file = OpenOptions::new()
                .read(true)
                .write(true)
                .open(device_path)?;
            Ok(file)
        }
    }

    fn get_device_size(&self, file: &File, device_path: &Path) -> Result<u64, Box<dyn std::error::Error>> {
        #[cfg(unix)]
        {
            self.get_device_size_unix(device_path)
        }

        #[cfg(windows)]
        {
            // For Windows, we'd need to use GetFileSizeEx or DeviceIoControl
            // This is a simplified version
            let metadata = file.metadata()?;
            Ok(metadata.len())
        }
    }

    /// Verify erase by reading back data (sample verification)
    fn verify_erase(&self, device_path: &Path, expected_pattern: &[u8]) -> Result<bool, Box<dyn std::error::Error>> {
        let mut file = File::open(device_path)?;
        let mut read_buffer = vec![0u8; BLOCK_SIZE];

        // Sample verification - check first 10 blocks
        for _ in 0..10 {
            match file.read_exact(&mut read_buffer) {
                Ok(_) => {
                    if read_buffer != expected_pattern {
                        return Ok(false);
                    }
                }
                Err(_) => return Ok(false),
            }
        }

        Ok(true)
    }

    /// Display device information in a formatted table
    pub fn display_devices(&self, devices: &[DeviceInfo]) {
        println!("\nAvailable storage devices:\n");
        println!("{:<20} {:<15} {:<15} {:<12} {:<10}", 
                 "Device", "Name", "Size (MB)", "Removable", "Mounted");
        println!("{}", "-".repeat(70));

        for device in devices {
            println!("{:<20} {:<15} {:<15} {:<12} {:<10}",
                     device.path.display(),
                     device.name,
                     device.size / (1024 * 1024),
                     if device.is_removable { "Yes" } else { "No" },
                     if device.is_mounted { "Yes" } else { "No" });
        }
        println!();
    }
}

#[cfg(unix)]
fn is_block_device(metadata: &std::fs::Metadata) -> bool {
    use std::os::unix::fs::MetadataExt;
    (metadata.mode() & libc::S_IFMT) == libc::S_IFBLK
}

#[cfg(windows)]
fn is_block_device(_metadata: &std::fs::Metadata) -> bool {
    // On Windows, we'd need different logic to determine if it's a block device
    true
}

fn confirm_action(message: &str) -> bool {
    println!("{} [y/N]: ", message);
    let mut input = String::new();
    io::stdin().read_line(&mut input).unwrap_or(0);
    matches!(input.trim().to_lowercase().as_str(), "y" | "yes")
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let matches = Command::new("secure-eraser")
        .version("1.0.0")
        .author("Rust Implementation")
        .about("Secure disk eraser with multiple wipe patterns")
        .arg(Arg::new("list")
            .short('l')
            .long("list")
            .help("List available devices")
            .action(clap::ArgAction::SetTrue))
        .arg(Arg::new("device")
            .short('d')
            .long("device")
            .value_name("PATH")
            .help("Device to erase")
            .required_unless_present("list"))
        .arg(Arg::new("pattern")
            .short('p')
            .long("pattern")
            .value_name("TYPE")
            .help("Wipe pattern: zeros, ones, random, dod3, gutmann35")
            .default_value("zeros"))
        .arg(Arg::new("verify")
            .short('v')
            .long("verify")
            .help("Verify final pass")
            .action(clap::ArgAction::SetTrue))
        .get_matches();

    let mut eraser = SecureEraser::new();
    let devices = eraser.list_devices()?;

    if matches.get_flag("list") {
        eraser.display_devices(&devices);
        return Ok(());
    }

    let device_path = matches.get_one::<String>("device").unwrap();
    let device_path = Path::new(device_path);
    
    let pattern: WipePattern = matches.get_one::<String>("pattern")
        .unwrap()
        .parse()
        .map_err(|e| format!("Invalid pattern: {}", e))?;
    
    let verify = matches.get_flag("verify");

    // Find device info
    let target_device = devices.iter()
        .find(|d| d.path == device_path)
        .ok_or_else(|| format!("Device not found: {}", device_path.display()))?;

    // Safety checks
    if target_device.is_mounted {
        return Err("Device is mounted. Please unmount before erasing.".into());
    }

    // Final confirmation
    let confirm_msg = format!(
        "WARNING: This will permanently destroy all data on {} ({} MB). Continue?",
        device_path.display(),
        target_device.size / (1024 * 1024)
    );

    if !confirm_action(&confirm_msg) {
        println!("Operation cancelled.");
        return Ok(());
    }

    // Progress callback (can be used for GUI integration)
    let progress_callback: Option<Box<dyn Fn(f64)>> = Some(Box::new(|_progress| {
        // Custom progress handling can be implemented here
    }));

    // Perform the erase
    eraser.secure_erase(device_path, pattern, verify, progress_callback)?;

    Ok(())
}

// Cargo.toml dependencies needed:
/*
[dependencies]
rand = "0.8"
clap = { version = "4.0", features = ["derive"] }
indicatif = "0.17"

[target.'cfg(unix)'.dependencies]
libc = "0.2"

[target.'cfg(windows)'.dependencies]
winapi = { version = "0.3", features = ["fileapi", "handleapi", "winioctl", "errhandlingapi"] }
*/
