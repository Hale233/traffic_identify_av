# traffic_identify_av
---
在使用规则判断、ACK判断、Burst下行判断的基础上加入burst上行特征规则判断\
其中burst特征为上下行块的负载与包数\
通过块个数、负载大小、包数三个阈值进行burst特征判断
* 2021.12.6新加入记录拥塞窗口、流负载、下行块时间间隔特征
* 2021.12.7修改配置项名称以及修改数组大小值通过配置项确定
* 2021.12.8增加线上线下两种运行模式
