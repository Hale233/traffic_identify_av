/*
 * KafkaProducer.cpp
 *
 *  Created on: 
 *      Author: 
 */
#include "KafkaProducer.h"

KafkaProducer::KafkaProducer(const string& b):brokers(b)
{
	partition = RD_KAFKA_PARTITION_UA;
	//partition = 0;
};

int KafkaProducer::KafkaConnection()
{
	config = rd_kafka_conf_new();
	rd_kafka_conf_set(config, "queue.buffering.max.messages", "1000000", NULL, 0);
	rd_kafka_conf_set(config, "topic.metadata.refresh.interval.ms", "600000", NULL, 0);

	if (!(kafka = rd_kafka_new(RD_KAFKA_PRODUCER, config, errString, sizeof(errString))))
	{
		return -1;
	}

	if (rd_kafka_brokers_add(kafka, brokers.c_str()) == 0)
	{
		return -2;
	}

	return 0;
}

KafkaProducer::~KafkaProducer()
{
	rd_kafka_destroy(kafka);
	for(iter = topicHandleMap.begin(); iter!=topicHandleMap.end(); ++iter)
	{
		rd_kafka_topic_destroy(iter->second);
	}
	rd_kafka_wait_destroyed(5000);
}

rd_kafka_topic_t* KafkaProducer::CreateTopicHandle(const string& topicName)
{
	if(!topicHandleMap.count(topicName))
	{
		rd_kafka_topic_conf_t* config = rd_kafka_topic_conf_new();
		rd_kafka_topic_t* rkt = rd_kafka_topic_new(kafka, topicName.c_str(), config);
		topicHandleMap[topicName] = rkt;
	}
	return topicHandleMap[topicName];
}

int KafkaProducer::SendData(string& topicName, void *payload, size_t paylen)
{
	rd_kafka_topic_t* currentTopicHandle = topicHandleMap[topicName];
		
	int status = rd_kafka_produce(currentTopicHandle, partition, RD_KAFKA_MSG_F_COPY, payload,
			paylen, NULL, 0, NULL);
	return status;

}

int KafkaProducer::MessageInQueue()
{
	return rd_kafka_outq_len(kafka);
}

void KafkaProducer::KafkaPoll(int interval)
{
	rd_kafka_poll(kafka, interval);
}




