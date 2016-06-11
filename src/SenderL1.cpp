/*
 * SenderL1.cpp
 *
 *  Created on: May 25, 2016
 *      Author: jcalvopi
 */



#include "SenderL1.h"
#include "Sender.h"

#include <l1/L1DistributionHandler.h>
#include <l1/MRP.h>
#include <l1/MEP.h>
#include <l1/MEPFragment.h>
#include <arpa/inet.h>
#include <monitoring/IPCHandler.h>
#include <l0/MEP.h>
#include <l0/MEPFragment.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <options/Options.h>
#include "options/MyOptions.h"
#include <socket/EthernetUtils.h>
#include <socket/NetworkHandler.h>
#include <structs/Network.h>
#include <utils/Stopwatch.h>
#include <cstdbool>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include <boost/array.hpp>
#include <boost/asio.hpp>
#define BUFSIZE 65000

namespace na62 {
//namespace l1{


static int sd;
fd_set readfdsl1;
//auto sourceL1IDs = Options::GetIntPairList(OPTION_L1_DATA_SOURCE_IDS);

SenderL1::SenderL1() : sourceID_(0), eventNumber_(0), eventLength_(0), io_service_(), socket_(
				io_service_) {


	/********socket Recieve_from*******/

	struct sockaddr_in hostAddr;
	bzero(&hostAddr, sizeof(hostAddr));
	FD_ZERO(&readfdsl1);
	hostAddr.sin_family = PF_INET;
	hostAddr.sin_addr.s_addr = htonl(INADDR_ANY);//htonl(MyOptions::GetInt(OPTION_RECEIVER_IP));; //use in_addr with listen_addr or get any IP address available htonl(INADDR_ANY)
	hostAddr.sin_port = htons(MyOptions::GetInt(OPTION_CREAM_MULTICAST_PORT));

			//std::cout<<"cream " << htons(MyOptions::GetInt(OPTION_CREAM_RECEIVER_PORT))<<std::endl;
			//std::cout<<"multi "<< hostAddr.sin_port<<std::endl;
			//std::cout<<hostAddr.sin_addr.s_addr<<std::endl;

	sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sd < 0) {perror("socket()");}
	//set socket RPORT option to 1
	int one = 1;
	int r = setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, (char*)&one, sizeof(one));
	if (r < 0) {perror("setsockopt(SO_REUSEPORT)");}

	//binding socket
	if (bind (sd, (struct sockaddr *)&hostAddr, sizeof(hostAddr)) < 0) {
		perror("bind()");
	}
	eventLength_ = 4;//MyOptions::GetInt(OPTION_EVENT_LENGTH);
	sentData_=0;
}

SenderL1::~SenderL1() {
	close(sd);
}

void SenderL1::thread() {
	//sendL1MEP();
}


void *SenderL1::sendL1MEP(void *threadid) {

using boost::asio::ip::udp;
	 auto sourceL1IDs = Options::GetIntPairList(OPTION_L1_DATA_SOURCE_IDS);

	 struct sockaddr_in senderAddr;
	 socklen_t senderLen = sizeof(senderAddr);

	 char bufferl1[BUFSIZE];
	 memset(bufferl1, 0, BUFSIZE);

	 uint randomLength = 100000 * sizeof(int);
	 char randomData[randomLength];
	 memset(randomData, 1, randomLength);
	 //uint16_t offset = offsetMRP + offsetTrigger;
	 //for (unsigned int i = 0; i < randomLength / sizeof(int); i++) {
	//	 int data = rand();
	//	 memcpy(randomData + (i * sizeof(int)), &data, sizeof(int));
	 //}

	 ssize_t result;


	 //while(true){

	 result = recvfrom(sd, (void*) bufferl1, BUFSIZE, MSG_DONTWAIT, (struct sockaddr *)&senderAddr, &senderLen);
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

				/***open socket Send_to with IP address***/
				boost::system::error_code ec;
				udp::resolver resolver(io_service_);
				udp::resolver::query query(udp::v4(), str , std::to_string(MyOptions::GetInt(OPTION_CREAM_RECEIVER_PORT)));
				receiver_endpoint_ = *resolver.resolve(query);
				socket_.open(udp::v4());
				//IP address is properly read

				//const l1::MRP_RAW_HDR* hdr = (const l1::MRP_RAW_HDR*)(bufferl1);
				//uint16_t numTriggers = hdr->numberOfTriggers;

				while (numTriggers > 0){
					offsetTrigger = sizeof(l1::TRIGGER_RAW_HDR) * (numTriggers-1);
					memcpy(buffTrigger, bufferl1 + sizeof(l1::MRP_FRAME_HDR) + offsetTrigger, sizeof(l1::TRIGGER_RAW_HDR));
					//const l1::TRIGGER_RAW_HDR* hdr1 = (const l1::TRIGGER_RAW_HDR*)(bufferl1 + offsetMRP);
					l1::TRIGGER_RAW_HDR* triggerReceived = (l1::TRIGGER_RAW_HDR*) buffTrigger;

					//eventNumber_ = hdrReceived->eventNumber;
					hdrToBeSent->eventNumber = triggerReceived->eventNumber;
					hdrToBeSent->timestamp = triggerReceived->timestamp;
					hdrToBeSent->l0TriggerWord = triggerReceived->triggerTypeWord;
					hdrToBeSent->numberOf4BWords = 5;

					//memcpy(buffTrigger, bufferl1 + sizeof(l1::MRP_FRAME_HDR) + offsetTrigger, sizeof(l1::TRIGGER_RAW_HDR));

					//unsigned long int randomOffset = rand() % eventLength_;
					memcpy(packet + sizeof(l1::L1_EVENT_RAW_HDR), randomData, eventLength_);

					for (auto sourceID : sourceL1IDs) {

						//hdrToBeSent->sourceSubID = sourceID.second;
					    hdrToBeSent->sourceID = sourceID.first;
						memcpy(packet, reinterpret_cast<char*> (hdrToBeSent), sizeof(l1::L1_EVENT_RAW_HDR));

						socket_.send_to(boost::asio::buffer(packet, eventLength_ + sizeof(l1::L1_EVENT_RAW_HDR)), receiver_endpoint_);

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

	 //}


}




} /* namespace na62 */

