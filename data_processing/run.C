// Schedule execution of the file processing.
// The check_file.C will check if the new file is available and then call the process_file.C.

#include "TTimer.h"
#include "Getline.h"
#include "TSystem.h"
#include <iostream>

TString prev = "noname";
void run()
{
	//char *input;
	char *cmd = ".x check_for_file.C(prev)";
	int interval = 1000;
	Bool_t done = kFALSE;
	Int_t ii=0;
	//TTimer *timer = new TTimer("gSystem->ProcessEvents();",1000,kFALSE);
	TTimer *timer = new TTimer(cmd,interval,kFALSE);
	cout<<cmd<<" scheduled for every "<<interval<<" ms"<<endl;
	timer->TurnOn();
	gRunInProgress=1; // defined in Init.C
}
