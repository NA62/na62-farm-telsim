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
#define OPTION_ETH_DEVICE_NAME (char*)"ethDeviceName"
#define OPTION_L0_RECEIVER_PORT (char*)"L0Port"
#define OPTION_L1_DATA_SOURCE_IDS (char*)"L1DataSourceIDs"
#define OPTION_CREAM_RECEIVER_PORT (char*)"CREAMPort"
#define OPTION_DATA_SOURCE_IDS (char*)"L0DataSourceIDs"
#define OPTION_CREAM_MULTICAST_PORT (char*)"creamMulticastPort"

#define OPTION_MEPS_PER_BURST (char*)"NumberOfMEPsPerBurst"
#define OPTION_EVENTS_PER_MEP (char*)"EventsPerMEP"
#define OPTION_SENDER_ID (char*)"SenderID"
#define OPTION_PROCESS_NUM (char*)"ProcessNum"
#define OPTION_RECEIVER_IP (char*)"ReceiverIP"
#define OPTION_USE_PF_RING (char*)"UsePfRing"
#define OPTION_EVENT_LENGTH (char*)"BytesPerMepFragment"
#define OPTION_EVENT_LENGTH_L1 (char*)"BytesPerL1Fragment"

/****************/
#define OPTION_TIME_BASED (char*)"TimeBasedEventsGeneration[0/1]"
#define OPTION_DURATION_GENERATE_EVENTS (char*)"GenerateEventsFor(seconds)"
#define OPTION_DURATION_PAUSE (char*) "PauseAmongGeneration(seconds)"
/****************/
#define OPTION_DURATION_PAUSE_BBURST (char*)"PauseAmongFarmBurstSignal(seconds)"
#define OPTION_AUTO_BURST (char*) "FollowFarmAutoBurst"


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
				po::value<std::string>()->default_value("/performance/udptest/na62-telsim.conf"),
				"Config file for the options shown here")

		(OPTION_ETH_DEVICE_NAME,
				po::value<std::string>()->required(),
				"Name of the device to be used for receiving data")

		(OPTION_L0_RECEIVER_PORT, po::value<int>()->default_value(58913),
				"UDP-Port for L1 data reception")

		(OPTION_CREAM_RECEIVER_PORT, po::value<int>()->default_value(58915),
				"UDP-Port for L1 data reception")

		(OPTION_DATA_SOURCE_IDS, po::value<std::string>()->required(),
				"Comma separated list of all available data source IDs sending Data to L1 (all but LKr) together with the expected numbers of packets per source. The format is like following (A,B,C are sourceIDs and a,b,c are the number of expected packets per source):\n \t A:a,B:b,C:c")

		(OPTION_L1_DATA_SOURCE_IDS, po::value<std::string>()->default_value(""),
				"Comma separated list of all available data source IDs sending Data to L1 together with the expected numbers of packets per source. The format is like following (A,B,C are sourceIDs and a,b,c are the number of expected packets per source):\n \t A:a,B:b,C:c")

		(OPTION_MEPS_PER_BURST, po::value<int>()->required(),
				"Number of MEPs to be sent per burst")

		(OPTION_CREAM_MULTICAST_PORT, po::value<int>()->default_value(58914),
				"The port all L1 multicast MRPs to the CREAMs should be sent to")

		(OPTION_RECEIVER_IP, po::value<std::string>()->required(),
				"IP address where the MEPs should be sent to")

		(OPTION_USE_PF_RING,
				"If this flag is set, pf_ring will be used instead of boost (linux kernel sockets)")

		(OPTION_EVENTS_PER_MEP, po::value<int>()->default_value(10),
				"Number of events within each MEP")

		(OPTION_SENDER_ID, po::value<int>()->default_value(0),
				"ID of this process. Must be unique if using several processes.")

		(OPTION_PROCESS_NUM, po::value<int>()->default_value(1),
				"Number of parallel sending processes N. The event numbers sent by this instance N0 will be N0+i*N")

		(OPTION_EVENT_LENGTH, po::value<int>()->default_value(128),
				"Length of every MEP fragment in bytes")

		(OPTION_EVENT_LENGTH_L1, po::value<int>()->default_value(4),
				"Length of L1 data fragment in bytes")

		(OPTION_DURATION_GENERATE_EVENTS, po::value<int>()->default_value(0),
				"Generate events duration")

		(OPTION_DURATION_PAUSE, po::value<int>()->default_value(0),
				"Pause generate events duration")

		(OPTION_AUTO_BURST, po::value<int>()->default_value(2),
				"Listen BurstID changes and stop sending L0 data when active")

		(OPTION_DURATION_PAUSE_BBURST, po::value<int>()->default_value(4),
				"Pause of L0 data generation after receiving a farm burstID change")

		(OPTION_TIME_BASED, po::value<int>()->default_value(0),
				"Events generation based on time: duration time and pause")


				;

		Options::Initialize(argc, argv, desc);
	}
};
} /* namespace na62 */

#endif /* MYOPTIONS_H_ */
