#include <sys/statvfs.h>
#include <cmath>
#include <string>
#include "user.h"
#include "devicelist.h"

using namespace std;

int devicememory(){
	const uint GB = (1024 * 1024) * 1024;
	string current_system_user = system_user_read();  
	string disk_mount_point = "/media/"+ current_system_user + "/" + currently_connected_system_drive;
	struct statvfs buffer;
	int ret = statvfs(disk_mount_point.c_str(), &buffer);
	const double total_disk_space = ceil((double)(buffer.f_blocks * buffer.f_frsize)/GB);
	return total_disk_space;

}
