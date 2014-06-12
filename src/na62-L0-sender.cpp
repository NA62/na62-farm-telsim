//============================================================================
// Name        :  Tel62 simulator sending L0 data
// Author      : Jonas Kunze (kunze.jonas@gmail.com)
//============================================================================


#include <socket/PFringHandler.h>
#include <vector>

#include "options/MyOptions.h"
#include "Sender.h"

using namespace std;
using namespace na62;

int main(int argc, char* argv[]) {
	MyOptions::Load(argc, argv);

	PFringHandler pfRingHandler("dna0");

	auto sourceIDs = Options::GetIntPairList(OPTION_DATA_SOURCE_IDS);

	int threadID = 0;
	std::vector<Sender*> senders;
	for (auto sourceID : sourceIDs) {
		Sender* sender = new Sender(sourceID.first, sourceID.second,
				Options::GetInt(OPTION_MEPS_PER_BURST));
		senders.push_back(sender);
		sender->startThread(threadID++, "Sender"+std::to_string((int)sourceID.first), -1, 15);
	}

	AExecutable::JoinAll();

	return 0;
}
