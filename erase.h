#include "user.h"
#include "devicelist.h"
#include <string>

using namespace std;

int erase_drive(string system_connected_drive_){
	string current_system_user;
	current_system_user = system_user_read();
	string system_drive_connected;
	system("rm -rf /media/" + current_system_user + "/" + system_connected_drive_ + "/*");
	return 0;
}

