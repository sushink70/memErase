#include <string>
#include <fstream>
#include <iostream>
#include "user.h"

using namespace std;

int list_of_drives(){
	string current_system_user;
	current_system_user = system_user_read();
	string cmd = "ls /media/" + current_system_user + " > storage-device-list";
	system(cmd.c_str());
	return 0;
}
string * system_connected_drives(){
	ifstream file("storage-device-list");
	string str;
	static string storagedeviceslist[4];
	int i=0;
	while (getline(file, str)){
		storagedeviceslist[i] = str;
		i++;
	}
	return storagedeviceslist;
}
