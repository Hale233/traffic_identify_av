import os

def exists_test(path):
    isExists = os.path.exists(path)
    if not isExists:
        return False
    else:
        return True

def get_filelist(path):
    Filelist = []
    dirs = os.listdir(path)
    for dir in dirs:
        Filelist.append(os.path.join(path, dir))
    return Filelist,dirs

def get_command_sh(pcap_root_path,command_path):
    command_file = open(command_path, mode='a+', encoding='utf-8')
    Filelist, filenames = get_filelist(pcap_root_path)
    for file_path, filename in zip(Filelist, filenames):
        Filelist2, filenames2 = get_filelist(file_path)
        for file_path2, filename2 in zip(Filelist2, filenames2):
            if (file_path2.find('.pcap')>0):
                command_file.write('./sapp -d -r {} --dumpfile-speed top-speed\n'.format(file_path2))
                command_file.write('mv /home/xuminchao/sapp_run/MLdata/record.txt /home/xuminchao/traffic_identify_av/1.1/data/{}.txt\n'.format(filename2[:-5]))
    
    command_file.close()

#把文件进行标注
def label_data(label_path,fea_path,recoed_path):
    label_file=open(label_path,mode='r',encoding='utf-8')
    fea_file=open(fea_path,mode='r',encoding='utf-8')
    recoed_file=open(recoed_path,mode='a+')
    label_datas=label_file.read()
    fea_datas=fea_file.read()

    label_data=label_datas.split('\n')

    fea_data=fea_datas.split('\n')
    for fea_line in fea_data:
        label=0
        fea_vlues=fea_line[1:-2].split(',')
        if len(fea_vlues)<5:
            continue
        tuple=fea_vlues[0].split('":"')[1][:-1]+'.'+fea_vlues[2].split(':')[1]+'>'+fea_vlues[1].split('":"')[1][:-1]+'.'+fea_vlues[3].split(':')[1]
        
        for label_line in label_data:
            if label_line==tuple:
                label=1
        
        recoed_file.write(fea_line[1:-2]+',"label":'+str(label)+'\n')
    
    label_file.close()
    fea_file.close()
    recoed_file.close()

#从sapp预测的输出结果中根据SNI进行标注并提取特征行
def label_data_from_predict_log(fea_path,recoed_path):
    fea_line=[]
    index_file=open(fea_path,mode='r',encoding='utf-8')
    recoed_file=open(recoed_path,mode='w')
    index_datas = index_file.read().split('\n')
    for index_lines in index_datas:
        index_line=index_lines.split('{')
        fea_line.append(index_line[1])

    for index_data in fea_line:
        label=-1
        fea_vlues=index_data[:-2].split(',')
        if len(fea_vlues)<5:
            continue
        SNI=fea_vlues[-1][fea_vlues[-1].find(':') + 1:]
        BURST_flag=int(fea_vlues[-4][fea_vlues[-4].find(':') + 1:])
        if SNI.find('video')>=0 or SNI.find('douyinvod')>=0:
            label=1
        if SNI.find('img')>=0 or SNI.find('image')>=0 or SNI.find('pic')>=0 :
            label=0
        if BURST_flag==1 and label!=-1:
            recoed_file.write(index_data[:-2]+',"label":'+str(label)+'\n')
    
    index_file.close()
    recoed_file.close()

#合并标注特征
def combine_labeled_data():
    fea_root_path='/home/xuminchao/traffic_identify_av/1.1/data/sapp_fea/'
    label_root_path='/home/xuminchao/video_stream_identification/feature_energine/video_data_all/label/'
    recoed_path='./data/all_labeled_fea_data.txt'
    Filelist, filenames = get_filelist(fea_root_path)
    for file_path, filename in zip(Filelist, filenames):
        label_file_path=label_root_path+filename
        if exists_test(label_file_path)==False:
            label_file_path='./data/label_null.txt'
        try:
            label_data(label_file_path,file_path,recoed_path)
        except:
            print(file_path)

#get_command_sh('/mesadata/pcap/video_stream_classification/2021/tencent','./data/command.txt')
#get_command_sh('/mesadata/pcap/video_stream_classification/2021/bilibili','./data/command.txt')
#get_command_sh('/mesadata/pcap/video_stream_classification/2021/youtube','./data/command.txt')
#label_data('/home/xuminchao/video_stream_identification/feature_energine/video_data_all/label/3_14_41_38.txt','/home/xuminchao/burst_video_analysis/burst_result/sapp_fea/all_data/3_14_41_38.txt','./data/temp.txt')
combine_labeled_data()
#label_data_from_predict_log('/home/xuminchao/video_stream_identification/sapp_project/7.2/data/traffic_identify_ML_log.2021-12-06','./data/temp.txt')