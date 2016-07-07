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
	int net_bind_udpl1();


	inline uint getSentL1Data() {
		return sentData_;
	}

	inline uint32_t getSentL1Frames() {
		return num_mens_;
	}

	inline uint32_t getMRP() {
		return mrps_;
	}

	inline double getL1Frec() {
		return frec_;
	}

	inline void setSentL1DataToZero(){
		sentData_ = 0;
	}

	inline bool getEndL1(){
		return endPro_;
	}

	inline bool setEndL1(){
		return endPro_ = true;
	}
	inline bool unSetEndL1(){
		return endPro_ = false;
	}

	inline bool isSetEndL1(){
		if (getEndL1()) return true;
		else  return false;
	}



private:
	uint sourceID_;
	uint32_t eventNumber_;
	uint eventLength_;

	boost::asio::io_service io_service_;
	boost::asio::ip::udp::socket socket_;
	boost::asio::ip::udp::endpoint receiver_endpoint_;
	boost::asio::ip::udp::resolver resolver_;

	uint32_t myIP_;

	double frec_;
	uint32_t num_mens_;
	uint sentData_;
	uint mrps_;
	bool endPro_;
	uint rateL1_;
	uint optRateL1_;

	boost::posix_time::time_duration timeTaken_;
	boost::posix_time::ptime end_;
	boost::posix_time::ptime start_;

	void thread();

	void sendL1MEP();


};

} /* namespace na62 */

#endif /* SENDER_H_ */


