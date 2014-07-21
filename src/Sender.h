/*
 * Sender.h
 *
 *  Created on: Mar 5, 2014
 \*      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#ifndef SENDER_H_
#define SENDER_H_

#include <boost/asio/ip/udp.hpp>
#include <sys/types.h>
#include <utils/AExecutable.h>
#include <cstdint>

namespace na62 {

class Sender: public AExecutable {
public:
	Sender(uint sourceID, uint numberOfTelBoards, uint numberOfMEPsPerBurst);
	virtual ~Sender();
private:
	uint sourceID_;
	uint numberOfTelBoards_;
	uint numberOfMEPsPerBurst_;

	boost::asio::io_service io_service_;
	boost::asio::ip::udp::socket socket_;
	boost::asio::ip::udp::endpoint receiver_endpoint_;

	void thread();

	void sendMEPs(uint8_t sourceID, uint tel62Num);
	uint16_t sendMEP(char* buffer, uint32_t firstEventNum,
			const unsigned short eventsPerMEP, uint& randomLength,
			char* randomData, bool isLastMEPOfBurst);

};

} /* namespace na62 */

#endif /* SENDER_H_ */
