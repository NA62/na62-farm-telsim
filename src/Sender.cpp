/*
 * Sender.cpp
 *
 *  Created on: Mar 5, 2014
 \*      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "Sender.h"

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

#include <boost/array.hpp>
#include <boost/asio.hpp>

namespace na62 {

Sender::Sender(uint sourceID, uint numberOfTelBoards, uint numberOfMEPsPerBurst) :
		sourceID_(sourceID), numberOfTelBoards_(numberOfTelBoards), numberOfMEPsPerBurst_(
				numberOfMEPsPerBurst), eventLength_(0), io_service_(), socket_(
				io_service_) {

	if (!Options::Isset(OPTION_USE_PF_RING)) {
		using boost::asio::ip::udp;

		udp::resolver resolver(io_service_);
		udp::resolver::query query(udp::v4(),
				Options::GetString(OPTION_RECEIVER_IP),
				std::to_string(MyOptions::GetInt(OPTION_L0_RECEIVER_PORT)));
		receiver_endpoint_ = *resolver.resolve(query);

		socket_.open(udp::v4());
	}

	eventLength_ = MyOptions::GetInt(OPTION_EVENT_LENGTH);
}

Sender::~Sender() {
}

void Sender::thread() {
	sendMEPs(sourceID_, 1);
}

void Sender::sendMEPs(uint8_t sourceID, uint tel62Num) {
	char* macAddr = EthernetUtils::StringToMAC(
			Options::GetString(OPTION_RECEIVER_MAC));
	std::string hostIP = Options::GetString(OPTION_RECEIVER_IP);

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

	uint bursts = 1;
	uint eventsPerMEP = Options::GetInt(OPTION_EVENTS_PER_MEP);

	l0::MEP_HDR* mep = (l0::MEP_HDR*) (packet + sizeof(struct UDP_HDR));
	mep->eventCount = eventsPerMEP;
	mep->sourceID = sourceID;

	double ticksToWaitPerKByte = -1;
	uint64_t tickStart = Stopwatch::GetTicks();
//	if (vm.count(CONOPTION_TRANSMISSION_RATE)) {
//		uint64_t Bps = vm[CONOPTION_TRANSMISSION_RATE].as<double>() * 1000000000
//				/ 8;
//		mycout("Sending with " << Bps << "Gbps");
//		if (Bps > 0) {
//			ticksToWaitPerKByte = Stopwatch::GetCPUFrequency() / Bps;
//		}
//	}

	uint32_t breakCounter = 0;
	uint32_t dataSent = 0;
	for (unsigned int BurstNum = 0; BurstNum < bursts; BurstNum++) {
		if (ticksToWaitPerKByte > 0 && breakCounter++ % 100 == 0) {
			tickStart = Stopwatch::GetTicks();
			dataSent = 0;
		}
		for (unsigned int MEPNum = BurstNum * numberOfMEPsPerBurst_;
				MEPNum < numberOfMEPsPerBurst_ * (1 + BurstNum); MEPNum++) {
			bool isLastMEPOfBurst = MEPNum
					== numberOfMEPsPerBurst_ * (1 + BurstNum) - 1;
			for (uint i = 0; i < tel62Num; i++) {
				dataSent += sendMEP(packet, firstEventNum, eventsPerMEP,
						randomLength, randomData, isLastMEPOfBurst);
			}
			firstEventNum += eventsPerMEP;
		}
		if (breakCounter % 100 == 0) {
			while ((Stopwatch::GetTicks() - tickStart)
					< (dataSent * ticksToWaitPerKByte))
				;
		}
	}

	delete[] packet;
}

uint16_t Sender::sendMEP(char* buffer, uint32_t firstEventNum,
		const unsigned short eventsPerMEP, uint& randomLength, char* randomData,
		bool isLastMEPOfBurst) {

// Write the MEP header
	struct l0::MEP_HDR* mep = (struct l0::MEP_HDR*) (buffer
			+ sizeof(struct UDP_HDR));
	uint32_t offset = sizeof(struct UDP_HDR) + sizeof(struct l0::MEP_HDR); // data header length

	uint numberOfProcesses = Options::GetInt(OPTION_PROCESS_NUM);
	uint senderID = Options::GetInt(OPTION_SENDER_ID);
	for (uint32_t eventNum = firstEventNum;
			eventNum < firstEventNum + eventsPerMEP; eventNum++) {

		if (offset + eventLength_ > MTU - sizeof(struct UDP_HDR)) {
			std::cerr << "Random event size too big for MTU: " << eventLength_
					<< std::endl;
			eventLength_ = MTU - sizeof(struct UDP_HDR) - offset;
		}
		uint eventID = senderID + numberOfProcesses * eventNum;
		// Write the Event header
		l0::MEPFragment_HDR* event = (l0::MEPFragment_HDR*) (buffer + offset);
		event->eventLength_ = eventLength_;
		event->eventNumberLSB_ = eventID;
		event->reserved_ = 0;
		event->lastEventOfBurst_ = isLastMEPOfBurst
				&& (eventID == firstEventNum + eventsPerMEP - 1);
		event->timestamp_ = eventID;

		unsigned long int randomOffset = rand() % eventLength_;

		memcpy(buffer + offset + sizeof(l0::MEPFragment_HDR),
				randomData + randomOffset,
				eventLength_ - sizeof(l0::MEPFragment_HDR));

//		for (uint i = 0; i != eventLength_ - sizeof(l0::MEPFragment_HDR); i++) {
//			memset(buffer + offset + sizeof(l0::MEPFragment_HDR) + i, i/10, 1);
//		}

		offset += eventLength_;
	}

	uint16_t MEPLength = offset - sizeof(struct UDP_HDR);

	mep->firstEventNum =  senderID + numberOfProcesses * firstEventNum;
	mep->mepLength = MEPLength;

	struct UDP_HDR* udpHeader = (struct UDP_HDR*) buffer;

	udpHeader->setPayloadSize(MEPLength);
	udpHeader->ip.check = 0;
	udpHeader->ip.check = EthernetUtils::GenerateChecksum(
			(const char*) &udpHeader->ip, sizeof(struct iphdr));
	udpHeader->udp.check = EthernetUtils::GenerateUDPChecksum(udpHeader,
			MEPLength);

	if (Options::Isset(OPTION_USE_PF_RING)) {
		DataContainer c = { buffer, (uint16_t) (MEPLength
				+ sizeof(struct UDP_HDR)), false };
		NetworkHandler::AsyncSendFrame(std::move(c));
		NetworkHandler::DoSendQueuedFrames(0);
	} else {
		/*
		 * Kernel socket version
		 */
		socket_.send_to(
				boost::asio::buffer(buffer + sizeof(UDP_HDR), MEPLength),
				receiver_endpoint_);
	}

	return MEPLength + sizeof(struct UDP_HDR);
}

} /* namespace na62 */
