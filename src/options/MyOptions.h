/*
 * MyOptions.h
 *
 *  Created on: Apr 11, 2014
 \*      Author: Jonas Kunze (kunze.jonas@gmail.com)
 */

#pragma once
#ifndef MYOPTIONS_H_
#define MYOPTIONS_H_

#include <options/Options.h>
#include <string>
#include <boost/thread.hpp>

/*
 * Available options
 */
#define OPTION_L0_RECEIVER_PORT (char*)"L0Port"
#define OPTION_DATA_SOURCE_IDS (char*)"L0DataSourceIDs"

#define OPTION_MEPS_PER_BURST (char*)"NumberOfMEPsPerBurst"
#define OPTION_EVENTS_PER_MEP (char*)"EventsPerMEP"
#define OPTION_RECEIVER_MAC (char*)"ReceiverMAC"
#define OPTION_RECEIVER_IP (char*)"ReceiverIP"

#define OPTION_USE_PF_RING (char*)"UsePfRing"

#define OPTION_EVENT_LENGTH (char*)"BytesPerMepFragment"

/*
 * Performance
 */
#define OPTION_ZMQ_IO_THREADS (char*)"zmqIoThreads"

namespace na62 {
class MyOptions: public Options {
public:
	MyOptions();
	virtual ~MyOptions();

	static void Load(int argc, char* argv[]) {
		desc.add_options()

		(OPTION_CONFIG_FILE,
				po::value<std::string>()->default_value(
						"/etc/na62-telsim.conf"),
				"Config file for the options shown here")

		(OPTION_L0_RECEIVER_PORT, po::value<int>()->default_value(58913),
				"UDP-Port for L1 data reception")

		(OPTION_DATA_SOURCE_IDS, po::value<std::string>()->required(),
				"Comma separated list of all available data source IDs sending Data to L1 (all but LKr) together with the expected numbers of packets per source. The format is like following (A,B,C are sourceIDs and a,b,c are the number of expected packets per source):\n \t A:a,B:b,C:c")

		(OPTION_MEPS_PER_BURST, po::value<int>()->required(),
				"Number of MEPs to be sent per burst")

		(OPTION_RECEIVER_MAC, po::value<std::string>()->required(),
				"MAC address where the MEPs should be sent to")

		(OPTION_RECEIVER_IP, po::value<std::string>()->required(),
				"IP address where the MEPs should be sent to")

		(OPTION_USE_PF_RING, "If this flag is set, pf_ring will be used instead of boost (linux kernel sockets)")

		(OPTION_EVENTS_PER_MEP, po::value<int>()->default_value(10), "Number of events within each MEP")

		(OPTION_EVENT_LENGTH, po::value<int>()->default_value(140), "Length of every MEP fragment")


		;

		Options::Initialize(argc, argv, desc);
	}
};
} /* namespace na62 */

#endif /* MYOPTIONS_H_ */
