import threading


def make_pkt(seq, data, label):
    if label == 'data':
        # 加上data seq 前缀
        return ('data' + '$' + str(seq) + '$' + str(data)).encode(encoding='utf-8')
    elif label == 'ACK':
        # 加上ACK seq 前缀
        return ('ACK' + '$' + str(seq)+'$' + str(0)).encode(encoding='utf-8')
    else:
        # 构造结束报文
        return ('end' + '$' + '$' + str(0)).encode(encoding='utf-8')


# 主机1
host1_address = ('127.0.0.1', 8080)
# 主机2
host2_address = ('127.0.0.1', 8081)
# 4个路径，用于双向传输
path1 = 'file/local_read_file'
path2 = 'file/local_write_file'
path3 = 'file/remote_read_file'
path4 = 'file/remote_write_file'
# 2个主机的传输信息存储文件
infoFile1 = open('out/host1_out.txt', 'a')
infoFile2 = open('out/host2_out.txt', 'a')


def GBN():
    host_1 = GBN_Server.GBNServer(host1_address, host2_address, path1, path2, infoFile1)
    host_2 = GBN_Client.GBNClient(host2_address, host1_address, path4, infoFile2)
    # 开启2个子线程，分别作为客户端和服务器，从而实现彼此通信
    threading.Thread(target=host_1.run_as_server).start()
    threading.Thread(target=host_2.run_as_client).start()


def double():
    host_1 = GBN_Server.GBNServer(host1_address, host2_address, path1, path2, infoFile1)
    host_2 = GBN_Server.GBNServer(host2_address, host1_address, path3, path4, infoFile2)
    threading.Thread(target=host_1.run_as_both).start()
    threading.Thread(target=host_2.run_as_both).start()


def sr():
    host_1 = SR.SR(host1_address, host2_address, path1, path4, infoFile1, infoFile2)
    host_2 = SR.SR(host2_address, host1_address, path1, path4, infoFile1, infoFile2)
    threading.Thread(target=host_1.server_run).start()
    threading.Thread(target=host_2.client_run).start()


if __name__ == '__main__':
    # 避免相互import的问题
    import GBN_Client
    import GBN_Server
    import SR

    choice = input('请选择要运行的协议:输入‘GBN’表示运行GBN，输入‘double’表示双向传输, ‘SR’表示运行SR协议')
    if choice.upper() == 'GBN':
        GBN()
    elif choice.upper() == 'DOUBLE':
        double()
    elif choice.upper() == 'SR':
        sr()
    else:
        print('输入非法字串，请重新运行程序')
