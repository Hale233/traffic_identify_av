/*在使用规则判断、ACK判断、Burst下行判断的基础上加入burst上行特征规则判断
其中burst特征为上下行块的负载与包数
通过块个数、负载大小、包数三个阈值进行burst特征判断
2021.12.6新加入记录拥塞窗口、流负载、下行块时间间隔特征
2021.12.7修改配置项名称以及修改数组大小值通过配置项确定
2021.12.10新增输出字段流创建与淘汰时间
2022.10.10增加视频指纹提取功能
2022.11.10增加过滤重传包的功能
2022.12.29增加lstf视频内容识别功能
2023.04.28指纹识别增加tf-idf全局概率判断
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stream.h>
#include <sys/time.h>
#include <cmath>
#include <math.h>
#include <dlfcn.h>
#include <MESA/MESA_prof_load.h>
#include <MESA_handle_logger.h>
#include <MESA/field_stat2.h>
#include<fstream>
#include<sstream>
#include<vector>
#include<cstring> 
#include "traffic_identify_av.h"
using namespace std;
traffic_identify_parameter traffic_identify_para;
//static const char* g_stream_type[TYPE_NUM] ={"TCP","UDP","SSL"};

vector<word_node> word_node_vector_creat(string video_id,float trans_prob)
{
	vector<word_node> tmp_word_array;
	word_node cur_word_node;
	cur_word_node.trans_prob=trans_prob;
	cur_word_node.video_id=video_id;
	tmp_word_array.push_back(cur_word_node);
	return tmp_word_array;
}

word_node word_node_creat(string video_id,float trans_prob)
{
	word_node cur_word_node;
	cur_word_node.trans_prob=trans_prob;
	cur_word_node.video_id=video_id;
	return cur_word_node;
}

/*块大小转换为符号下标*/
int chunk2symbol(int chunk)
{
	int symbol=0;
	if (chunk >= traffic_identify_para.video_chunk_size_max)
		symbol=traffic_identify_para.symbol_num-1;
	else if (chunk <= traffic_identify_para.video_chunk_size_min)
		symbol=0;
	else
		symbol=(chunk-traffic_identify_para.video_chunk_size_min)/traffic_identify_para.symbol_range;
	return symbol;
}

void word_dictionary_init()
{
	traffic_identify_para.word_dictionary_local.clear();
	traffic_identify_para.word_dictionary_global.clear();
	//读取离线指纹文件，并存入到fileArray中
	ifstream inFile(traffic_identify_para.video_fingerprint_dataset_file);
	string lineStr;
	vector<vector<string> > fileArray;
	while (getline(inFile, lineStr)) 
	{
		stringstream ss(lineStr);
		string str;
		vector<string> lineArray;
		while (getline(ss, str, ','))
		{
			lineArray.push_back(str);
		}
		fileArray.push_back(lineArray);
	}
	//遍历fileArray中的每一个视频指纹
	//符号表范围
	traffic_identify_para.symbol_range=(traffic_identify_para.video_chunk_size_max-traffic_identify_para.video_chunk_size_min)/traffic_identify_para.symbol_num;
	for (int i=0;i<fileArray.size();i++)
	{	
		if (fileArray[i][2]!="video")
			continue;
		string video_id=fileArray[i][0];//指纹ID
		stringstream fingerprint_string(fileArray[i][4]);
		//cout<<fileArray[i][4]<<endl;
		string finger_element_string;
		vector<int> finger_array;//指纹序列
		vector<int> word_gen_array;//用于词生成
		int tmp_chunk_size;
		int chunk;
		int bin_index_cur;
		string cur_word;
		//遍历指纹中的每一个元素记录到finger_array中
		while (getline(fingerprint_string, finger_element_string, '/'))
		{
			if (finger_element_string=="")
			{
				continue;
			}
			//cout<<tmp_chunk_size<<endl;
			tmp_chunk_size=stoi(finger_element_string);
			//去除小块
			if (tmp_chunk_size>traffic_identify_para.video_chunk_size_min)
				finger_array.push_back(tmp_chunk_size);
		}
		//去除比长词短的指纹
		if (finger_array.size()<traffic_identify_para.word_len_long)
			continue;
		//遍历finger_array中的每一个元素生成词典,长词
		//一次的转移概率
		float cur_trans_prob=1.0/(float)(finger_array.size()-traffic_identify_para.word_len_long+1);
		int word_count_long=0;
		for (int j=0;j<finger_array.size();j++)
		{
			chunk=finger_array[j];
			//得到每一个元素的下标
			bin_index_cur=chunk2symbol(chunk);
			//生成词cur_word
			word_gen_array.push_back(bin_index_cur);
			if (word_gen_array.size()<traffic_identify_para.word_len_long)
				continue;
			cur_word="";
			for (int k=0;k<word_gen_array.size();k++)
			{
				cur_word +=to_string(word_gen_array[k])+'-';
			}
			//cout<<cur_word<<endl;
			word_gen_array.erase(word_gen_array.begin());
			//将词插入到局部概率词典中
			if (traffic_identify_para.word_dictionary_local.find(cur_word)!= traffic_identify_para.word_dictionary_local.end())
			{	
				if(traffic_identify_para.word_dictionary_local[cur_word][0].video_id==video_id)
				{
					traffic_identify_para.word_dictionary_local[cur_word][0].trans_prob +=cur_trans_prob;
				}
				else
				{
					traffic_identify_para.word_dictionary_local[cur_word].insert(traffic_identify_para.word_dictionary_local[cur_word].begin(),word_node_creat(video_id,cur_trans_prob));
				}
			}
			else
			{
				traffic_identify_para.word_dictionary_local[cur_word] = word_node_vector_creat(video_id,cur_trans_prob);
				//traffic_identify_para.word_dictionary_local.emplace(cur_word,word_node_vector_creat(video_id,cur_trans_prob));
				//cout<<cur_trans_prob<<video_id<<endl;
			}
			//将词插入到全局概率词典中
			if (traffic_identify_para.word_dictionary_global.find(cur_word)!= traffic_identify_para.word_dictionary_global.end())
			{	
				traffic_identify_para.word_dictionary_global[cur_word] += 1;
			}
			else
			{
				traffic_identify_para.word_dictionary_global[cur_word] = 1;
			}
			word_count_long ++;
		}
		traffic_identify_para.word_count_long=word_count_long;
		/*
		//计算全局概率表
		for (auto& long_word_element: traffic_identify_para.word_dictionary_global)
		{
			float prob=log(word_count_long/(long_word_element.second+1));
			traffic_identify_para.word_dictionary_global[long_word_element.first]=prob;
		}
		*/
		//短词
		word_gen_array.clear();
		int word_count_short=0;
		for (int j2=0;j2<finger_array.size();j2++)
		{
			chunk=finger_array[j2];
			//得到每一个元素的下标
			bin_index_cur=chunk2symbol(chunk);
			//生成词cur_word
			word_gen_array.push_back(bin_index_cur);
			if (word_gen_array.size()<traffic_identify_para.word_len_short)
				continue;
			cur_word="";
			for (int k2=0;k2<word_gen_array.size();k2++)
			{
				cur_word +=to_string(word_gen_array[k2])+'-';
			}
			//cout<<cur_word<<endl;
			word_gen_array.erase(word_gen_array.begin());
			//将词插入到词典中
			if (traffic_identify_para.word_dictionary_local.find(cur_word)!= traffic_identify_para.word_dictionary_local.end())
			{
				if(traffic_identify_para.word_dictionary_local[cur_word][0].video_id==video_id)
				{
					traffic_identify_para.word_dictionary_local[cur_word][0].trans_prob +=cur_trans_prob;
				}
				else
				{
					traffic_identify_para.word_dictionary_local[cur_word].insert(traffic_identify_para.word_dictionary_local[cur_word].begin(),word_node_creat(video_id,cur_trans_prob));
				}
			}
			else
			{
				traffic_identify_para.word_dictionary_local[cur_word] = word_node_vector_creat(video_id,cur_trans_prob);
				//traffic_identify_para.word_dictionary_local.emplace(cur_word,tmp_word_array);
			}
			//将词插入到全局概率词典中
			if (traffic_identify_para.word_dictionary_global.find(cur_word)!= traffic_identify_para.word_dictionary_global.end())
			{	
				traffic_identify_para.word_dictionary_global[cur_word] += 1;
			}
			else
			{
				traffic_identify_para.word_dictionary_global[cur_word] = 1;
			}
			word_count_short ++;
		}
		traffic_identify_para.word_count_short=word_count_short;
	}
}

int traffic_identify_para_read_main_conf(char* filename)
{
	char log_filename[256];
	char video_fingerprint_dataset_file_[512];
	short log_level;

	MESA_load_profile_short_def(filename, "LOG", "log_level", &log_level, 10);
	MESA_load_profile_string_def(filename, "LOG", "log_path", log_filename, sizeof(log_filename),"./tilog/traffic_identify_av_log");
	MESA_load_profile_uint_def(filename, "ACK", "ack_list_len", &traffic_identify_para.ack_list_len,15);
	MESA_load_profile_uint_def(filename, "ACK", "time_win_size", &traffic_identify_para.time_win_size, 3);
	MESA_load_profile_uint_def(filename, "ACK", "ack_payload_threshlod", &traffic_identify_para.ack_payload_threshlod, 100000);
	MESA_load_profile_uint_def(filename, "ACK", "ack_paknum_threshlod", &traffic_identify_para.ack_paknum_threshlod, 30);
	
	MESA_load_profile_uint_def(filename, "KAFKA", "use_kafka", &traffic_identify_para.use_kafka,0);
	MESA_load_profile_uint_def(filename, "KAFKA", "send_kafka_flag", &traffic_identify_para.send_kafka_flag,1);
	MESA_load_profile_uint_def(filename, "KAFKA", "kafka_output_stream_state", &traffic_identify_para.kafka_output_stream_state,1);
	MESA_load_profile_uint_def(filename, "KAFKA", "kafka_output_feature_state", &traffic_identify_para.kafka_output_feature_state, 1);	

	MESA_load_profile_short_def(filename,"RULE", "identifier_type", &traffic_identify_para.identifier_type, 1);	
	MESA_load_profile_uint_def(filename, "RULE", "min_bytes", &traffic_identify_para.min_bytes,512);	
	MESA_load_profile_uint_def(filename, "RULE", "total_bytes", &traffic_identify_para.total_bytes, 500*1024);	
	MESA_load_profile_uint_def(filename, "RULE", "duration", &traffic_identify_para.duration, 10);	
	MESA_load_profile_uint_def(filename, "RULE", "avg_pkt_len", &traffic_identify_para.avg_pkt_len, 1000);	
	MESA_load_profile_uint_def(filename, "RULE", "Bps", &traffic_identify_para.Bps, 100);	
	MESA_load_profile_uint_def(filename, "RULE", "min_pktsnum", &traffic_identify_para.min_pktsnum, 50);

	MESA_load_profile_short_def(filename,"CSV", "csv_record_flag", &traffic_identify_para.csv_record_flag, 0);
	MESA_load_profile_uint_def(filename, "CSV", "csv_output_state", &traffic_identify_para.csv_output_state, 0);
	
	MESA_load_profile_uint_def(filename, "BURST", "burst_interval", &traffic_identify_para.burst_interval,50);
	MESA_load_profile_uint_def(filename, "BURST", "burst_payload_threshlod", &traffic_identify_para.burst_payload_threshlod,8000);
	MESA_load_profile_uint_def(filename, "BURST", "burst_paknum_threshlod", &traffic_identify_para.burst_paknum_threshlod,50);
	MESA_load_profile_uint_def(filename, "BURST", "burst_chunkcount_threshlod", &traffic_identify_para.burst_chunkcount_threshlod,3);
	MESA_load_profile_uint_def(filename, "BURST", "burst_list_len", &traffic_identify_para.burst_list_len,20);
	MESA_load_profile_uint_def(filename, "BURST", "burst_feature_output_chunk_count", &traffic_identify_para.burst_feature_output_chunk_count,15);

	MESA_load_profile_uint_def(filename, "MODE", "run_mode", &traffic_identify_para.run_mode, 1);

	MESA_load_profile_string_def(filename, "FINGER", "video_fingerprint_dataset_file", traffic_identify_para.video_fingerprint_dataset_file, sizeof(video_fingerprint_dataset_file_),"./ticonf/finger_dataset.csv");
	MESA_load_profile_int_def(filename, "FINGER", "video_chunk_size_max", &traffic_identify_para.video_chunk_size_max, 2200000);
	MESA_load_profile_int_def(filename, "FINGER", "video_chunk_size_min", &traffic_identify_para.video_chunk_size_min, 700000);
	MESA_load_profile_int_def(filename, "FINGER", "symbol_num", &traffic_identify_para.symbol_num, 4000);
	MESA_load_profile_int_def(filename, "FINGER", "word_len_long", &traffic_identify_para.word_len_long, 6);
	MESA_load_profile_int_def(filename, "FINGER", "word_len_short", &traffic_identify_para.word_len_short, 2);
	MESA_load_profile_int_def(filename, "FINGER", "word_num_long", &traffic_identify_para.word_num_long, 1);
	MESA_load_profile_int_def(filename, "FINGER", "word_num_short", &traffic_identify_para.word_num_short, 2);

	traffic_identify_para.log_handle = MESA_create_runtime_log_handle(log_filename,log_level);

	if(traffic_identify_para.log_handle == NULL)
	{
		printf("traffic_identify_ML.so get log handle error!\n");
		return -1;
	}

	if (traffic_identify_para.use_kafka==1)
	{
		if(traffic_identify_para.send_kafka_flag)
		{	
			if(MESA_load_profile_string_nodef(filename, "KAFKA", "kafka_brokers", traffic_identify_para.kafka_brokers, sizeof(traffic_identify_para.kafka_brokers)) < 0)
			{
				printf("get [NETWORK]KafkaBrokers error.\n");
				return -1;	
			}
			printf("%s \n",traffic_identify_para.kafka_brokers);
		}

		traffic_identify_para.kafka_producer = new KafkaProducer(traffic_identify_para.kafka_brokers);
		if(NULL==traffic_identify_para.kafka_producer)
		{
			printf("KafkaProducer error.\n");
			return -1;	
		}
		if(0!=traffic_identify_para.kafka_producer->KafkaConnection())
		{	
			printf("KafkaConnection %s error.\n", traffic_identify_para.kafka_brokers);
			MESA_handle_runtime_log(traffic_identify_para.log_handle, RLOG_LV_FATAL, TRAFFIC_IDENTIFY_AV, 
									"{%s:%d} KafkaConnection %s error.", 
									__FILE__,__LINE__, traffic_identify_para.kafka_brokers);
		}
		else
		{
			printf("KafkaConnection %s succ.\n", traffic_identify_para.kafka_brokers);
			MESA_handle_runtime_log(traffic_identify_para.log_handle, RLOG_LV_FATAL, TRAFFIC_IDENTIFY_AV, 
									"{%s:%d} KafkaConnection %s succ.", 
									__FILE__,__LINE__, traffic_identify_para.kafka_brokers);
		}
		MESA_load_profile_string_def(filename, "KAFKA", "kafka_topic", traffic_identify_para.topic_name, sizeof(traffic_identify_para.topic_name),"TRAFFIC_IDENTIFY");
		if((traffic_identify_para.kafka_producer->CreateTopicHandle(traffic_identify_para.topic_name)) == NULL)
		{
			printf("Kafka CreateTopicHandle %s failed.", traffic_identify_para.topic_name);
			return -1;
		}
		MESA_handle_runtime_log(traffic_identify_para.log_handle, RLOG_LV_FATAL, TRAFFIC_IDENTIFY_AV, 
									"{%s:%d} Kafka CreateTopicHandle %s succ.", 
									__FILE__,__LINE__, traffic_identify_para.topic_name);
	}
	word_dictionary_init();
	return 0;
}

int init_pme(void** param, int thread_seq ,struct streaminfo *a_stream,stream_type a_stream_type)
{
	traffic_identify_pmeinfo* identifier_pme=(traffic_identify_pmeinfo*) *param;
	struct timeval cur_time;
	if(identifier_pme!=NULL)
		return -1;

	identifier_pme=(traffic_identify_pmeinfo*)dictator_malloc(thread_seq, sizeof(traffic_identify_pmeinfo));
	if (identifier_pme==NULL)
	{
		return -1;
	}
	memset(identifier_pme,0,sizeof(traffic_identify_pmeinfo));

	time_t t;
	t=time(NULL);
	identifier_pme->found_time=int(t);

	identifier_pme->payload_cnt=0;
	identifier_pme->c2s_payload_max=0;
	identifier_pme->s2c_payload_max=0;
	identifier_pme->win_c2s_max=0;
	identifier_pme->win_c2s_min=65530;
	identifier_pme->win_c2s_sum=0;
	identifier_pme->win_s2c_max=0;
	identifier_pme->win_s2c_min=65530;
	identifier_pme->win_s2c_sum=0;

	identifier_pme->ack_count=0;
	identifier_pme->current_ack_num=0;
	identifier_pme->video_id="";
	/*	包到达时间间隔相关
	identifier_pme->time_interval_max_c2s=0;
	identifier_pme->time_interval_max_s2c=0;
	identifier_pme->time_interval_min_c2s=9999999;
	identifier_pme->time_interval_min_s2c=9999999;
	*/

	identifier_pme->ACK_label=0;
	identifier_pme->SSL_flag=0;
	identifier_pme->burst_label=0;

	identifier_pme->ML_ACK_labeled_flag=0;

	if (traffic_identify_para.run_mode==0)
	{
		get_rawpkt_opt_from_streaminfo(a_stream, RAW_PKT_GET_TIMESTAMP, &cur_time);
	}
	else
	{
		gettimeofday(&cur_time, NULL);
	}
	identifier_pme->pre_time_s2c=cur_time.tv_sec*1000 + cur_time.tv_usec/1000;
	identifier_pme->pre_time_c2s=cur_time.tv_sec*1000 + cur_time.tv_usec/1000;

	identifier_pme->burst_chunk_count=0;
	if(NULL == identifier_pme->cjson_obj)
	{
		identifier_pme->cjson_obj = cJSON_CreateObject();
	}

	identifier_pme->payload_burst_list_s2c=(long*)dictator_malloc(thread_seq,sizeof(long)*traffic_identify_para.burst_list_len);
	memset(identifier_pme->payload_burst_list_s2c,0,sizeof(long)*traffic_identify_para.burst_list_len);
	identifier_pme->paknum_burst_list_s2c=(int*)dictator_malloc(thread_seq,sizeof(int)*traffic_identify_para.burst_list_len);
	memset(identifier_pme->paknum_burst_list_s2c,0,sizeof(int)*traffic_identify_para.burst_list_len);
	identifier_pme->payload_burst_list_c2s=(long*)dictator_malloc(thread_seq,sizeof(long)*traffic_identify_para.burst_list_len);
	memset(identifier_pme->payload_burst_list_c2s,0,sizeof(long)*traffic_identify_para.burst_list_len);
	identifier_pme->paknum_burst_list_c2s=(int*)dictator_malloc(thread_seq,sizeof(int)*traffic_identify_para.burst_list_len);
	memset(identifier_pme->paknum_burst_list_c2s,0,sizeof(int)*traffic_identify_para.burst_list_len);
	identifier_pme->burst_time_interval_s2c=(long*)dictator_malloc(thread_seq,sizeof(long)*traffic_identify_para.burst_list_len);
	memset(identifier_pme->burst_time_interval_s2c,0,sizeof(long)*traffic_identify_para.burst_list_len);
	identifier_pme->ack_list=(ack_str*)dictator_malloc(thread_seq,sizeof(ack_str)*traffic_identify_para.ack_list_len);
	memset(identifier_pme->ack_list,0,sizeof(ack_str)*traffic_identify_para.ack_list_len);
	
	*param=identifier_pme;
	return 0;
}

void release_pme(void** param, int thread_seq)
{
	traffic_identify_pmeinfo* identifier_pme = (traffic_identify_pmeinfo*)*param;

	if(NULL != identifier_pme)
	{
		if(identifier_pme->cjson_obj)
		{
			cJSON_Delete(identifier_pme->cjson_obj); 
			identifier_pme->cjson_obj = NULL;		
		}
		if(NULL != identifier_pme->paknum_burst_list_s2c)
		{
			dictator_free(thread_seq, identifier_pme->paknum_burst_list_s2c);
			identifier_pme->paknum_burst_list_s2c = NULL;
		}
		if(NULL != identifier_pme->payload_burst_list_s2c)
		{
			dictator_free(thread_seq, identifier_pme->payload_burst_list_s2c);
			identifier_pme->payload_burst_list_s2c = NULL;
		}
		if(NULL != identifier_pme->paknum_burst_list_c2s)
		{
			dictator_free(thread_seq, identifier_pme->paknum_burst_list_c2s);
			identifier_pme->paknum_burst_list_c2s = NULL;
		}
		if(NULL != identifier_pme->payload_burst_list_c2s)
		{
			dictator_free(thread_seq, identifier_pme->payload_burst_list_c2s);
			identifier_pme->payload_burst_list_c2s = NULL;
		}
		if(NULL != identifier_pme->burst_time_interval_s2c)
		{
			dictator_free(thread_seq, identifier_pme->burst_time_interval_s2c);
			identifier_pme->burst_time_interval_s2c = NULL;
		}
		if(NULL != identifier_pme->ack_list)
		{
			dictator_free(thread_seq, identifier_pme->ack_list);
			identifier_pme->ack_list = NULL;
		}
		dictator_free(thread_seq, identifier_pme);
		identifier_pme = NULL;
	}   
}

void send_kafka(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet,stream_type a_stream_type)
{
	traffic_identify_pmeinfo* identifier_pme = (traffic_identify_pmeinfo*)*pme;	  	

	char* out = NULL;
	uint32_t out_len = 0;
	out = cJSON_PrintUnformatted(identifier_pme->cjson_obj);
	//out=identifier_pme->SNI;
	out_len = strlen(out);

	string topic = (string)traffic_identify_para.topic_name;	
	
	/*send to kafka*/
	if (traffic_identify_para.send_kafka_flag==0)
		return;
	
	switch(traffic_identify_para.kafka_output_stream_state)
	{
	case 0:
		if(traffic_identify_para.kafka_producer->SendData(topic, (void *)out, (size_t)out_len) != 0)
		{
			MESA_handle_runtime_log(traffic_identify_para.log_handle, RLOG_LV_FATAL, TRAFFIC_IDENTIFY_AV,
				"{%s:%d} Kafka SendData error.",__FILE__,__LINE__);		
		}
		//MESA_handle_runtime_log(traffic_identify_para.log_handle, RLOG_LV_DEBUG, TRAFFIC_IDENTIFY_AV,"%s send_kafka JSON is %s.",printaddr(&a_stream->addr,thread_seq),out); 
		
		break;
	case 1://just 1 send
		if (identifier_pme->burst_label==1||identifier_pme->ACK_label>2||identifier_pme->rule_label==1) 
		{
			if(traffic_identify_para.kafka_producer->SendData(topic, (void *)out, (size_t)out_len) != 0)
			{
				MESA_handle_runtime_log(traffic_identify_para.log_handle, RLOG_LV_FATAL, TRAFFIC_IDENTIFY_AV,
					"{%s:%d} Kafka SendData error.",__FILE__,__LINE__);		
			}
			//MESA_handle_runtime_log(traffic_identify_para.log_handle, RLOG_LV_DEBUG, TRAFFIC_IDENTIFY_AV,"%s send_kafka JSON is %s.",printaddr(&a_stream->addr,thread_seq),out); 			
		}
		break;
	default:
		break;
	}
	
	if(out)
	{
		free(out);
		out = NULL;
	}
	
	if(identifier_pme->cjson_obj)
	{
		cJSON_Delete(identifier_pme->cjson_obj); 
		identifier_pme->cjson_obj = NULL; 	
	}
	
}

void structure_json_base(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet,stream_type a_stream_type)
{
	traffic_identify_pmeinfo* identifier_pme = (traffic_identify_pmeinfo*)*pme;
	char s_ip_str[64] = {0};
	char d_ip_str[64] = {0};
	struct stream_tuple4_v4 *paddr;
	struct stream_tuple4_v6 *paddr6;
	time_t t;
	t=time(NULL);

	cJSON_AddNumberToObject(identifier_pme->cjson_obj, "found_time",identifier_pme->found_time);
	cJSON_AddNumberToObject(identifier_pme->cjson_obj, "expire_time",int(t));

	if(a_stream->addr.addrtype==ADDR_TYPE_IPV4)
	{
		paddr=(struct stream_tuple4_v4 *)a_stream->addr.paddr;
		inet_ntop(AF_INET, &paddr->saddr, s_ip_str, 64);
		inet_ntop(AF_INET, &paddr->daddr, d_ip_str, 64);
		cJSON_AddStringToObject(identifier_pme->cjson_obj, "src_ip", s_ip_str);
		cJSON_AddStringToObject(identifier_pme->cjson_obj, "dst_ip", d_ip_str);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "src_port", ntohs(paddr->source));
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "dst_port", ntohs(paddr->dest));
	}
	else if(a_stream->addr.addrtype==ADDR_TYPE_IPV6)
	{
		paddr6=(struct stream_tuple4_v6 *)a_stream->addr.paddr;
		inet_ntop(AF_INET6, &paddr6->saddr, s_ip_str, 64);
		inet_ntop(AF_INET6, &paddr6->daddr, d_ip_str, 64);
		cJSON_AddStringToObject(identifier_pme->cjson_obj, "src_ip", s_ip_str);
		cJSON_AddStringToObject(identifier_pme->cjson_obj, "dst_ip", d_ip_str);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "src_port", ntohs(paddr6->source));
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "dst_port", ntohs(paddr6->dest));
	}
	cJSON_AddNumberToObject(identifier_pme->cjson_obj, "stream_dir", a_stream->dir);
	cJSON_AddNumberToObject(identifier_pme->cjson_obj, "trans_proto", a_stream_type);
}

void structure_json_burst(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet,stream_type a_stream_type)
{
	traffic_identify_pmeinfo* identifier_pme = (traffic_identify_pmeinfo*)*pme;
	if (traffic_identify_para.kafka_output_feature_state==1)
	{	
		cJSON_AddItemToObject(identifier_pme->cjson_obj, "burst_payload_s2c", cJSON_CreateLongArray(identifier_pme->payload_burst_list_s2c,traffic_identify_para.burst_feature_output_chunk_count));
		cJSON_AddItemToObject(identifier_pme->cjson_obj, "burst_paknum_s2c", cJSON_CreateIntArray(identifier_pme->paknum_burst_list_s2c,traffic_identify_para.burst_feature_output_chunk_count));
		cJSON_AddItemToObject(identifier_pme->cjson_obj, "burst_payload_c2s", cJSON_CreateLongArray(identifier_pme->payload_burst_list_c2s,traffic_identify_para.burst_feature_output_chunk_count));
		cJSON_AddItemToObject(identifier_pme->cjson_obj, "burst_paknum_c2s", cJSON_CreateIntArray(identifier_pme->paknum_burst_list_c2s,traffic_identify_para.burst_feature_output_chunk_count));
		cJSON_AddItemToObject(identifier_pme->cjson_obj, "burst_time_interval_s2c", cJSON_CreateLongArray(identifier_pme->burst_time_interval_s2c,traffic_identify_para.burst_feature_output_chunk_count));
	}
}

void structure_json_ML(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet,stream_type a_stream_type,float* pre_feature)
{
	traffic_identify_pmeinfo* identifier_pme = (traffic_identify_pmeinfo*)*pme;
	if (traffic_identify_para.kafka_output_feature_state==1)
	{
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "win_sum_c2s", pre_feature[0]);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "win_mean_c2s", pre_feature[1]);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "win_min_c2s", pre_feature[2]);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "win_max_c2s", pre_feature[3]);

		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "win_sum_s2c", pre_feature[4]);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "win_mean_s2c", pre_feature[5]);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "win_min_s2c", pre_feature[6]);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "win_max_s2c", pre_feature[7]);

		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "win_sum", pre_feature[8]);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "win_mean", pre_feature[9]);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "win_min", pre_feature[10]);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "win_max", pre_feature[11]);

		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "payload_sum_c2s", pre_feature[12]);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "payload_mean_c2s", pre_feature[13]);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "payload_max_c2s", pre_feature[14]);

		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "payload_sum_s2c", pre_feature[15]);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "payload_mean_s2c", pre_feature[16]);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "payload_max_s2c", pre_feature[17]);

		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "payload_sum", pre_feature[18]);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "payload_mean", pre_feature[19]);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "payload_max", pre_feature[20]);

		/*
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "time_interval_min_c2s", identifier_pme->time_interval_min_c2s);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "time_interval_max_c2s", identifier_pme->time_interval_max_c2s);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "time_interval_min_s2c", identifier_pme->time_interval_min_s2c);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "time_interval_max_s2c", identifier_pme->time_interval_max_s2c);
		*/
	}
}

void structure_json_label(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet,stream_type a_stream_type)
{
	traffic_identify_pmeinfo* identifier_pme = (traffic_identify_pmeinfo*)*pme;
	float time_duration=a_stream->ptcpdetail->lastmtime-a_stream->ptcpdetail->createtime;
	float bps_c2s=0;
	float bps_s2c=0;

	if (time_duration!=0)
	{
		bps_s2c=a_stream->ptcpdetail->clientbytes/time_duration;
		bps_c2s=a_stream->ptcpdetail->serverbytes/time_duration;
	}

	if (traffic_identify_para.kafka_output_feature_state==1)
	{
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "time_duration", time_duration);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "bps_c2s", bps_c2s);
		cJSON_AddNumberToObject(identifier_pme->cjson_obj, "bps_s2c", bps_s2c);		
	}
	
	cJSON_AddNumberToObject(identifier_pme->cjson_obj, "SSL_flag", identifier_pme->SSL_flag);
	cJSON_AddNumberToObject(identifier_pme->cjson_obj, "BURST_flag", identifier_pme->burst_label);
	cJSON_AddNumberToObject(identifier_pme->cjson_obj, "RULE_flag", identifier_pme->rule_label);
	cJSON_AddNumberToObject(identifier_pme->cjson_obj, "ACK_flag", identifier_pme->ACK_label);
	cJSON_AddStringToObject(identifier_pme->cjson_obj, "dst_SNI", identifier_pme->SNI);
}

unsigned short get_win(void *a_packet,stream_type a_stream_type)
{
	pak_HEAD pak_head;
	switch(a_stream_type)
		{
		case TCP:
			if (a_packet!=NULL)
			{
				pak_head=*(pak_HEAD*)a_packet;
				unsigned short WIN=short(ntohs(pak_head.WinSize));
				return WIN;
			}
			else 
				return 0;
			break;
		case UDP:
			return 0;
			break;
		default:
			return 0;
			break;
		}
	return 0;
}

unsigned long get_ack(void *a_packet,stream_type a_stream_type)
{
	pak_HEAD pak_head;
	switch(a_stream_type)
		{
		case TCP:
			if (a_packet!=NULL)
			{
				pak_head=*(pak_HEAD*)a_packet;
				unsigned short ACK[2]={ntohs(pak_head.AckNum2),ntohs(pak_head.AckNum)};
				unsigned long val = *(unsigned long*)ACK;
				return val;
			}
			else 
				return 0;
			break;
		case UDP:
			return 0;
			break;
		default:
			return 0;
			break;
		}
	return 0;
}

unsigned long get_seq(void *a_packet,stream_type a_stream_type)
{
	pak_HEAD pak_head;
	switch(a_stream_type)
		{
		case TCP:
			if (a_packet!=NULL)
			{
				pak_head=*(pak_HEAD*)a_packet;
				unsigned short SEQ[2]={ntohs(pak_head.SeqNum2),ntohs(pak_head.SeqNum)};
				unsigned long val = *(unsigned long*)SEQ;
				return val;
			}
			else 
				return 0;
			break;
		case UDP:
			return 0;
			break;
		default:
			return 0;
			break;
		}
	return 0;
}

void record_burst(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet,stream_type a_stream_type,unsigned long long cur_time)
{
	traffic_identify_pmeinfo* identifier_pme = (traffic_identify_pmeinfo*)*pme;
	unsigned long long time_duration=0;
	short payload=0;
	switch(a_stream_type)
	{
		case TCP:
		{
			payload=a_stream->ptcpdetail->datalen+58;
			break;
		}
		case UDP:
		{
			payload=a_stream->pudpdetail->datalen+42;
			break;
		}
		default:
			break;
	}

	if (identifier_pme->burst_chunk_count>int(traffic_identify_para.burst_list_len)-2)
		return;

	switch(a_stream->curdir)
	{
	case DIR_C2S:
		{
			if (identifier_pme->paknum_burst_list_c2s[identifier_pme->burst_chunk_count]<1000000)
			{
				identifier_pme->payload_burst_list_c2s[identifier_pme->burst_chunk_count]=identifier_pme->payload_burst_list_c2s[identifier_pme->burst_chunk_count]+payload;
				identifier_pme->paknum_burst_list_c2s[identifier_pme->burst_chunk_count]++;
			}
		}
		break;
	case DIR_S2C:
		{
			time_duration=cur_time-identifier_pme->pre_time_s2c;
			if (time_duration>traffic_identify_para.burst_interval)
			{
				identifier_pme->burst_time_interval_s2c[identifier_pme->burst_chunk_count]=time_duration;
				identifier_pme->burst_chunk_count++;
				/*
				if (identifier_pme->burst_chunk_count>=int(traffic_identify_para.paknum_threshold*identifier_pme->burst_list_thlevel))
				{
					identifier_pme->burst_list_thlevel++;
					long* temp1=identifier_pme->payload_burst_list_s2c;
					int* temp2=identifier_pme->paknum_burst_list_s2c;
					long* newlist1=(long*)dictator_malloc(thread_seq,(identifier_pme->burst_list_thlevel)*traffic_identify_para.paknum_threshold*sizeof(long));
					int* newlist2=(int*)dictator_malloc(thread_seq,(identifier_pme->burst_list_thlevel)*traffic_identify_para.paknum_threshold*sizeof(int));
					memset(identifier_pme->payload_burst_list_s2c,0,sizeof((identifier_pme->burst_list_thlevel)*traffic_identify_para.paknum_threshold*sizeof(long)));
					memset(identifier_pme->paknum_burst_list_s2c,0,sizeof((identifier_pme->burst_list_thlevel)*traffic_identify_para.paknum_threshold*sizeof(int)));
					for (int i=0;i<identifier_pme->burst_chunk_count;i++)
					{
						newlist1[i]=temp1[i];
						newlist2[i]=temp2[i];
					}
					identifier_pme->payload_burst_list_s2c=newlist1;
					identifier_pme->paknum_burst_list_s2c=newlist2;
					if(temp1!=NULL)
					{
						dictator_free(thread_seq, temp1);
						temp1=NULL;
					}
					if(temp2!=NULL)
					{
						dictator_free(thread_seq, temp2);
						temp2=NULL;
					}
				}
				*/
			}
			identifier_pme->payload_burst_list_s2c[identifier_pme->burst_chunk_count]=identifier_pme->payload_burst_list_s2c[identifier_pme->burst_chunk_count]+payload;
			identifier_pme->paknum_burst_list_s2c[identifier_pme->burst_chunk_count]++;
		}
		break;
	default:
		break;
	}
}

void record_ack(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet,stream_type a_stream_type)
{
	traffic_identify_pmeinfo* identifier_pme =(traffic_identify_pmeinfo*)*pme;
	int flag=0;
	int i=0;
	switch(a_stream->curdir)
	{
	case DIR_C2S:		
		break;
	
	case DIR_S2C:
		switch(a_stream_type)
		{
		case TCP:
		{	
			if (identifier_pme->ack_count>int(traffic_identify_para.ack_list_len)-2)
			{
				return;
			}
			unsigned long ack_num=get_ack(a_packet,a_stream_type);
			unsigned long seq_num=get_seq(a_packet,a_stream_type);
			if (ack_num==identifier_pme->current_ack_num)
			{
				identifier_pme->ack_list[identifier_pme->ack_count].payload_bytes +=a_stream->ptcpdetail->datalen;
				identifier_pme->ack_list[identifier_pme->ack_count].pak_count++;
				//统计当前ack块种最大以及最小的的seq号，用于后续计算去除重传包后的ack块长度
				if (identifier_pme->ack_list[identifier_pme->ack_count].min_seq==0)
				{
					identifier_pme->ack_list[identifier_pme->ack_count].min_seq=seq_num;
				}
				if (identifier_pme->ack_list[identifier_pme->ack_count].max_seq<seq_num)
				{
					identifier_pme->ack_list[identifier_pme->ack_count].max_seq=seq_num;
					identifier_pme->ack_list[identifier_pme->ack_count].max_seq_payload=a_stream->ptcpdetail->datalen;
				}
			}
			else
			{
				flag=0;
				//后续应该为字典，或哈希索引
				for(i=0;i<=identifier_pme->ack_count;i++)
				{
					if(identifier_pme->ack_list[i].ack_id==ack_num)
					{
						identifier_pme->ack_list[i].payload_bytes +=a_stream->ptcpdetail->datalen;
						identifier_pme->ack_list[i].pak_count++;
						flag=1;
						if (identifier_pme->ack_list[i].min_seq==0)
						{
							identifier_pme->ack_list[i].min_seq=seq_num;
						}
						if (identifier_pme->ack_list[i].max_seq<seq_num)
						{
							identifier_pme->ack_list[i].max_seq=seq_num;
							identifier_pme->ack_list[i].max_seq_payload=a_stream->ptcpdetail->datalen;
						}
						break;
					}
				}
				if (flag==0)
				{
					identifier_pme->ack_count++;
					identifier_pme->ack_list[identifier_pme->ack_count].payload_bytes=a_stream->ptcpdetail->datalen;
					identifier_pme->ack_list[identifier_pme->ack_count].pak_count=1;
					identifier_pme->ack_list[identifier_pme->ack_count].ack_id=ack_num;
					identifier_pme->current_ack_num=ack_num;
					if (identifier_pme->ack_list[identifier_pme->ack_count].min_seq==0)
					{
						identifier_pme->ack_list[identifier_pme->ack_count].min_seq=seq_num;
					}
					if (identifier_pme->ack_list[identifier_pme->ack_count].max_seq<seq_num)
					{
						identifier_pme->ack_list[identifier_pme->ack_count].max_seq=seq_num;
						identifier_pme->ack_list[identifier_pme->ack_count].max_seq_payload=a_stream->ptcpdetail->datalen;
					}
				}
			}
			break;
		}
		case UDP:
			break;

		default:
			break;
		}
		break;

	default:
			break;
	}
}

void record_win(struct streaminfo *a_stream,void **pme,void *a_packet,stream_type a_stream_type)
{
	unsigned short win_size=get_win(a_packet,a_stream_type);
	traffic_identify_pmeinfo* identifier_pme =(traffic_identify_pmeinfo*)*pme;

	switch(a_stream->curdir)
	{
	case DIR_C2S:
		if (win_size>identifier_pme->win_c2s_max)
			identifier_pme->win_c2s_max=win_size;
		if (win_size<identifier_pme->win_c2s_min)
			identifier_pme->win_c2s_min=win_size;
		identifier_pme->win_c2s_sum=identifier_pme->win_c2s_sum+win_size;
		break;

	case DIR_S2C:
		if (win_size>identifier_pme->win_s2c_max)
			identifier_pme->win_s2c_max=win_size;
		if (win_size<identifier_pme->win_s2c_min)
			identifier_pme->win_s2c_min=win_size;
		identifier_pme->win_s2c_sum=identifier_pme->win_s2c_sum+win_size;
		break;
	
	default:
		break;
	}
}

/*
void record_time_interval(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet,stream_type a_stream_type,unsigned long long cur_time)
{
	traffic_identify_pmeinfo* identifier_pme =(traffic_identify_pmeinfo*)*pme;
	long time_duration=0;
	
	switch(a_stream->curdir)
	{
	case DIR_C2S:
		time_duration=cur_time-identifier_pme->pre_time_c2s;
		if (identifier_pme->time_interval_max_c2s<time_duration)
			identifier_pme->time_interval_max_c2s=time_duration;
		if (identifier_pme->time_interval_min_c2s>time_duration)
			identifier_pme->time_interval_min_c2s=time_duration;
		break;
	
	case DIR_S2C:
		time_duration=cur_time-identifier_pme->pre_time_s2c;
		if (identifier_pme->time_interval_max_s2c<time_duration)
			identifier_pme->time_interval_max_s2c=time_duration;
		if (identifier_pme->time_interval_min_s2c>time_duration)
			identifier_pme->time_interval_min_s2c=time_duration;
		break;

	default:
		break;
	}
}
*/

void record_len_max(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet,stream_type a_stream_type)
{
	traffic_identify_pmeinfo* identifier_pme =(traffic_identify_pmeinfo*)*pme;
	switch(a_stream->curdir)
	{
	case DIR_C2S:		
		switch(a_stream_type)
		{
		case TCP:
			if(identifier_pme->c2s_payload_max<int(a_stream->ptcpdetail->datalen))
				identifier_pme->c2s_payload_max=a_stream->ptcpdetail->datalen;
			//identifier_pme->payload_bytes_c2s=a_stream->ptcpdetail->datalen+identifier_pme->payload_bytes_c2s+58;
			break;

		case UDP:
			if(identifier_pme->c2s_payload_max<int(a_stream->pudpdetail->datalen))
				identifier_pme->c2s_payload_max=a_stream->pudpdetail->datalen;
			//identifier_pme->payload_bytes_c2s=a_stream->pudpdetail->datalen+identifier_pme->payload_bytes_c2s+42;
			break;

		default:
			break;
		}
		//identifier_pme->c2s_pak_count ++;
		break;
	
	case DIR_S2C:
		switch(a_stream_type)
		{
		case TCP:
			if(identifier_pme->s2c_payload_max<int(a_stream->ptcpdetail->datalen))
				identifier_pme->s2c_payload_max=a_stream->ptcpdetail->datalen;
			//identifier_pme->payload_bytes_s2c=a_stream->ptcpdetail->datalen+identifier_pme->payload_bytes_s2c+58;
			break;

		case UDP:
			if(identifier_pme->s2c_payload_max<int(a_stream->pudpdetail->datalen))
				identifier_pme->s2c_payload_max=a_stream->pudpdetail->datalen;
			//identifier_pme->payload_bytes_s2c=a_stream->pudpdetail->datalen+identifier_pme->payload_bytes_s2c+42;
			break;

		default:
			break;
		}
		//identifier_pme->s2c_pak_count ++;
		break;

	default:
			break;
	}
	//identifier_pme->payload_cnt ++;
}

void ssl_analysis_tls12(void **pme, int thread_seq,void *a_packet,stream_type a_stream_type)
{
	traffic_identify_pmeinfo* identifier_pme =(traffic_identify_pmeinfo*)*pme;
	pak_HEAD pak_head;
	unsigned short packetLen=0;
	unsigned char* cursor;
	cursor=(unsigned char*)a_packet;
	if (a_stream_type!=TCP||a_packet==NULL)
		return;

	pak_head=*(pak_HEAD*)a_packet;
	packetLen=ntohs(pak_head.wPacketLen);

	if ((packetLen-40)<5)
		return;
	
	cursor=cursor+40;//1.3+52、1.2+40、1+52

	if(int(*cursor)==20||int(*cursor)==21||int(*cursor)==22||int(*cursor)==23)
		identifier_pme->SSL_flag=1;
		
	if (int(*cursor)!=22)
		return;
	
	cursor=cursor+5;
	if (int(*cursor)!=1)
		return;
	
	cursor=cursor+38;
	int session_id_len=*cursor;

	cursor=cursor+session_id_len+1;
	int cipher_suites_len=ntohs(*(unsigned short*)cursor);

	cursor=cursor+cipher_suites_len+2;
	int compression_len=*cursor;

	cursor=cursor+compression_len+1;
	int extension_len=ntohs(*(unsigned short*)cursor);
	int flag=0;
	int extension_type=0;
	int Length=0;
	char SNI[1024];
	int j=0;
	memset(SNI, 0 ,sizeof(SNI));
	
	cursor=cursor+2;
	while(flag<extension_len)
	{
		extension_type=ntohs(*(unsigned short*)cursor);
		cursor=cursor+2;
		Length=ntohs(*(unsigned short*)cursor);
		if (extension_type==0)
		{
			cursor=cursor+7;
			Length=Length-5;
			if (Length>1024)
				Length=1020;
			for(j=0;j<Length;j++)
			{
				SNI[j]=*(char*)(cursor);
				cursor=cursor+1;
			}
			memcpy(identifier_pme->SNI,SNI,strlen(SNI));
			break;
		}
		else
		{
			cursor=cursor+Length+2;
			flag=flag+4+Length;
		}
	}
}

void ssl_analysis_tls13(void **pme, int thread_seq,void *a_packet,stream_type a_stream_type)
{
	traffic_identify_pmeinfo* identifier_pme =(traffic_identify_pmeinfo*)*pme;
	pak_HEAD pak_head;
	unsigned short packetLen=0;
	unsigned char* cursor;
	cursor=(unsigned char*)a_packet;
	if (a_stream_type!=TCP||a_packet==NULL)
		return;

	pak_head=*(pak_HEAD*)a_packet;
	packetLen=ntohs(pak_head.wPacketLen);

	if ((packetLen-52)<5)
		return;
	
	cursor=cursor+52;//1.3+52、1.2+40、1+52

	if(int(*cursor)==20||int(*cursor)==21||int(*cursor)==22||int(*cursor)==23)
		identifier_pme->SSL_flag=1;
		
	if (int(*cursor)!=22)
		return;
	
	cursor=cursor+5;
	if (int(*cursor)!=1)
		return;
	
	cursor=cursor+38;
	int session_id_len=*cursor;

	cursor=cursor+session_id_len+1;
	int cipher_suites_len=ntohs(*(unsigned short*)cursor);

	cursor=cursor+cipher_suites_len+2;
	int compression_len=*cursor;

	cursor=cursor+compression_len+1;
	int extension_len=ntohs(*(unsigned short*)cursor);
	int flag=0;
	int extension_type=0;
	int Length=0;
	char SNI[1024];
	int j=0;
	memset(SNI, 0 ,sizeof(SNI));
	
	cursor=cursor+2;
	while(flag<extension_len)
	{
		extension_type=ntohs(*(unsigned short*)cursor);
		cursor=cursor+2;
		Length=ntohs(*(unsigned short*)cursor);
		if (extension_type==0)
		{
			cursor=cursor+7;
			Length=Length-5;
			if (Length>1024)
				Length=1020;
			for(j=0;j<Length;j++)
			{
				SNI[j]=*(char*)(cursor);
				cursor=cursor+1;
			}
			memcpy(identifier_pme->SNI,SNI,strlen(SNI));
			break;
		}
		else
		{
			cursor=cursor+Length+2;
			flag=flag+4+Length;
		}
	}
}

void auto_label(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet,stream_type a_stream_type)
{
	traffic_identify_pmeinfo* identifier_pme = (traffic_identify_pmeinfo*)*pme;

	if(a_stream->ptcpdetail->lastmtime - a_stream->ptcpdetail->createtime == 0||a_stream->ptcpdetail->clientpktnum ==0||a_stream->ptcpdetail->serverpktnum==0)
		return;

	if((traffic_identify_para.identifier_type >> LEN_RELATED) && (traffic_identify_para.identifier_type >> TIME_RELATED))
	{
		if((a_stream->ptcpdetail->clientbytes + a_stream->ptcpdetail->serverbytes > traffic_identify_para.total_bytes)
			&&(a_stream->ptcpdetail->lastmtime - a_stream->ptcpdetail->createtime > traffic_identify_para.duration)
			&&(((a_stream->ptcpdetail->clientpktnum != 0)&&(a_stream->ptcpdetail->clientbytes/a_stream->ptcpdetail->clientpktnum > traffic_identify_para.avg_pkt_len))
			||((a_stream->ptcpdetail->serverpktnum != 0)&&(a_stream->ptcpdetail->serverbytes/a_stream->ptcpdetail->serverpktnum > traffic_identify_para.avg_pkt_len)))
			&&(((a_stream->ptcpdetail->lastmtime - a_stream->ptcpdetail->createtime != 0)&&(a_stream->ptcpdetail->clientbytes/(a_stream->ptcpdetail->lastmtime - a_stream->ptcpdetail->createtime) > traffic_identify_para.Bps))
			||((a_stream->ptcpdetail->lastmtime - a_stream->ptcpdetail->createtime != 0)&&(a_stream->ptcpdetail->serverbytes/(a_stream->ptcpdetail->lastmtime - a_stream->ptcpdetail->createtime) > traffic_identify_para.Bps))))
		{
			identifier_pme->rule_label = 1;
		}
	}
	else if(traffic_identify_para.identifier_type >> LEN_RELATED)
	{
		if((a_stream->ptcpdetail->clientbytes + a_stream->ptcpdetail->serverbytes > traffic_identify_para.total_bytes)
			&&(((a_stream->ptcpdetail->clientpktnum != 0)&&(a_stream->ptcpdetail->clientbytes/a_stream->ptcpdetail->clientpktnum > traffic_identify_para.avg_pkt_len))
			||((a_stream->ptcpdetail->serverpktnum != 0)&&(a_stream->ptcpdetail->serverbytes/a_stream->ptcpdetail->serverpktnum > traffic_identify_para.avg_pkt_len))))
		{
			identifier_pme->rule_label = 1;
		}			
	}	
}

void ACK_label_fun(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet,stream_type a_stream_type)
{
	traffic_identify_pmeinfo* identifier_pme =(traffic_identify_pmeinfo*)*pme;
	int chunk_count=0;
	int i=0;
	//ack_str* cursor=NULL;

	if (identifier_pme->ack_count<=1)
		return;

	for(i=0;i<=identifier_pme->ack_count;i++)
	{
		if(identifier_pme->ack_list[i].payload_bytes>traffic_identify_para.ack_payload_threshlod&&identifier_pme->ack_list[i].pak_count>traffic_identify_para.ack_paknum_threshlod)
		{
			chunk_count++;
		}

	}
	/*
	for(i=0;i<=identifier_pme->ack_count;i++)
	{
		cursor=identifier_pme->ack_list+i;
		if(cursor->payload_bytes>traffic_identify_para.ack_payload_threshlod&&cursor->pak_count>traffic_identify_para.ack_paknum_threshlod)
		{
			chunk_count++;
			//printf("%ld,%ld \n",identifier_pme->ack_list[i].payload_bytes,identifier_pme->ack_list[i].pak_count);
		}

	}
	*/
	if (chunk_count>=1)
	{
		identifier_pme->ACK_label=chunk_count;
	}
}

void ML_feature_record(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet,stream_type a_stream_type,float* pre_feature)
{
	traffic_identify_pmeinfo* identifier_pme =(traffic_identify_pmeinfo*)*pme;
	unsigned short win_mean=0;
	unsigned short win_max=0;
	unsigned short win_min=0;
	short paklen_max=0;

	float c2s_len_mean=0;
	float s2c_len_mean=0;

	long win_c2s_mean=0;
	long win_s2c_mean=0;

	int c2s_pak_count=0;
	int s2c_pak_count=0;
	long payload_bytes_c2s=0;
	long payload_bytes_s2c=0;

	switch (a_stream_type)
	{
	case TCP:
		c2s_pak_count=a_stream->ptcpdetail->serverpktnum;
		s2c_pak_count=a_stream->ptcpdetail->clientpktnum;
		payload_bytes_c2s=a_stream->ptcpdetail->serverbytes+c2s_pak_count*58;
		payload_bytes_s2c=a_stream->ptcpdetail->clientbytes+s2c_pak_count*58;
		break;
	case UDP:
		c2s_pak_count=a_stream->pudpdetail->serverpktnum;
		s2c_pak_count=a_stream->pudpdetail->clientpktnum;
		payload_bytes_c2s=a_stream->pudpdetail->serverbytes+c2s_pak_count*42;
		payload_bytes_s2c=a_stream->pudpdetail->clientbytes+s2c_pak_count*42;
		break;
	
	default:
		c2s_pak_count=a_stream->ptcpdetail->serverpktnum;
		s2c_pak_count=a_stream->ptcpdetail->clientpktnum;
		payload_bytes_c2s=a_stream->ptcpdetail->serverbytes+c2s_pak_count*58;
		payload_bytes_s2c=a_stream->ptcpdetail->clientbytes+s2c_pak_count*58;
		break;
	}

	identifier_pme->s2c_payload_max=identifier_pme->s2c_payload_max+50;
	identifier_pme->c2s_payload_max=identifier_pme->c2s_payload_max+50;

	if(c2s_pak_count!=0)
	{
		c2s_len_mean=payload_bytes_c2s/c2s_pak_count;
		win_c2s_mean=long(identifier_pme->win_c2s_sum/c2s_pak_count);
	}
	else
	{
		c2s_len_mean=payload_bytes_c2s;
		win_c2s_mean=0;
	}

	if(s2c_pak_count!=0)
	{
		s2c_len_mean=payload_bytes_s2c/s2c_pak_count;
		win_s2c_mean=long(identifier_pme->win_s2c_sum/s2c_pak_count);
	}
	else
	{
		s2c_len_mean=payload_bytes_s2c;
		win_s2c_mean=0;
	}

	if(identifier_pme->win_c2s_min==65530)
		identifier_pme->win_c2s_min=0;
	if(identifier_pme->win_s2c_min==65530)
		identifier_pme->win_s2c_min=0;

	long long win_sum=identifier_pme->win_c2s_sum+identifier_pme->win_s2c_sum;

	if (identifier_pme->win_c2s_max>=identifier_pme->win_s2c_max)
		win_max=identifier_pme->win_c2s_max;
	else
		win_max=identifier_pme->win_s2c_max;
	if (identifier_pme->win_c2s_min<=identifier_pme->win_s2c_min)
		win_min=identifier_pme->win_c2s_min;
	else
		win_min=identifier_pme->win_s2c_min;

	
	if(identifier_pme->payload_cnt!=0)
		win_mean=win_sum/identifier_pme->payload_cnt;
	else
		identifier_pme->payload_cnt=1;

	if (identifier_pme->c2s_payload_max>=identifier_pme->s2c_payload_max)
		paklen_max=identifier_pme->c2s_payload_max;
	else
		paklen_max=identifier_pme->s2c_payload_max;

	pre_feature[0]=float(identifier_pme->win_c2s_sum);pre_feature[1]=float(win_c2s_mean);pre_feature[2]=float(identifier_pme->win_c2s_min);pre_feature[3]=float(identifier_pme->win_c2s_max);
	pre_feature[4]=float(identifier_pme->win_s2c_sum);pre_feature[5]=float(win_s2c_mean);pre_feature[6]=float(identifier_pme->win_s2c_min);pre_feature[7]=float(identifier_pme->win_s2c_max);
	pre_feature[8]=float(win_sum);pre_feature[9]=float(win_mean);pre_feature[10]=float(win_min);pre_feature[11]=float(win_max);
	pre_feature[12]=float(payload_bytes_c2s);pre_feature[13]=float(c2s_len_mean);pre_feature[14]=float(identifier_pme->c2s_payload_max);
	pre_feature[15]=float(payload_bytes_s2c);pre_feature[16]=float(s2c_len_mean);pre_feature[17]=float(identifier_pme->s2c_payload_max);
	pre_feature[18]=float((payload_bytes_s2c+payload_bytes_c2s));pre_feature[19]=float(((payload_bytes_s2c+payload_bytes_c2s)/identifier_pme->payload_cnt));pre_feature[20]=float(paklen_max);
}

void write_csv(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet,stream_type a_stream_type)
{
	traffic_identify_pmeinfo* identifier_pme =(traffic_identify_pmeinfo*)*pme;
	char* out = NULL;
	//uint32_t out_len = 0;
	out = cJSON_PrintUnformatted(identifier_pme->cjson_obj);
	fprintf(traffic_identify_para.file,"%s,",out);
	fprintf(traffic_identify_para.file,"\n");
}

void write_csv_ack(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet,stream_type a_stream_type)
{
	traffic_identify_pmeinfo* identifier_pme =(traffic_identify_pmeinfo*)*pme;
	int chunk_count=0;
	for(int i=0;i<=identifier_pme->ack_count;i++)
	{
		if (identifier_pme->ack_list[i].payload_bytes> 6000)
			chunk_count++;
	}
	if (chunk_count>=5)
	{
		fprintf(traffic_identify_para.file,"\r\n%s,",printaddr(&a_stream->addr,thread_seq));
		for(int j=0;j<=identifier_pme->ack_count;j++)
		{
			if (identifier_pme->ack_list[j].payload_bytes> 6000)
				fprintf(traffic_identify_para.file,"%ld/",identifier_pme->ack_list[j].payload_bytes);
		}
	}

	//fprintf(traffic_identify_para.file,"\n");
}

void lstf_video_title_identification(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet,stream_type a_stream_type)
{
	traffic_identify_pmeinfo* identifier_pme =(traffic_identify_pmeinfo*)*pme;
	vector<float> chunk_list;
	float chunk_size;
	//最后一个ACK块可能不完整，因此不计入统计
	for(int i=0;i<identifier_pme->ack_count;i++)
	{
		if (identifier_pme->ack_list[i].payload_bytes> (traffic_identify_para.video_chunk_size_min*1.00135177+1119))
		{
			chunk_size=(float)(identifier_pme->ack_list[i].payload_bytes-1119)/1.00135177;
			chunk_list.push_back(chunk_size);
			if (chunk_list.size()==traffic_identify_para.word_len_long-1+traffic_identify_para.word_num_long)
				break;
		}
	}
	//保证进行匹配的单词个数
	if (chunk_list.size()<traffic_identify_para.word_len_long-1+traffic_identify_para.word_num_long)
	{
		//cout<<"chunk size:"<<chunk_list.size()<<endl;
		identifier_pme->video_title_type=1;
		return;
	}
	vector<int> word_gen_array;//用于词生成
	map<string,float> match_map;
	string cur_word;
	int word_count=0;
	for (int j=0;j<chunk_list.size();j++)
	{
		float chunk=chunk_list[j];
		int bin_index_cur=0;
		bin_index_cur=chunk2symbol(chunk);
		//生成词cur_word
		word_gen_array.push_back(bin_index_cur);
		if (word_gen_array.size()<traffic_identify_para.word_len_long)
			continue;
		cur_word="";
		for (int k=0;k<word_gen_array.size();k++)
		{
			cur_word +=to_string(word_gen_array[k])+'-';
		}
		cout<<cur_word<<endl;
		word_gen_array.erase(word_gen_array.begin());
		//统计匹配概率，并记录到match_map中
		if (traffic_identify_para.word_dictionary_local.find(cur_word) != traffic_identify_para.word_dictionary_local.end())
		{
			float global_prob=log(traffic_identify_para.word_count_long/(traffic_identify_para.word_dictionary_global[cur_word]+1));
			for (int k=0;k<traffic_identify_para.word_dictionary_local[cur_word].size();k++)
			{
				string cur_key=traffic_identify_para.word_dictionary_local[cur_word][k].video_id;
				float cur_val=traffic_identify_para.word_dictionary_local[cur_word][k].trans_prob;
				if (match_map.find(cur_key)==match_map.end())
					match_map[cur_key]=cur_val+global_prob*100;
				else
					match_map[cur_key] +=cur_val+global_prob*100;
			}
		}
		word_count ++;
		if (word_count==traffic_identify_para.word_num_long)
			break;
	}
	//找到转移概率最大的指纹
	if (match_map.size()!=0)
	{
		float max_prob=0;
		string target_id="";
		for (auto& element: match_map)
		{
			if (element.second>max_prob)
			{
				max_prob=element.second;
				target_id=element.first;
			}
		}
		identifier_pme->video_id=target_id.c_str();
		identifier_pme->video_title_type=2;
		cout<<target_id<<endl;
		return;
	}
	//二阶段匹配
	match_map.clear();
	word_gen_array.clear();
	word_count=0;
	for (int j=0;j<chunk_list.size();j++)
	{
		float chunk=chunk_list[j];
		int bin_index_cur=0;
		bin_index_cur=chunk2symbol(chunk);
		//生成词cur_word
		word_gen_array.push_back(bin_index_cur);
		if (word_gen_array.size()<traffic_identify_para.word_len_short)
			continue;
		cur_word="";
		for (int k=0;k<word_gen_array.size();k++)
		{
			cur_word +=to_string(word_gen_array[k])+'-';
		}
		cout<<cur_word<<endl;
		word_gen_array.erase(word_gen_array.begin());
		//统计匹配概率，并记录到match_map中
		if (traffic_identify_para.word_dictionary_local.find(cur_word) != traffic_identify_para.word_dictionary_local.end())
		{
			float global_prob=log(traffic_identify_para.word_count_short/(traffic_identify_para.word_dictionary_global[cur_word]+1));
			for (int k=0;k<traffic_identify_para.word_dictionary_local[cur_word].size();k++)
			{
				string cur_key=traffic_identify_para.word_dictionary_local[cur_word][k].video_id;
				float cur_val=traffic_identify_para.word_dictionary_local[cur_word][k].trans_prob;
				if (match_map.find(cur_key)==match_map.end())
					match_map[cur_key]=cur_val+global_prob*100;
				else
					match_map[cur_key] +=cur_val+global_prob*100;
			}
		}
		word_count ++;
		if (word_count==traffic_identify_para.word_num_short)
			break;
	}
	//找到转移概率最大的指纹
	cout<<match_map.size()<<endl;
	if (match_map.size()!=0)
	{
		float max_prob=0;
		string target_id="";
		for (auto& element: match_map)
		{	
			if (element.second>max_prob)
			{
				max_prob=element.second;
				target_id=element.first;
			}
		}
		identifier_pme->video_id=target_id.c_str();
		identifier_pme->video_title_type=2;
		cout<<target_id<<endl;
		return;
	}
	identifier_pme->video_title_type=2;
	cout<<"match fail"<<endl;
	return;
}

void write_csv_ack_seqLen(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet,stream_type a_stream_type)
{
	traffic_identify_pmeinfo* identifier_pme =(traffic_identify_pmeinfo*)*pme;
	int chunk_count=0;
	for(int i=0;i<=identifier_pme->ack_count;i++)
	{
		if (identifier_pme->ack_list[i].payload_bytes> 6000)
			chunk_count++;
	}
	if (chunk_count>=5)
	{
		fprintf(traffic_identify_para.file,"\r\n%s,",printaddr(&a_stream->addr,thread_seq));
		for(int j=0;j<=identifier_pme->ack_count;j++)
		{
			if (identifier_pme->ack_list[j].payload_bytes> 6000)
				fprintf(traffic_identify_para.file,"%ld/",(identifier_pme->ack_list[j].max_seq-identifier_pme->ack_list[j].min_seq+identifier_pme->ack_list[j].max_seq_payload));
		}
	}

	//fprintf(traffic_identify_para.file,"\n");
}

void burst_label(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet,stream_type a_stream_type)
{
	int pass_chunk=0;
	traffic_identify_pmeinfo* identifier_pme =(traffic_identify_pmeinfo*)*pme;
	if (identifier_pme->burst_chunk_count<int(traffic_identify_para.burst_chunkcount_threshlod))
		return;
	for (int i=0;i<identifier_pme->burst_chunk_count;i++)
	{
		if (identifier_pme->payload_burst_list_s2c[i]>=int(traffic_identify_para.burst_payload_threshlod) && identifier_pme->paknum_burst_list_s2c[i]>=int(traffic_identify_para.burst_paknum_threshlod))
		{
			pass_chunk++;
		}
		if (pass_chunk>=int(traffic_identify_para.burst_chunkcount_threshlod))
		{
			identifier_pme->burst_label=1;
			return;
		}
	}
}

UCHAR traffic_process(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet,stream_type a_stream_type)
{
	traffic_identify_pmeinfo* identifier_pme = NULL;
	unsigned long time_duration=0;
	int ret=0;
	float pre_feature[21];
	memset(pre_feature,0,sizeof(pre_feature));
	struct timeval cur_time_str;
	unsigned long long cur_time;
	string video_tuple=printaddr(&a_stream->addr,thread_seq);
	string target_sni="1";

	switch (a_stream->pktstate)
	{
		case OP_STATE_PENDING:// init pme
			init_pme(pme, thread_seq, a_stream,a_stream_type);
			ssl_analysis_tls12(pme,thread_seq,a_packet,a_stream_type);
			ssl_analysis_tls13(pme,thread_seq,a_packet,a_stream_type);

		case OP_STATE_DATA://record len and time
			identifier_pme = (traffic_identify_pmeinfo*)*pme;
			
			if (traffic_identify_para.run_mode==0)
			{
				ret=get_rawpkt_opt_from_streaminfo(a_stream, RAW_PKT_GET_TIMESTAMP, &cur_time_str);
				if (ret>=0)
					cur_time=cur_time_str.tv_sec*1000 + cur_time_str.tv_usec/1000;
				else
				{
					if (identifier_pme->pre_time_c2s<identifier_pme->pre_time_s2c)
						cur_time=identifier_pme->pre_time_s2c;
					else
						cur_time=identifier_pme->pre_time_c2s;
				}
			}
			else
			{	
				gettimeofday(&cur_time_str, NULL);
				cur_time=cur_time_str.tv_sec*1000 + cur_time_str.tv_usec/1000;
			}

			time_duration=a_stream->ptcpdetail->lastmtime-a_stream->ptcpdetail->createtime;
			identifier_pme->payload_cnt ++;
			
			ssl_analysis_tls12(pme,thread_seq,a_packet,a_stream_type);
			ssl_analysis_tls13(pme,thread_seq,a_packet,a_stream_type);
			record_burst(a_stream,pme,thread_seq,a_packet,a_stream_type,cur_time);
			//record_time_interval(a_stream,pme,thread_seq,a_packet,a_stream_type,cur_time);
			record_len_max(a_stream,pme,thread_seq,a_packet,a_stream_type);
			record_win(a_stream,pme,a_packet,a_stream_type);

			if (a_stream->curdir==DIR_C2S)
			{
				identifier_pme->pre_time_c2s=cur_time;
			}
			else if(a_stream->curdir==DIR_S2C)
			{
				identifier_pme->pre_time_s2c=cur_time;
			}
			record_ack(a_stream,pme,thread_seq,a_packet,a_stream_type);
			switch(a_stream_type)
			{
			case TCP:
				target_sni=identifier_pme->SNI;
				//未完成匹配的视频流
				if (target_sni.find("googlevideo")!=-1 && identifier_pme->video_title_type!=2)
				{
					if (identifier_pme->ack_count>=identifier_pme->video_title_ack_level*3+traffic_identify_para.word_num_long*2+1)
					{	
						//cout<<"ack level:"<<identifier_pme->video_title_ack_level<<" ack count:"<<identifier_pme->ack_count<<endl;
						identifier_pme->video_title_ack_level ++;
						lstf_video_title_identification(a_stream,pme,thread_seq,a_packet,a_stream_type);
						if (identifier_pme->video_title_type==2)
						{
							cout<<"end"<<identifier_pme->video_title_ack_level<<endl;
							return APP_STATE_DROPME;
						}
					}
				}
				break;
			case UDP:
				break;
			default:
				break;
			}
			/* 控制提取前n秒的ack块
			if (time_duration<traffic_identify_para.time_win_size)
			{
				record_ack(a_stream,pme,thread_seq,a_packet,a_stream_type);
			}
			else
			{
				switch (identifier_pme->ML_ACK_labeled_flag)
				{
					case 0:
						identifier_pme->ML_ACK_labeled_flag=1;
						if (video_tuple.find("51042")!=-1)
						{
							write_csv_ack(a_stream,pme,thread_seq,a_packet,a_stream_type);
						}
						ACK_label_fun(a_stream,pme,thread_seq,a_packet,a_stream_type);
					case 1:
						break;
					default:
						break;
				}
			}
			*/
			break;

		case OP_STATE_CLOSE:// model predict and write csv
			identifier_pme = (traffic_identify_pmeinfo*)*pme;
			target_sni=identifier_pme->SNI;
			//record_burst(a_stream,pme,thread_seq,a_packet,a_stream_type,cur_time);
			
			switch (identifier_pme->ML_ACK_labeled_flag)
			{
				case 0:
					identifier_pme->ML_ACK_labeled_flag=1;
					//对短流进行匹配
					if (target_sni.find("googlevideo")!=-1 && identifier_pme->video_title_type!=2)
					{
						cout<<"matching..."<<endl;
						//write_csv_ack_seqLen(a_stream,pme,thread_seq,a_packet,a_stream_type);
						//write_csv_ack(a_stream,pme,thread_seq,a_packet,a_stream_type);
						lstf_video_title_identification(a_stream,pme,thread_seq,a_packet,a_stream_type);
					}
					ACK_label_fun(a_stream,pme,thread_seq,a_packet,a_stream_type);
				case 1:
					break;
				default:
					break;
			}

			if(identifier_pme->payload_cnt > traffic_identify_para.min_pktsnum)
			{
				auto_label(a_stream,pme,thread_seq,a_packet,a_stream_type);
			}
			burst_label(a_stream,pme,thread_seq,a_packet,a_stream_type);
			ML_feature_record(a_stream,pme,thread_seq,a_packet,a_stream_type,pre_feature);
			
			structure_json_base(a_stream,pme,thread_seq,a_packet,a_stream_type);
			structure_json_burst(a_stream,pme,thread_seq,a_packet,a_stream_type);
			structure_json_ML(a_stream,pme,thread_seq,a_packet,a_stream_type,pre_feature);
			structure_json_label(a_stream,pme,thread_seq,a_packet,a_stream_type);
			/*
			if(traffic_identify_para.csv_record_flag == 1)
			{
				write_csv(a_stream,pme,thread_seq,a_packet,a_stream_type);
			}
			*/
			if (traffic_identify_para.use_kafka==1)
				send_kafka(a_stream,pme,thread_seq,a_packet,a_stream_type);
			release_pme(pme,thread_seq);
			return APP_STATE_DROPME;
		default:
			break;
	}
	return APP_STATE_GIVEME;
}

int TRAFFIC_IDENTIFY_AV_INIT(void)
{
    memset(&traffic_identify_para,0,sizeof(traffic_identify_parameter));

	if(traffic_identify_para_read_main_conf((char*)"./ticonf/traffic_identify_av.conf")!=0)//read main.conf init traffic par
	{
		return -1;
	}

	if(traffic_identify_para.csv_record_flag==1)//creat csv
	{
		char filename[128]={0};
		time_t curtime;
		time(&curtime);
		struct tm *lt;
		lt= localtime (&curtime);
		//snprintf(filename,128,"tilog/traffic_identify_av_%d-%d-%d-%d-%d-%d.csv",lt->tm_year+1900, lt->tm_mon+1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);
		snprintf(filename,128,"tilog/traffic_identify_av.csv");
		traffic_identify_para.file=fopen(filename,"a+");
	}
	return 0;
}

void TRAFFIC_IDENTIFY_AV_DESTROY(void)
{
	fclose(traffic_identify_para.file);
}

UCHAR TRAFFIC_IDENTIFY_AV_UDP_ENTRY(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet)
{
	//return traffic_process(a_stream,pme,thread_seq,a_packet,UDP);
	return APP_STATE_DROPME;
}

UCHAR TRAFFIC_IDENTIFY_AV_TCP_ENTRY(struct streaminfo *a_stream,  void **pme, int thread_seq,void *a_packet)
{
	return traffic_process(a_stream,pme,thread_seq,a_packet,TCP);
}