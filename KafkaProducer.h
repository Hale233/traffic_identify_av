/*
 * kafkaProducer.h
 *
 *  Created on: 
 *      Author: 
 */

#ifndef KAFKAPRODUCER_H_
#define KAFKAPRODUCER_H_

#include <string>
#include <iostream>
#include <map>


extern "C"
{
	#include "rdkafka.h"
}

using namespace std;


class KafkaProducer
{
public:
	KafkaProducer(const string& b);

	~KafkaProducer();

	int KafkaConnection();

	int SendData(string& topicName, void *payload, size_t paylen);

	int MessageInQueue();

	void KafkaPoll(int interval);
	rd_kafka_topic_t* CreateTopicHandle(const string& topicName);

private:
	int partition;

	string brokers;
	char errString[512];

	rd_kafka_conf_t* config;
	rd_kafka_t* kafka;
	map < string, rd_kafka_topic_t* > topicHandleMap;
	map < string, rd_kafka_topic_t* >::iterator iter;
};



#endif /* KAFKAPRODUCER_H_ */
