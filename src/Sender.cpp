/*
 * Sender.cpp
 *
 *  Created on: Mar 5, 2014
 \*      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "Sender.h"
#include "SenderL1.h"

#include <fcntl.h>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <l0/MEP.h>
#include <l0/MEPFragment.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
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
#include <utility>
//#include <chrono>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/array.hpp>
#include <boost/chrono.hpp>
#include <boost/asio.hpp>

#define BUFSIZE 65000
#define BUFI 128




namespace na62 {




Sender::Sender(uint sourceID, uint numberOfTelBoards, uint numberOfMEPsPerBurst) :
				sourceID_(sourceID), numberOfTelBoards_(numberOfTelBoards), numberOfMEPsPerBurst_(
						numberOfMEPsPerBurst), eventLength_(0), io_service_(), socket_(
								io_service_), burstNum_(0), sentData_(0), autoburst_(0), sock_(0), timebased_(0), num_mens_(0) {


	using boost::asio::ip::udp;

	udp::resolver resolver(io_service_);
	udp::resolver::query query(udp::v4(), MyOptions::GetString(OPTION_RECEIVER_IP), std::to_string(MyOptions::GetInt(OPTION_L0_RECEIVER_PORT)));
	receiver_endpoint_ = *resolver.resolve(query);
	socket_.open(udp::v4());
	durationSeconds_ = MyOptions::GetInt(OPTION_DURATION_GENERATE_EVENTS);
	pauseSeconds_ = MyOptions::GetInt(OPTION_DURATION_PAUSE);
	eventLength_ = MyOptions::GetInt(OPTION_EVENT_LENGTH);
	autoburst_ = MyOptions::GetInt(OPTION_AUTO_BURST);
	timebased_ = MyOptions::GetInt(OPTION_TIME_BASED);
	start_ = boost::posix_time::microsec_clock::local_time();


}

Sender::~Sender() {
}

void Sender::thread() {


	sendMEPs(sourceID_, numberOfTelBoards_);
}

void Sender::sendMEPs(uint8_t sourceID, uint tel62Num) {
	char* macAddr = EthernetUtils::StringToMAC(
			MyOptions::GetString(OPTION_RECEIVER_MAC));
	std::string hostIP = MyOptions::GetString(OPTION_RECEIVER_IP);

	char* packet = new char[MTU];
	memset(packet, 0, MTU);


	EthernetUtils::GenerateUDP(packet, macAddr, inet_addr(hostIP.c_str()), 6666,
			MyOptions::GetInt(OPTION_L0_RECEIVER_PORT));

	uint32_t firstEventNum = 0;

	uint randomLength = 100000 * sizeof(int);
	char randomData[randomLength];
	for (unsigned int i = 0; i < randomLength / sizeof(int); i++) {
		int data = rand();
		memcpy(randomData + (i * sizeof(int)), &data, sizeof(int));
	}


	uint eventsPerMEP = Options::GetInt(OPTION_EVENTS_PER_MEP);

	l0::MEP_HDR* mep = (l0::MEP_HDR*) (packet + sizeof(struct UDP_HDR));
	mep->eventCount = eventsPerMEP;
	mep->sourceID = sourceID;

	if(autoburst_ != 1 && timebased_ != 1){

		uint bursts = 1;
		for (unsigned int BurstNum = 0; BurstNum < bursts; BurstNum++) {

			for (unsigned int MEPNum = BurstNum * numberOfMEPsPerBurst_;
					MEPNum < numberOfMEPsPerBurst_ * (1 + BurstNum); MEPNum++) {
				bool isLastMEPOfBurst = MEPNum
						== numberOfMEPsPerBurst_ * (1 + BurstNum) - 1;
				for (uint i = 0; i < tel62Num; i++) {

					//std::cout << "sendMEPs for source ID " << (int) sourceID_ <<":"<< i << std::endl;
					sentData_ += sendMEP(packet, firstEventNum, eventsPerMEP,
							randomLength, randomData, isLastMEPOfBurst);
				}
				firstEventNum += eventsPerMEP;
			}

		}

		delete[] packet;


	}if(autoburst_ == 1){



		struct sockaddr_in senderAddr;
		socklen_t senderLen;
		char buff[BUFI];
		int sock = net_bind_udp();
		//bool chBurst = false;

		while (true){

			ssize_t res = recvfrom(sock, (void*) buff, BUFI, MSG_DONTWAIT, (struct sockaddr *)&senderAddr, &senderLen);
			if (res > 0){
				//std::cout<< "Stop sending L0 data" << std::endl;
				break;
				//chBurst = true;
			}

			for (uint i = 0; i < tel62Num; i++) {

				sentData_ += sendMEP(packet, firstEventNum, eventsPerMEP,
						randomLength, randomData, 0);
			}
			firstEventNum += eventsPerMEP;

		}
		//After breaking the loop suddenly, let's send last
		for (uint i = 0; i < tel62Num; i++) {
			sentData_ += sendMEP(packet, firstEventNum, eventsPerMEP,
					randomLength, randomData, 1);
		}
		close(sock);
		firstEventNum = 0;
		delete[] packet;


	}if(timebased_ == 1 && autoburst_ != 1){

		time_t start = time(0);
		time_t timeLeft = (time_t) durationSeconds_;

		while ((timeLeft > 0))
		{
			time_t end = time(0);
			time_t timeTaken = end - start;
			timeLeft = durationSeconds_ - timeTaken;

			for (uint i = 0; i < tel62Num; i++) {

				sentData_ += sendMEP(packet, firstEventNum, eventsPerMEP,
						randomLength, randomData, 0);
			}
			firstEventNum += eventsPerMEP;

		}
		//After breaking the loop suddenly, let's send last
		for (uint i = 0; i < tel62Num; i++) {
			sentData_ += sendMEP(packet, firstEventNum, eventsPerMEP,
					randomLength, randomData, 1);
		}

		firstEventNum = 0;
		delete[] packet;

	}


}

uint16_t Sender::sendMEP(char* buffer, uint32_t firstEventNum,
		const unsigned short eventsPerMEP, uint& randomLength, char* randomData,
		bool isLastMEPOfBurst) {

	boost::posix_time::time_duration timeTaken;
	boost::posix_time::ptime end;



	// Write the MEP header
	struct l0::MEP_HDR* mep = (struct l0::MEP_HDR*) (buffer + sizeof(struct UDP_HDR));
	uint32_t offset = sizeof(struct UDP_HDR) + sizeof(struct l0::MEP_HDR); // data header length

	uint numberOfProcesses = Options::GetInt(OPTION_PROCESS_NUM);
	uint senderID = Options::GetInt(OPTION_SENDER_ID);
	for (uint32_t eventNum = firstEventNum; eventNum < firstEventNum + eventsPerMEP; eventNum++) {

		if (offset + eventLength_ > MTU - sizeof(struct UDP_HDR)) {
			std::cout << "Random event size too big for MTU: " << eventLength_
					<< std::endl;
			eventLength_ = MTU - sizeof(struct UDP_HDR) - offset;
		}
		uint eventID = senderID + numberOfProcesses * eventNum;
		// Write the Event header
		l0::MEPFragment_HDR* event = (l0::MEPFragment_HDR*) (buffer + offset);
		event->eventLength_ = eventLength_;
		event->eventNumberLSB_ = eventID;
		//std::cout<<eventID<<std::endl;
		event->reserved_ = 0;
		event->lastEventOfBurst_ = isLastMEPOfBurst && (eventID == firstEventNum + eventsPerMEP - 1);
		event->timestamp_ = eventID;

		unsigned long int randomOffset = rand() % eventLength_;

		memcpy(buffer + offset + sizeof(l0::MEPFragment_HDR),
				randomData + randomOffset,
				eventLength_ - sizeof(l0::MEPFragment_HDR));


		offset += eventLength_;
	}

	uint16_t MEPLength = offset - sizeof(struct UDP_HDR);

	mep->firstEventNum =  senderID + numberOfProcesses * firstEventNum;
	mep->mepLength = MEPLength;


	socket_.send_to(boost::asio::buffer(buffer + sizeof(UDP_HDR), MEPLength),receiver_endpoint_);
	num_mens_ = num_mens_ + 1;

	//for (int i=0; i < 1; i++){}

	//boost::this_thread::sleep(boost::posix_time::microsec(1));

	end = boost::posix_time::microsec_clock::local_time();
	timeTaken = end - start_;
				if (timeTaken.total_seconds() > 1){

							start_ = boost::posix_time::second_clock::local_time();
							std::cout << "MEPs enviados: " << num_mens_ << std::endl;
							//std::cout << "Rate: "<< num_mens / 1000000 << " MHz" << std::endl;
							//fs << num_msgs_sec << "\n" << std::flush;
							//num_mens = 0;
				}

	return MEPLength + sizeof(struct UDP_HDR);
}

int Sender::net_bind_udp()
{
	struct sockaddr_in hostAddr;
	bzero(&hostAddr, sizeof(hostAddr));
	hostAddr.sin_family = PF_INET;
	//inet_pton(AF_INET, "137.138.104.155", &(hostAddr.sin_addr.s_addr));
	hostAddr.sin_addr.s_addr = htonl(INADDR_ANY); //use in_addr with listen_addr or get any IP address available htonl(INADDR_ANY)
	hostAddr.sin_port = htons(55555);//MyOptions::GetInt(OPTION_L0_RECEIVER_PORT));

	sock_ = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock_ < 0) {
		perror("socket()");
	}

	int one = 1;
	int r = setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, (char*)&one,
			   sizeof(one));
	if (r < 0) {
		perror("setsockopt(SO_REUSEADDR)");
	}



	int r1 = setsockopt(sock_, SOL_SOCKET, SO_REUSEPORT, (char*)&one, sizeof(one));
	if (r1 < 0) {
		perror("setsockopt(SO_REUSEPORT)");

	}

	int n = 128;
	if (setsockopt(sock_, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n)) == -1) {
				perror("setting buffer");
			}


	//if (setsockopt(socket_, SOL_SOCKET, SO_BINDTODEVICE, deviceName.c_str(), deviceName.length()) == -1) {
	//				perror("SO_BINDTODEVICE");
	//				close(socket_);
	//				exit(EXIT_FAILURE);
	//}

	if (bind (sock_, (struct sockaddr *)&hostAddr, sizeof(hostAddr)) < 0) {
		perror("bind()");
	}
	/* Bind to device */




	return sock_;


}


} /* namespace na62 */
