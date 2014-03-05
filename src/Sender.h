/*
 * Sender.h
 *
 *  Created on: Mar 5, 2014
 *      Author: root
 */

#ifndef SENDER_H_
#define SENDER_H_

#include <sys/types.h>
#include <utils/AExecutable.h>

namespace na62 {

class Sender: public AExecutable {
public:
	Sender(uint sourceID);
	virtual ~Sender();
private:
	uint sourceID_;

	void thread();

	void sendMEPs(int threadNum, uint8_t sourceID, uint tel62Num);
	uint16_t sendMEP(int8_t threadNum, char* buffer, uint32_t firstEventNum,
			const unsigned short eventsPerMEP, uint& randomLength,
			char* randomData, bool isLastMEPOfBurst);

};

} /* namespace na62 */

#endif /* SENDER_H_ */
