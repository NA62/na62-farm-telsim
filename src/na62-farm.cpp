//============================================================================
// Name        :  Tel62 simulator sending L0 data
// Author      : Jonas Kunze (kunze.jonas@gmail.com)
//============================================================================


#include <options/Options.h>
#include <socket/PFringHandler.h>

#include "Sender.h"

using namespace std;
using namespace na62;

int main(int argc, char* argv[]) {

	Options::Initialize(argc, argv);

	PFringHandler::Initialize("dna0");

	int numberOfSources = 10;
	for (int i = 0; i < numberOfSources; i++) {
		Sender s(i);
		s.startThread(i, -1, 15);
	}

	AExecutable::JoinAll();

	return 0;
}
