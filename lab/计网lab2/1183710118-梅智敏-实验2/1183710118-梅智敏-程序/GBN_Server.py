import random
import socket
import select

import main


class GBNServer:
    def __init__(self, localHost, remoteHost, readPath, writePath, infoFile):
        # 设置2个主机：本机和对方主机
        self.localHost = localHost
        self.remoteHost = remoteHost
        # 设置本地UDP类型Socket
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # 绑定本机地址
        self.socket.bind(self.localHost)

        # 本机读取的文件路径
        self.read_path = readPath

        # 发送窗口大小
        self.send_window = 5
        # 当前窗口的起始分组序号
        self.send_base = 0
        # 将要使用的下一个分组序号
        self.next_seq = 0
        # 发送缓存区
        self.send_buf = []
        self.getData()
        # 设置数据包丢包率阈值
        self.pkt_loss = 0
        # 时钟和最大延迟时限
        self.clock = 0
        self.maxDelay = 5

        # 本机写入的文件路径
        self.write_path = writePath

        # 当前期望收到该序号的数据
        self.exp_seq = 0
        # ACK丢包率阈值
        self.ack_loss = 0

        self.infoFile = infoFile

    # 将文件内容存入send_buf中，以列表形式存储：每一项都是1024个字节
    def getData(self):
        with open(self.read_path, 'r', encoding='utf-8') as f:
            while True:
                tmp = f.read(1024)  # 一次读取1024个字节
                if len(tmp) <= 0:  # 说明读取结束
                    break
                self.send_buf.append(tmp)  # 将读取到的数据保存到data缓存中

    # 发送数据包方法
    def sendData(self):
        # 判断数据是否传输结束
        if self.send_base == len(self.send_buf):
            # 发送结束报文
            self.socket.sendto(main.make_pkt(0, 0, 'end'), self.remoteHost)
            print((self.localHost, ' : 所有数据报都已发送结束且均接收到ACK，服务器关闭'), file=self.infoFile)
        # 若窗口仍有空间，则构造数据进行发送
        if self.next_seq < self.send_base + self.send_window:
            # 模拟丢包
            if random.random() > self.pkt_loss:
                # 构造data数据报进行发送
                self.socket.sendto(main.make_pkt(self.next_seq, self.send_buf[self.next_seq], 'data'), self.remoteHost)
                print((self.localHost, ':成功发送数据报' + str(self.next_seq)), file=self.infoFile)
                self.next_seq += 1
        else:
            print((self.localHost, "：窗口空间已满，请等待！"), file=self.infoFile)

    # 作为服务器运行
    def run_as_both(self):
        while True:
            self.sendData()
            # 非阻塞接收
            readable = select.select([self.socket], [], [], 1)[0]
            # 说明接收到ACK
            if len(readable) > 0:
                rcv_ack = self.socket.recvfrom(2000)[0].decode()
                label, seq, data = rcv_ack.split('$')
                if label == 'ACK':
                    print((self.localHost, ': 收到ACK:' + seq), file=self.infoFile)
                    # 窗口向右滑动一位
                    self.send_base = int(seq) + 1
                    # 计时器计次清0，GBN中只有一个计时器（属于send_base的）
                    self.clock = 0
                elif label == 'data':
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
                else:
                    print((self.localHost, ' : 所有数据报都已成功接收且均发欧发送ACK，客户端关闭'), file=self.infoFile)
                    break
            # 未收到ACK
            else:
                # 时钟加1，若超时则进行相关处理
                self.clock += 1
                if self.clock > self.maxDelay:
                    # 超时处理函数
                    self.handle_timeOut(self.send_base)
            # 判断数据是否传输结束
            if self.send_base == len(self.send_buf):
                # 发送结束报文
                self.socket.sendto(main.make_pkt(0, 0, 'end'), self.remoteHost)
                print((self.localHost, ' : 所有数据报都已发送结束且均接收到ACK，服务器关闭'), file=self.infoFile)
                break

    # 处理超时事件
    def handle_timeOut(self, seq):
        print((self.localHost, ': 数据报' + str(seq) + '超时，下面开始重传所有空中分组'), file=self.infoFile)
        # 发送空中的所有分组
        for i in range(self.send_base, self.next_seq):
            # 模拟丢包
            if random.random() > self.pkt_loss:
                self.socket.sendto(main.make_pkt(i, self.send_buf[i], 'data'), self.remoteHost)
            print((self.localHost, ': 已重发数据报:' + str(i)), file=self.infoFile)
        # 超时计次重启
        self.clock = 0

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
                if label == 'end':
                    print((self.localHost, ' : 所有数据报都已成功接收且均发欧发送ACK，客户端关闭'), file=self.infoFile)
                    break

    # 作为服务器运行
    def run_as_server(self):
        while True:
            self.sendData()
            # 非阻塞接收
            readable = select.select([self.socket], [], [], 1)[0]
            # 说明接收到ACK
            if len(readable) > 0:
                rcv_ack = self.socket.recvfrom(2000)[0].decode()
                label, rcv_ack_seq, data = rcv_ack.split('$')
                if label == 'ACK':
                    print((self.localHost, ': 收到ACK:' + rcv_ack_seq), file=self.infoFile)
                    # 窗口向右滑动一位
                    self.send_base = int(rcv_ack_seq) + 1
                    # 计时器计次清0，GBN中只有一个计时器（属于send_base的）
                    self.clock = 0
            # 未收到ACK
            else:
                # 时钟加1，若超时则进行相关处理
                self.clock += 1
                if self.clock > self.maxDelay:
                    # 超时处理函数
                    self.handle_timeOut(self.send_base)
            # 判断数据是否传输结束
            if self.send_base == len(self.send_buf):
                # 发送结束报文
                self.socket.sendto(main.make_pkt(0, 0, 'end'), self.remoteHost)
                print((self.localHost, ' : 所有数据报都已发送结束且均接收到ACK，服务器关闭'), file=self.infoFile)
                break
