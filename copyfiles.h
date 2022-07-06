#include "devicememory.h"
#include "user.h"
#include "inputfilesize.h"
#include <string>

using namespace std;

int copy_files(/*path/file from user*/ string file_path){
	int i = 0;
	int intput_file_size_ = intput_file_size_return();
	string cmd;
	string system_user_from_user_h = system_user_read();
	int device_drive_size = devicememory();

	cmd  = "cp" + file_path + "/home/" + system_user_from_user_h + "pendrive_name";

 	while (i < device_drive_size){
		system (cmd.c_str());
		//cout << thismuch% completed; info + log -> & add to log file as well;
		i++;
	}
	return 0;
}


