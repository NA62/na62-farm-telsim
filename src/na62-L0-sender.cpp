//============================================================================
// Name        :  Tel62 simulator sending L0 data
// Author      : Jonas Kunze (kunze.jonas@gmail.com)
//============================================================================

#include <options/Options.h>
#include <socket/PFringHandler.h>
#include <utils/LoggingHandler.hpp>
#include <vector>

#include "Sender.h"

using namespace std;
using namespace na62;

int main(int argc, char* argv[]) {
	Options::Initialize(argc, argv);

	InitializeLogging(argv);

	PFringHandler::Initialize("dna0");

	auto sourceIDs = Options::GetIntPairList(OPTION_DATA_SOURCE_IDS);

	int threadID = 0;
	std::vector<Sender*> senders;
	for (auto sourceID : sourceIDs) {
		Sender* sender = new Sender(sourceID.first, sourceID.second);
		senders.push_back(sender);
		sender->startThread(threadID++, -1, 15);
	}

	AExecutable::JoinAll();

	return 0;
}
