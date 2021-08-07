import random
import socket

import select

import main


class GBNClient:
    def __init__(self, localHost, remoteHost, writePath, infoFIle):
        # 设置2个主机：本机和对方主机
        self.localHost = localHost
        self.remoteHost = remoteHost
        # 设置本地UDP类型Socket
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # 绑定本机地址
        self.socket.bind(self.localHost)

        # 本机写入的文件路径
        self.write_path = writePath

        # 当前期望收到该序号的数据
        self.exp_seq = 0
        # ACK丢包率阈值
        self.ack_loss = 0

        self.infoFile = infoFIle

    def writeDataToFile(self, data):
        with open(self.write_path, 'a', encoding='utf-8') as f:
            f.write(data)

    def run_as_client(self):
        while True:
            # 非阻塞接收
            readable = select.select([self.socket], [], [], 1)[0]
            # 接收到数据
            if len(readable) > 0:
                receive_data = self.socket.recvfrom(2000)[0].decode()
                label, seq, data = receive_data.split('$')
                if label == 'data':
                    # 接收到按序数据报
                    if int(seq) == self.exp_seq:
                        print((self.localHost, ' : 收到期望序号分组数据报 ' + str(seq)), file=self.infoFile)
                        # 将数据写入到本地文件中
                        self.writeDataToFile(data)
                        self.exp_seq = self.exp_seq + 1  # 期望数据的序号更新
                    # 接收到不按序的数据报，直接丢弃
                    else:
                        print((self.localHost, ' : 收到非期望数据，期望 : ' + str(self.exp_seq) + '实际 : ' + str(seq)),
                              file=self.infoFile)
                    # 随机丢包发送ACK
                    if random.random() >= self.ack_loss:
                        self.socket.sendto(main.make_pkt(self.exp_seq - 1, 0, 'ACK'), self.remoteHost)
                # 接收到结束报
                else:
                    print((self.localHost, ' : 所有数据报都已成功接收且均发欧发送ACK，客户端关闭'), file=self.infoFile)
                    break
