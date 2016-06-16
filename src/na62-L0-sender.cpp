//============================================================================
// Name        :  Tel62 simulator sending L0 data
// Author      : Jonas Kunze
//
//Sending L1 data by Julio Calvo (julio.calvo.pinto@cern.es)
//============================================================================

#include <socket/NetworkHandler.h>
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
//#include <options/Logging.h>
//#include <socket/ZMQHandler.h>
#include <zmq.hpp>
#include "options/MyOptions.h"
//#include <eventBuilding/SourceIDManager.h>
#include "Sender.h"
#include "SenderL1.h"

#define BUFSIZE 65000

using namespace std;
using namespace na62;



void *sendingL1(void *);

int main(int argc, char* argv[]) {
	MyOptions::Load(argc, argv);


	NetworkHandler NetworkHandler("lo", MyOptions::GetInt(OPTION_L0_RECEIVER_PORT), MyOptions::GetInt(OPTION_CREAM_RECEIVER_PORT), 1);
	auto sourceIDs = Options::GetIntPairList(OPTION_DATA_SOURCE_IDS);


	int threadID = 0;
	//std::atomic<bool> chBurst;
	uint32_t sentTotal = 0;
	std::vector<Sender*> senders;

	pthread_t threads[1];
	int rc;
	int i;
	for( i=0; i < 1; i++ ){
		cout << "Sender L1 data after request...ready " << i << endl;
		rc = pthread_create(&threads[i], NULL, sendingL1, (void *)i);
		if (rc){
			cout << "Error:unable to create thread senderL1 " << rc << endl;
			exit(-1);
		}
	}


	uint autoburst = MyOptions::GetInt(OPTION_AUTO_BURST);
	uint pauseSeconds = MyOptions::GetInt(OPTION_DURATION_PAUSE);
	uint pauseBurstSec = MyOptions::GetInt(OPTION_DURATION_PAUSE_BBURST);
	uint timebased = MyOptions::GetInt(OPTION_TIME_BASED);
	uint burstNum = 0;

	//First option DEFAULT ONE: sending MEPs until the number define in options, then stop totally.
	if (autoburst != 1 && timebased != 1){

		auto sourceIDs = Options::GetIntPairList(OPTION_DATA_SOURCE_IDS);
		std::vector<Sender*> senders;
		for (auto sourceID : sourceIDs) {

			std::cout << "Starting sender with SourceID " << std::hex
					<< sourceID.first << ":" << sourceID.second << std::dec << std::endl;
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
		std::cout << "Sent " << sentData << " bytes" << std::endl;



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
				Sender* sender = new Sender(sourceID.first, sourceID.second, Options::GetInt(OPTION_MEPS_PER_BURST));
				senders.push_back(sender);
				sender->startThread(threadID++,
						"Sender" + std::to_string((int) sourceID.first) + ":" + std::to_string((int) sourceID.second));

			}


			AExecutable::JoinAll();
			uint sentData = 0;
			for (auto snd : senders) {

				sentData+=snd->getSentData();
				snd->setSentDataToZero();

			}
			std::cout << "Sent in last Burst " << sentData << " bytes" << std::endl;
			sentTotal += sentData;
			std::cout << "Total sent " << sentTotal << " bytes" << std::endl;

			if (timebased == 1 && autoburst != 1){

				/**Normal Pause begins*/
				time_t start2 = time(0);
				time_t timeLeft2 = (time_t) pauseSeconds;

				std::cout<< "Events generation is paused for " << pauseSeconds << " seconds" << std::endl;
				boost::this_thread::sleep(boost::posix_time::microsec(1000));
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
				time_t timeLeft2 = (time_t) pauseBurstSec;

				std::cout<< "Events generation is paused for " << pauseBurstSec << " seconds" << std::endl;
				boost::this_thread::sleep(boost::posix_time::microsec(1000));
				while ((timeLeft2 > 0))
				{
					time_t end2 = time(0);
					time_t timeTaken2 = end2 - start2;
					timeLeft2 = pauseBurstSec - timeTaken2;
				}
				burstNum += 1;

				/**********/
			}
		}
	}
	return 0;
}

void *sendingL1(void *threadid)
{

	/*socket Send to*/
	boost::asio::io_service io_service_;
	boost::asio::ip::udp::socket socket_(io_service_);
	boost::asio::ip::udp::endpoint receiver_endpoint_;

	/*socket receive */
	static int sd;
	struct sockaddr_in hostAddr;
	bzero(&hostAddr, sizeof(hostAddr));
	struct sockaddr_in senderAddr;
	socklen_t senderLen = sizeof(senderAddr);

	hostAddr.sin_family = PF_INET;
	hostAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	hostAddr.sin_port = htons(MyOptions::GetInt(OPTION_CREAM_MULTICAST_PORT));

	sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sd < 0) {perror("socket()");}


	int one = 1;
	int r = setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, (char*)&one, sizeof(one));
	if (r < 0) {perror("setsockopt(SO_REUSEPORT)");}

	//binding socket
	if (bind (sd, (struct sockaddr *)&hostAddr, sizeof(hostAddr)) < 0) {
		perror("bind()");
	}
	/*End socket receiveFrom*/


	int eventLength = 4;//MyOptions::GetInt(OPTION_EVENT_LENGTH);
	auto sourceL1IDs = Options::GetIntPairList(OPTION_L1_DATA_SOURCE_IDS);
	char bufferl1[BUFSIZE];
	memset(bufferl1, 0, BUFSIZE);

	/*Creating dummy data to be sent*/
	uint randomLength = 100000 * sizeof(int);
	char randomData[randomLength];
	memset(randomData, 1, randomLength);

	ssize_t result;


	while(true){

		result = recvfrom(sd, (void*) bufferl1, BUFSIZE, 0, (struct sockaddr *)&senderAddr, &senderLen);
		if (result > 0){

			char str[INET_ADDRSTRLEN];
			uint16_t offsetTrigger = sizeof(l1::TRIGGER_RAW_HDR);
			char* packet = new char[BUFSIZE];
			memset(packet, 0, BUFSIZE);
			l1::L1_EVENT_RAW_HDR *hdrToBeSent = new l1::L1_EVENT_RAW_HDR;
			char *buffTrigger = new char[offsetTrigger];
			uint16_t offsetMRP = sizeof(l1::MRP_FRAME_HDR);
			char *mrpHeader = new char[offsetMRP];


			memcpy(mrpHeader, bufferl1, offsetMRP);
			l1::MRP_FRAME_HDR* hdrReceived = (l1::MRP_FRAME_HDR*) mrpHeader;
			uint16_t numTriggers = hdrReceived->MRP_HDR.numberOfTriggers;
			in_addr_t ip = hdrReceived->MRP_HDR.ipAddress;
			inet_ntop(AF_INET, &(ip), str, INET_ADDRSTRLEN);
			//IP address is properly read

			/***open socket Send_to with IP address***/
			boost::system::error_code ec;
			boost::asio::ip::udp::resolver resolver(io_service_);
			boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), str , std::to_string(MyOptions::GetInt(OPTION_CREAM_RECEIVER_PORT)));
			receiver_endpoint_ = *resolver.resolve(query);
			socket_.open(boost::asio::ip::udp::v4());



			while (numTriggers > 0){
				offsetTrigger = sizeof(l1::TRIGGER_RAW_HDR) * (numTriggers-1);
				memcpy(buffTrigger, bufferl1 + sizeof(l1::MRP_FRAME_HDR) + offsetTrigger, sizeof(l1::TRIGGER_RAW_HDR));
				l1::TRIGGER_RAW_HDR* triggerReceived = (l1::TRIGGER_RAW_HDR*) buffTrigger;

				//eventNumber_ = hdrReceived->eventNumber;
				hdrToBeSent->eventNumber = triggerReceived->eventNumber;
				hdrToBeSent->timestamp = triggerReceived->timestamp;
				hdrToBeSent->l0TriggerWord = triggerReceived->triggerTypeWord;
				hdrToBeSent->numberOf4BWords = 5;

				memcpy(packet + sizeof(l1::L1_EVENT_RAW_HDR), randomData, eventLength);

				uint i = 0;
				for (auto sourceID : sourceL1IDs) {
					uint telL1 = sourceID.second;
					for (i=0; i<telL1; i++){
					//hdrToBeSent->sourceSubID = sourceID.second;
					hdrToBeSent->sourceID = sourceID.first;
					memcpy(packet, reinterpret_cast<char*> (hdrToBeSent), sizeof(l1::L1_EVENT_RAW_HDR));

					socket_.send_to(boost::asio::buffer(packet, eventLength + sizeof(l1::L1_EVENT_RAW_HDR)), receiver_endpoint_);

					}
				}
				offsetTrigger -= sizeof(l1::TRIGGER_RAW_HDR);
				numTriggers--;

			}

			delete[] packet;
			delete[] buffTrigger;
			delete[] mrpHeader;
			delete hdrToBeSent;
			socket_.close(ec);
		}/*end if result*/


		boost::this_thread::sleep(boost::posix_time::microsec(1));

	}//while


	pthread_exit(NULL);
}
