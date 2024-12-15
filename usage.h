
#include <iostream>

using namespace std;

int memerase_usage(){
	system("cat banner");
	cout << "memerase -f <file> -i <number of iterations> -> deep erase" << endl;
	cout << "memerase -o -> fill all sectors zeros" << endl;
	cout << "memerase -l -> fill all sectors once" << endl;
	return 0;
}

