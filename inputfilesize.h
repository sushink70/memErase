#include <string>
#include <fstream>

using namespace std;

int input_file_size_save_to_file(string file_from_main){
	file_from_main = "ls -l " + file_from_main + " > file_from_user";
	system(file_from_main.c_str());
	return 0;
}
//open file_from_user and return the value to copyfiles.h
int input_file_size_return(){
	ifstream file("file_from_user");
	string str, input_file_size_return_;
	while (getline(file, str)){
		input_file_size_return_ = str;
	}
	return input_file_size_return_;
}

	
	
