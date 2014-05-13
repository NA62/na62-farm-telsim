/*
 * Sender.cpp
 *
 *  Created on: Mar 5, 2014
 \*      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#include "Sender.h"

#include <arpa/inet.h>
#include <boost/asio/io_service.hpp>
#include <l0/MEP.h>
#include <l0/MEPEvent.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include "options/MyOptions.h"
#include <socket/EthernetUtils.h>
#include <socket/PFringHandler.h>
#include <structs/Network.h>
#include <utils/Stopwatch.h>
#include <cstdbool>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace na62 {

Sender::Sender(uint sourceID, uint numberOfTelBoards, uint numberOfMEPsPerBurst) :
		sourceID_(sourceID), numberOfTelBoards_(numberOfTelBoards), numberOfMEPsPerBurst_(
				numberOfMEPsPerBurst) {
}

Sender::~Sender() {
}

void Sender::thread() {
	sendMEPs(sourceID_, 1);
}

void Sender::sendMEPs(uint8_t sourceID, uint tel62Num) {
	char* macAddr = EthernetUtils::StringToMAC(Options::GetString(OPTION_RECEIVER_MAC));
	std::string hostIP = Options::GetString(OPTION_RECEIVER_IP);

	char* packet = new char[MTU];
	for (int i = 0; i < MTU; i++) {
		packet[i] = 0;
	}

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
	uint eventsPerMEP = 10;

	l0::MEP_RAW_HDR* mep = (l0::MEP_RAW_HDR*) (packet + sizeof(struct UDP_HDR));
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
	struct l0::MEP_RAW_HDR* mep = (struct l0::MEP_RAW_HDR*) (buffer
			+ sizeof(struct UDP_HDR));
	uint32_t offset = sizeof(struct UDP_HDR) + sizeof(struct l0::MEP_RAW_HDR); // data header length

	for (uint32_t eventNum = firstEventNum;
			eventNum < firstEventNum + eventsPerMEP; eventNum++) {
		uint16_t eventLength = 140;

		if (offset + eventLength > 1500 - sizeof(struct UDP_HDR)) {
			std::cerr << "Random event size too big for MTU: " << eventLength
					<< std::endl;
			eventLength = 1500 - sizeof(struct UDP_HDR) - offset;
		}
		// Write the Event header
		l0::MEPEVENT_RAW_HDR* event = (l0::MEPEVENT_RAW_HDR*) (buffer + offset);
		event->eventLength_ = eventLength;
		event->eventNumberLSB_ = eventNum;
		event->reserved_ = 0;
		event->lastEventOfBurst_ = isLastMEPOfBurst
				&& (eventNum == firstEventNum + eventsPerMEP - 1);
		event->timestamp_ = eventNum;

		unsigned long int randomOffset = rand() % eventLength;

		memcpy(buffer + offset + sizeof(l0::MEPEVENT_RAW_HDR),
				randomData + randomOffset,
				eventLength - sizeof(l0::MEPEVENT_RAW_HDR));

		offset += eventLength;
	}

	uint16_t MEPLength = offset - sizeof(struct UDP_HDR);

	mep->firstEventNum = firstEventNum;
	mep->mepLength = MEPLength;

	struct UDP_HDR* udpHeader = (struct UDP_HDR*) buffer;

	udpHeader->setPayloadSize(MEPLength);
	udpHeader->ip.check = 0;
	udpHeader->ip.check = EthernetUtils::GenerateChecksum(
			(const char*) &udpHeader->ip, sizeof(struct iphdr));
	udpHeader->udp.check = EthernetUtils::GenerateUDPChecksum(udpHeader,
			MEPLength);

	PFringHandler::SendFrame(buffer, MEPLength + sizeof(struct UDP_HDR));

	return MEPLength + sizeof(struct UDP_HDR);
}

} /* namespace na62 */
