//============================================================================
// Name        :  Tel62 simulator sending L0 data
// Author      : Jonas Kunze (kunze.jonas@gmail.com)
//============================================================================

#include <socket/NetworkHandler.h>
#include <vector>
#include <options/Logging.h>

#include "options/MyOptions.h"
#include <eventBuilding/SourceIDManager.h>
#include "Sender.h"

using namespace std;
using namespace na62;

int main(int argc, char* argv[]) {
	MyOptions::Load(argc, argv);

	if (Options::Isset(OPTION_USE_PF_RING)) {
		NetworkHandler NetworkHandler("dna0");
	} else {
		NetworkHandler NetworkHandler("lo");
	}

	auto sourceIDs = Options::GetIntPairList(OPTION_DATA_SOURCE_IDS);

	int threadID = 0;
	std::vector<Sender*> senders;
	for (auto sourceID : sourceIDs) {
		if (sourceID.first == SOURCE_ID_LKr) {
			LOG_INFO << "Drop source "<< std::hex
					<< sourceID.first << ":" << sourceID.second << std::dec << ENDL;
			// Skip lkr data
			continue;
		}
		LOG_INFO << "Starting sender with SourceID " << std::hex
				<< sourceID.first << ":" << sourceID.second << std::dec << ENDL;
		Sender* sender = new Sender(sourceID.first, sourceID.second,
				Options::GetInt(OPTION_MEPS_PER_BURST));
		senders.push_back(sender);
		sender->startThread(threadID++,
				"Sender" + std::to_string((int) sourceID.first) + ":" + std::to_string((int) sourceID.second));
	}


	AExecutable::JoinAll();

	uint sentData=0;
	for (auto snd : senders) {
		sentData+=snd->getSentData();
	}
	LOG_INFO << "Sent " << sentData << " bytes" << ENDL;
	return 0;
}
