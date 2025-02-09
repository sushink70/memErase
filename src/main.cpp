#include <iostream>
#include <string>

#include "erase.h"
#include "copyfiles.h"
#include "devicelist.h"
#include "usage.h"

using namespace std;

int main(int argc, char ** argv){
	string file_path, cmd; 
	int number_of_iteration;
	system("cat banner");
	memerase_usage();
	char input;
	for (int i = 1; i < argc; ++i){
		string str = *(argv + i);
		if (str == "-f"){
			file_path = *((argv + i) + 1);
			cmd = file_path + " >> file-from-user";
			system(cmd.c_str());
		}
		else if (str == "-i"){
			//string value_i = *(argv + i);
			number_of_iteration = stoi(*((argv + i) + 1));
		}
		else if(str == "-o"){
			//fill sectors with 0 (Safe, Quick)
			//fill_sectors_with_zero();
		}
		else if (str == "-l"){
			//fill sectors with 1 (Safe, Quick)
			//fill_sectors_with_one();
		}
		else {
			cout << "Please enter a valid input" << endl; //call usage here;
			memerase_usage();
		}
	}

	cout <<"Corrently inserted device : " << endl;
	string * list;
    list = system_connected_drives();
    for (int i = 0; i < 4; i++){
    	cout << list[i] << endl;
	}
	cout << "Insert the memory device or confirm the drive[Y/n]:" << endl;
	cin >> input;
	//which type of wipe -> fill with 0 or with 1 or 1 & 0, or DoD
	if (input == 'y' or input == 'Y'){ 
		for (int i = 0; i < number_of_iteration; i++){
			erase_drive(string system_connected_drive_);
			copy_files(file_path);
		}
	}
	else {
		cout << "Program terminated" << endl;
	}
return 0;
}

