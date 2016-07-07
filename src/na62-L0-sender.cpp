//============================================================================
// Name        :  Tel62 simulator sending L0 data
// Author      : Jonas Kunze
//
//Sending L1 data by Julio Calvo (julio.calvo.pinto@cern.es)
//============================================================================

#include <socket/NetworkHandler.h>
#include <socket/EthernetUtils.h>
#include <vector>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#include <tbb/task.h>
#include <l1/MRP.h>
#include <l1/MEP.h>
#include <tbb/tbb.h>
#include <pthread.h>
#include <zmq.h>
#include <zmq.hpp>
#include "options/MyOptions.h"

#include "Sender.h"
#include "SenderL1.h"

using namespace std;
using namespace na62;

int main(int argc, char* argv[]) {
	MyOptions::Load(argc, argv);
	//start = boost::posix_time::microsec_clock::local_time();

	NetworkHandler NetworkHandler(MyOptions::GetString(OPTION_ETH_DEVICE_NAME), MyOptions::GetInt(OPTION_L0_RECEIVER_PORT), MyOptions::GetInt(OPTION_CREAM_RECEIVER_PORT), 1);

	//Reading configuration options
	auto sourceIDs = Options::GetIntPairList(OPTION_DATA_SOURCE_IDS);
	auto sourceIDL1 = Options::GetIntPairList(OPTION_L1_DATA_SOURCE_IDS);
	uint autoburst = MyOptions::GetInt(OPTION_AUTO_BURST);
	uint pauseSeconds = MyOptions::GetInt(OPTION_DURATION_PAUSE);
	uint pauseBurstSec = MyOptions::GetInt(OPTION_DURATION_PAUSE_BBURST);
	uint timebased = MyOptions::GetInt(OPTION_TIME_BASED);



	int threadID = 0;

	std::vector<SenderL1*> sendersl1;
	std::vector<Sender*> senders;


	for( auto sourceL1 : sourceIDL1 ){

		std::cout << "Starting sender with SourceID " << std::hex
				<< sourceL1.first << ":" << sourceL1.second << std::dec << std::endl;
		SenderL1* senderl1 = new SenderL1();
		sendersl1.push_back(senderl1);
		senderl1->startThread(threadID++,
				"SenderL1" + std::to_string((int) sourceL1.first) + ":" + std::to_string((int) sourceL1.second));
	}


	uint burstNum = 0;
	//First option DEFAULT ONE: sending MEPs until the number define in options, then stop totally.
	if (autoburst != 1 && timebased != 1){

		for (auto sourceID : sourceIDs) {

			std::cout << "Starting sender with SourceID " << std::hex
					<< sourceID.first << ":" << sourceID.second << std::dec << std::endl;
			Sender* sender = new Sender(sourceID.first, sourceID.second,
					Options::GetInt(OPTION_MEPS_PER_BURST), autoburst, timebased);
			senders.push_back(sender);
			sender->startThread(threadID++,
					"Sender" + std::to_string((int) sourceID.first) + ":" + std::to_string((int) sourceID.second));
		}

		uint sentData = 0;
		uint32_t sentFrame = 0;
		for (auto snd : senders) {
			snd->join();
			sentData+=snd->getSentData();
			sentFrame+=snd->getSentFrames();
		}

		std::cout << "Sent " << sentData << " bytes" << std::endl;
		std::cout << "Sent packets L0 " << sentFrame << std::endl;

	}else{
		//Second and third choices. Autoburst has been defined with 0 or 1.
		while (true){

			boost::this_thread::sleep(boost::posix_time::microsec(1000));
			std::cout << "BurstID " << burstNum << std::endl;
			boost::this_thread::sleep(boost::posix_time::microsec(10));

			if (burstNum > 0)
			{std::cout<<"Restarting events generation"<< std::endl;}

			for (auto sourceID : sourceIDs) {

				std::cout << "Starting sender with SourceID " << std::hex
						<< sourceID.first << ":" << sourceID.second << std::dec << std::endl;
				Sender* sender = new Sender(sourceID.first, sourceID.second,
						Options::GetInt(OPTION_MEPS_PER_BURST), autoburst, timebased);
				senders.push_back(sender);
				sender->startThread(threadID++,
						"Sender" + std::to_string((int) sourceID.first) + ":" + std::to_string((int) sourceID.second));
			}

			uint sentData = 0;
			uint32_t sentFrame = 0;
			for (auto snd : senders) {
				snd->join();
				sentData+=snd->getSentData();
				sentFrame+=snd->getSentFrames();
				snd->setSentDataToZero();
			}

			std::cout << sentData << " Bytes sent in Burst " << burstNum << std::endl;
			std::cout << sentFrame << " L0 MEPs sent in Burst " << burstNum << std::endl;

			/***To show, in time and autoBurst execution modes, L1 complete statistics***/
			for (auto sour : sendersl1) {
				sour->unSetEndL1();
				do {
				}
				while (!sour->isSetEndL1());
			}

			uint32_t sentL1Frame = 0;
			uint mrp = 0;
			for (auto snd : sendersl1) {

				mrp+=snd->getMRP();
				sentL1Frame+=snd->getSentL1Frames();
			}
			std::cout << "L1 data sending is finished " << std::endl;
			std::cout <<  mrp << " L1 MRP received in Burst " << burstNum << std::endl;
			std::cout << sentL1Frame <<  " L1 packets sent in Burst " << burstNum << std::endl;

			if (timebased == 1 && autoburst != 1){

				/**Normal Pause begins*/
				time_t start2 = time(0);
				time_t timeLeft2 = (time_t) pauseSeconds - 2; /*two seconds already spent checking no more MRP is coming*/

				std::cout<< "L0 events generation will start again in " << pauseSeconds - 2 << " seconds" << std::endl;

				while ((timeLeft2 > 0))
				{
					time_t end2 = time(0);
					time_t timeTaken2 = end2 - start2;
					timeLeft2 = pauseSeconds - timeTaken2;
				}
				burstNum += 1;

			}if(autoburst == 1){
				/**Auto BurstId pause begins**/

				time_t start2 = time(0);
				time_t timeLeft2 = (time_t) pauseBurstSec - 2; /*two seconds already spent checking no more MRP is coming*/

				std::cout<< "L0 events generation will start again in " << pauseBurstSec - 2 << " seconds" << std::endl;

				while ((timeLeft2 > 0))
				{
					time_t end2 = time(0);
					time_t timeTaken2 = end2 - start2;
					timeLeft2 = pauseBurstSec - timeTaken2;
				}
				burstNum += 1;

				/**********/
			}


		}//End While

	}//End If Else. This program is going to finish
	/***To finish the one time execution mode and show L1 complete statistics***/
	for (auto sour : sendersl1) {
	sour->unSetEndL1();
		do {
		}
		while (!sour->isSetEndL1());
	}
	//uint sentL1Data = 0;
	uint32_t sentL1Frame = 0;
	uint mrp = 0;
	for (auto snd : sendersl1) {
		//sentL1Data+=snd->getSentL1Data();
		mrp+=snd->getMRP();
		sentL1Frame+=snd->getSentL1Frames();
	}
	std::cout << "MRP L1 " <<  mrp << std::endl;
	std::cout << "Sent packets L1 " << sentL1Frame << std::endl;
	return 0;
}

