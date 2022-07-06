#include "devicememory.h"
#include "inputfilesize.h"
#include <cmath>
using namespace std;

int progress(){
	double file_size = input_file_size();
	double drive_size = devicememory();
	static double quotient_progress;
	double quotient_size = drive_size / file_size;
	quotient_progress = 100 / quotient_size;
	return quotient_progress;
}


