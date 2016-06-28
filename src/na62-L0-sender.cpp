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

#define BUFSIZE 65000

using namespace std;
using namespace na62;


std::atomic<uint64_t> framesl1;
std::atomic<double> frec;
boost::posix_time::ptime start;
uint32_t myIP;

void *sendingL1(void *);

int main(int argc, char* argv[]) {
	MyOptions::Load(argc, argv);
	start = boost::posix_time::microsec_clock::local_time();

	NetworkHandler NetworkHandler(MyOptions::GetString(OPTION_ETH_DEVICE_NAME), MyOptions::GetInt(OPTION_L0_RECEIVER_PORT), MyOptions::GetInt(OPTION_CREAM_RECEIVER_PORT), 1);
	auto sourceIDs = Options::GetIntPairList(OPTION_DATA_SOURCE_IDS);


	int threadID = 0;
	uint32_t sentTotal = 0;
	std::vector<Sender*> senders;

	//L1 threading. Only one thread at the moment.
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

	//Reading configuration options
	uint autoburst = MyOptions::GetInt(OPTION_AUTO_BURST);
	uint pauseSeconds = MyOptions::GetInt(OPTION_DURATION_PAUSE);
	uint pauseBurstSec = MyOptions::GetInt(OPTION_DURATION_PAUSE_BBURST);
	uint timebased = MyOptions::GetInt(OPTION_TIME_BASED);
	myIP = EthernetUtils::GetIPOfInterface(MyOptions::GetString(OPTION_ETH_DEVICE_NAME));
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
			uint32_t sentFrame = 0;
			for (auto snd : senders) {

				sentData+=snd->getSentData();
				sentFrame+=snd->getSentFrames();
				snd->setSentDataToZero();

			}
			std::cout << "Sent in last Burst " << sentData << " bytes" << std::endl;
			sentTotal += sentData;
			//std::cout << "Total bytes sent " << sentTotal << " bytes" << std::endl;
			std::cout << "Sent packets L0 " << sentFrame << std::endl;
			std::cout << "Sent packets L1 " << framesl1 << std::endl;
			std::cout << "Total packets sent " << framesl1 + sentFrame << std::endl;

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

	boost::posix_time::time_duration timeTaken;
	boost::posix_time::ptime end;

	/*socket SendTo Boost*/
	boost::asio::io_service io_service_;
	boost::system::error_code ec;
	boost::asio::ip::udp::socket socket_(io_service_);
	boost::asio::ip::udp::endpoint receiver_endpoint_;
	boost::asio::ip::udp::resolver resolver(io_service_);
	socket_.open(boost::asio::ip::udp::v4());

	/*socket receive */
	static int sd;
	struct sockaddr_in hostAddr;
	bzero(&hostAddr, sizeof(hostAddr));
	struct sockaddr_in senderAddr;
	socklen_t senderLen = sizeof(senderAddr);
	hostAddr.sin_family = AF_INET;
	in_addr_t ip;
	//inet_pton(AF_INET, "10.194.20.9", &(hostAddr.sin_addr.s_addr));
	hostAddr.sin_addr.s_addr = myIP;
	hostAddr.sin_port = htons(MyOptions::GetInt(OPTION_CREAM_MULTICAST_PORT));
	char str[INET_ADDRSTRLEN];

	sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sd < 0) {perror("socket()");}

	int one = 1;
	int r = setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, (char*)&one, sizeof(one));
	if (r < 0) {perror("setsockopt(SO_REUSEPORT)");}

	//binding socket
	if (bind (sd, (struct sockaddr *)&hostAddr, sizeof(hostAddr)) < 0) {
		perror("bind()");
	}
	/*End socket receiveFrom*/

	int eventLength = MyOptions::GetInt(OPTION_EVENT_LENGTH_L1);
	auto sourceL1IDs = MyOptions::GetIntPairList(OPTION_L1_DATA_SOURCE_IDS);
	char bufferl1[BUFSIZE];
	memset(bufferl1, 0, BUFSIZE);

	/*Creating dummy data to be sent*/
	uint32_t randomLength = 100000 * sizeof(int);
	char randomData[randomLength];
	memset(randomData, 1, randomLength);

	uint offsetTrigger = sizeof(l1::TRIGGER_RAW_HDR);
	char* packet = new char[BUFSIZE];
	memset(packet, 0, BUFSIZE);
	l1::L1_EVENT_RAW_HDR *hdrToBeSent = new l1::L1_EVENT_RAW_HDR;
	char *buffTrigger = new char[offsetTrigger];
	uint offsetMRP = sizeof(l1::MRP_RAW_HDR);
	char *mrpHeader = new char[offsetMRP];
	uint numTriggers;

	ssize_t result;

	while(true){

		result = recvfrom(sd, (void*) bufferl1, BUFSIZE, 0, (struct sockaddr *)&senderAddr, &senderLen);

		if (result > 0){

			memcpy(mrpHeader, bufferl1, offsetMRP);
			l1::MRP_RAW_HDR* hdrReceived = (l1::MRP_RAW_HDR*) mrpHeader;
			numTriggers = hdrReceived->numberOfTriggers;
			ip = senderAddr.sin_addr.s_addr;
			inet_ntop(AF_INET, &(ip), str, INET_ADDRSTRLEN);
			//IP address is properly read

			/***Send_to with IP address***/
			boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), str , std::to_string(MyOptions::GetInt(OPTION_CREAM_RECEIVER_PORT)));
			receiver_endpoint_ = *resolver.resolve(query);

			offsetTrigger = 0;
			while (numTriggers > 0){

				memcpy(buffTrigger, bufferl1 + sizeof(l1::MRP_RAW_HDR) + offsetTrigger, sizeof(l1::TRIGGER_RAW_HDR));
				offsetTrigger+=sizeof(l1::TRIGGER_RAW_HDR);
				l1::TRIGGER_RAW_HDR* triggerReceived = (l1::TRIGGER_RAW_HDR*) buffTrigger;

				hdrToBeSent->eventNumber = triggerReceived->eventNumber;
				hdrToBeSent->timestamp = triggerReceived->timestamp;
				hdrToBeSent->l0TriggerWord = triggerReceived->triggerTypeWord;
				hdrToBeSent->numberOf4BWords = 5;

				memcpy(packet + sizeof(l1::L1_EVENT_RAW_HDR), randomData, eventLength);

				for (auto sourceID : sourceL1IDs) {
					uint telL1 = sourceID.second;
					for (uint i=0; i<telL1; i++){
						hdrToBeSent->sourceSubID = sourceID.second;
						hdrToBeSent->sourceID = sourceID.first;
						memcpy(packet, reinterpret_cast<char*> (hdrToBeSent), sizeof(l1::L1_EVENT_RAW_HDR));
						socket_.send_to(boost::asio::buffer(packet, eventLength + sizeof(l1::L1_EVENT_RAW_HDR)), receiver_endpoint_);

						boost::this_thread::sleep(boost::posix_time::microsec(5));

						/*******Calculating rate and data sent**********/
						framesl1.fetch_add(1, std::memory_order_relaxed);
						frec =  frec + 1;
						end = boost::posix_time::microsec_clock::local_time();
						timeTaken = end - start;
						if (timeTaken.total_seconds() > 1){

							start = boost::posix_time::second_clock::local_time();
							std::cout << "Rate L1: "<< frec / 1000000 << " MHz" << std::endl;
							frec = 0;
						}
						/***********End rate and data sent***********/

					}
				}
				numTriggers--;

			}



		}/*end if result*/


	}//while
	delete[] packet;
	delete[] buffTrigger;
	delete[] mrpHeader;
	delete hdrToBeSent;
	socket_.close(ec);
	pthread_exit(NULL);
}
