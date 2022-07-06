#include <string>
#include <fstream>

using namespace std;

int system_current_user(){
	system("whoami > user");
	return 0;
}
string system_user_read(){
	ifstream file("user");
	string str, current_system_user;
	while (getline(file, str)){
		current_system_user = str;
	}
	return current_system_user;
}
