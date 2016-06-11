/*
 * SenderL1.h
 *
 *  Created on: May 25, 2016
 *      Author: jcalvopi
 */



#ifndef SENDERL1_H_
#define SENDERL1_H_

#include <boost/asio/ip/udp.hpp>
#include <sys/types.h>
#include <utils/AExecutable.h>
#include <cstdint>

namespace na62 {

class SenderL1: public AExecutable {
public:
	SenderL1();
	virtual ~SenderL1();
	void sendL1MEPs();
	void *sendL1MEP(void *threadid);

	uint getSentData() {
		return sentData_;
	}
private:
	uint sourceID_;
	uint32_t eventNumber_;
	uint eventLength_;

	boost::asio::io_service io_service_;
	boost::asio::ip::udp::socket socket_;
	boost::asio::ip::udp::endpoint receiver_endpoint_;
	/***L1***/
	//boost::asio::io_service io_servicel1_;
	//boost::asio::ip::udp::socket socketl1_;
	//boost::asio::ip::udp::endpoint receiver_endpointl1_;

	//void sendL1MEP();

	uint sentData_;

	void thread();




};

} /* namespace na62 */

#endif /* SENDER_H_ */


