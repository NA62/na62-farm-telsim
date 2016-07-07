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

namespace na62 {

//auto sourceL1IDs = Options::GetIntPairList(OPTION_L1_DATA_SOURCE_IDS);

SenderL1::SenderL1() : sourceID_(0), eventNumber_(0), eventLength_(0), io_service_(), socket_(io_service_),
		resolver_(io_service_), myIP_(0), frec_(0), num_mens_(0), sentData_(0), mrps_(0), endPro_(false), rateL1_(0){

	socket_.open(boost::asio::ip::udp::v4());
	start_ = boost::posix_time::microsec_clock::local_time();
	eventLength_ = MyOptions::GetInt(OPTION_EVENT_LENGTH_L1);
	optRateL1_ = MyOptions::GetInt(OPTION_RATE_L1);
	myIP_ = EthernetUtils::GetIPOfInterface(MyOptions::GetString(OPTION_ETH_DEVICE_NAME));

	if (optRateL1_!=0){
		switch (optRateL1_) {
		case 5:
			rateL1_ = 100;
			break;
		case 15:
			rateL1_ = 1;
			break;
		default:
			break;
		}
	}


}

SenderL1::~SenderL1() {
	socket_.close();
}

void SenderL1::thread() {
	sendL1MEP();
}


void SenderL1::sendL1MEP() {


	auto sourceL1IDs = Options::GetIntPairList(OPTION_L1_DATA_SOURCE_IDS);
	/***for receiving socket***/
	int sd;
	sd = net_bind_udpl1();
	struct sockaddr_in senderAddr;
	socklen_t senderLen = sizeof(senderAddr);

	/****for send To socket****/
	in_addr_t ip;
	char str[INET_ADDRSTRLEN];

	/****Data received*****/
	char bufferl1[MTU];
	memset(bufferl1, 0, MTU);

	/*Creating dummy data to be sent*/
	uint32_t randomLength = 100000 * sizeof(int);
	char randomData[randomLength];
	memset(randomData, 1, randomLength);

	/*****To see what is inside MRP****/
	uint offsetTrigger = sizeof(l1::TRIGGER_RAW_HDR);
	char* packet = new char[MTU];
	memset(packet, 0, MTU);
	l1::L1_EVENT_RAW_HDR *hdrToBeSent = new l1::L1_EVENT_RAW_HDR;
	char *buffTrigger = new char[offsetTrigger];
	uint offsetMRP = sizeof(l1::MRP_RAW_HDR);
	char *mrpHeader = new char[offsetMRP];
	uint numTriggers;

	ssize_t result = 0;

	while(true){

		result = recvfrom(sd, (void*) bufferl1, MTU, 0, (struct sockaddr *)&senderAddr, &senderLen);

		if (result == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
				//LOG_INFO("Timeout");
				setEndL1();
			}

		}else{
			++mrps_;
			memcpy(mrpHeader, bufferl1, offsetMRP);
			l1::MRP_RAW_HDR* hdrReceived = (l1::MRP_RAW_HDR*) mrpHeader;
			numTriggers = hdrReceived->numberOfTriggers;
			ip = senderAddr.sin_addr.s_addr;
			inet_ntop(AF_INET, &(ip), str, INET_ADDRSTRLEN);
			//IP address is properly read

			/***Send_to with IP address***/
			boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), str , std::to_string(MyOptions::GetInt(OPTION_CREAM_RECEIVER_PORT)));
			receiver_endpoint_ = *resolver_.resolve(query);

			offsetTrigger = 0;
			while (numTriggers > 0){

				memcpy(buffTrigger, bufferl1 + sizeof(l1::MRP_RAW_HDR) + offsetTrigger, sizeof(l1::TRIGGER_RAW_HDR));
				offsetTrigger+=sizeof(l1::TRIGGER_RAW_HDR);
				l1::TRIGGER_RAW_HDR* triggerReceived = (l1::TRIGGER_RAW_HDR*) buffTrigger;

				hdrToBeSent->eventNumber = triggerReceived->eventNumber;
				hdrToBeSent->timestamp = triggerReceived->timestamp;
				hdrToBeSent->l0TriggerWord = triggerReceived->triggerTypeWord;
				hdrToBeSent->numberOf4BWords = 5;


				memcpy(packet + sizeof(l1::L1_EVENT_RAW_HDR), randomData, eventLength_);

				for (auto sourceID : sourceL1IDs) {
					uint telL1 = sourceID.second;
					for (uint i=0; i<telL1; i++){
						hdrToBeSent->sourceSubID = sourceID.second;
						hdrToBeSent->sourceID = sourceID.first;
						memcpy(packet, reinterpret_cast<char*> (hdrToBeSent), sizeof(l1::L1_EVENT_RAW_HDR));

						socket_.send_to(boost::asio::buffer(packet, eventLength_ + sizeof(l1::L1_EVENT_RAW_HDR)), receiver_endpoint_);

						++num_mens_;
						++frec_;

						if ((optRateL1_ > 0) && (optRateL1_ < 120)){
							boost::this_thread::sleep(boost::posix_time::microsec(rateL1_));
						}

						//boost::this_thread::sleep(boost::posix_time::microsec(1));

						/*******Calculating rate**********/
						end_ = boost::posix_time::microsec_clock::local_time();
						timeTaken_ = end_ - start_;
						if (timeTaken_.total_seconds() >= 1){

							start_ = boost::posix_time::second_clock::local_time();
							std::cout << "Rate L1: "<< getL1Frec() / 1000 << " KHz" << std::endl;
							frec_ = 0;
						}
						/***********End rate***********/


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


}

int SenderL1::net_bind_udpl1(){

	static int sd;
	struct sockaddr_in hostAddr;
	bzero(&hostAddr, sizeof(hostAddr));
	hostAddr.sin_family = AF_INET;
	//inet_pton(AF_INET, "10.194.20.9", &(hostAddr.sin_addr.s_addr));
	hostAddr.sin_addr.s_addr = myIP_;
	hostAddr.sin_port = htons(MyOptions::GetInt(OPTION_CREAM_MULTICAST_PORT));


	sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sd < 0) {perror("socket()");}

	int one = 1;
	int r = setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, (char*)&one, sizeof(one));
	if (r < 0) {perror("setsockopt(SO_REUSEPORT)");}

	struct timeval tv;

	tv.tv_sec = 2;
	tv.tv_usec = 0;

	if (setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval))==-1){
		LOG_ERROR("setting timeout Telsim request");
	}

	//binding socket
	if (bind (sd, (struct sockaddr *)&hostAddr, sizeof(hostAddr)) < 0) {
		perror("bind()");
	}

	return sd;

}


} /* namespace na62 */

