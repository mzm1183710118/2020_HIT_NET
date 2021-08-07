import random
import socket

import select

import main


class SR:

    def __init__(self, localHost, remoteHost, read_path, save_path, host1_infoFIle, host2_infoFIle):
        self.send_window_size = 4  # 窗口尺寸
        self.send_base = 0  # 最小的被发送的分组序号
        self.next_seq = 0  # 当前未被利用的序号
        self.time_out = 5  # 设置超时时间
        self.local_address = localHost  # 设置本地socket地址
        self.remote_address = remoteHost  # 设置远程socket地址
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(self.local_address)  # 绑定套接字的本地IP地址和端口号
        self.data = []  # 缓存发送数据
        self.read_path = read_path  # 需要发送的源文件数据
        self.ack_buf_size = 10
        self.get_data_from_file()

        self.rcv_window_size = 4  # 接受窗口尺寸
        self.data_buf_size = 1678  # 作为客户端接收数据缓存
        self.save_path = save_path  # 接收数据时，保存数据的地址
        self.write_data_to_file('', mode='w')

        self.pkt_loss = 0.1  # 发送数据丢包率
        self.ack_loss = 0  # 返回的ack丢包率

        self.time_counts = {}  # 存储窗口中每个发出序号的时间
        self.ack_seqs = {}  # 储存窗口中每个序号的ack情况

        self.rcv_base = 0  # 最小的需要接收的数据的分组序号
        self.rcv_data = {}  # 缓存失序的接收数据

        self.infoFIle1 = host1_infoFIle
        self.infoFIle2 = host2_infoFIle

    # 若仍剩余窗口空间，则构造数据报发送；否则拒绝发送数据
    def send_data(self):
        if self.next_seq == len(self.data):  # 判断是否还有缓存数据可以发送
            print((self.local_address, ' :发送完毕，等待确认'), file=self.infoFIle1)
            return
        if self.next_seq < self.send_base + self.send_window_size:  # 窗口中仍有可用空间
            if random.random() > self.pkt_loss:
                self.socket.sendto(main.make_pkt(self.next_seq, self.data[self.next_seq], 'data'),
                                   self.remote_address)
            self.time_counts[self.next_seq] = 0  # 设置计时器
            self.ack_seqs[self.next_seq] = False  # 设置为未接受确认包
            print((self.local_address, ' :发送数据' + str(self.next_seq)), file=self.infoFIle1)
            self.next_seq += 1
        else:  # 窗口中无可用空间
            print((self.local_address, ' :窗口已满，暂不发送数据'), file=self.infoFIle1)

    # 超时处理函数：计时器置0，设为未接受ACK，同时发送该序列号数据
    def handle_time_out(self, time_out_seq):
        print((self.local_address, '超时重传数据报:' + str(time_out_seq)), file=self.infoFIle1)
        self.time_counts[time_out_seq] = 0  # 重新定时
        if random.random() > self.pkt_loss:  # 随机发送数据包
            self.socket.sendto(main.make_pkt(time_out_seq, self.data[time_out_seq], 'data'),
                               self.remote_address)

    # 从文件中读取数据，并存储到data属性里
    def get_data_from_file(self):
        f = open(self.read_path, 'r', encoding='utf-8')
        while True:
            send_data = f.read(1024)  # 一次读取1024个字节（如果有这么多）
            if len(send_data) <= 0:
                break
            self.data.append(send_data)  # 将读取到的数据保存到data数据结构中

    # 滑动发送窗口，用于接收到最小的ack后调用
    def slide_send_window(self):
        while self.ack_seqs.get(self.send_base):  # 一直滑动到未接收到ACK的分组序号处
            del self.ack_seqs[self.send_base]  # 从dict数据结构中删除此关键字
            del self.time_counts[self.send_base]  # 从dict数据结构中删除此关键字
            self.send_base = self.send_base + 1  # 滑动窗口
            print((self.local_address, ' :窗口滑动到' + str(self.send_base)), file=self.infoFIle1)

    # 滑动接收窗口:滑动rcv_base，向上层交付数据，并清除已交付数据的缓存
    def slide_rcv_window(self):
        while self.rcv_data.get(self.rcv_base) is not None:  # 循环直到出现未接受的数据包
            self.write_data_to_file(self.rcv_data.get(self.rcv_base))  # 写入文件
            del self.rcv_data[self.rcv_base]  # 清除该缓存
            self.rcv_base = self.rcv_base + 1  # 滑动窗口
            print((self.local_address, ' :窗口滑动到' + str(self.rcv_base)), file=self.infoFIle2)

    # 保存来自服务器的合适的数据
    def write_data_to_file(self, data, mode='a'):
        with open(self.save_path, mode, encoding='utf-8') as f:
            f.write(data)

    # 客户端执行函数、
    # 不断接收服务器发送的数据，若失序则保存到缓存；若按序则滑动窗口；否则丢弃
    def client_run(self):
        while True:
            readable = select.select([self.socket], [], [], 1)[0]
            if len(readable) > 0:

                rcv_data = self.socket.recvfrom(self.data_buf_size)[0].decode()
                label, rcv_seq, rcv_data = rcv_data.split('$')
                # 接受到数据报文
                if label == 'data':
                    print((self.local_address, ' :收到数据' + rcv_seq), file=self.infoFIle2)
                    if self.rcv_base - self.rcv_window_size <= int(
                            rcv_seq) < self.rcv_base + self.rcv_window_size:
                        if self.rcv_base <= int(rcv_seq) < self.rcv_base + self.rcv_window_size:  # 序号在窗口内
                            self.rcv_data[int(rcv_seq)] = rcv_data  # 失序的数据到来:缓存+发送ack
                            if int(rcv_seq) == self.rcv_base:  # 按序数据的到来:滑动窗口并交付数据(清除对应的缓冲区)
                                self.slide_rcv_window()
                        if random.random() >= self.ack_loss:
                            self.socket.sendto(main.make_pkt(int(rcv_seq), 0, 'ACK'), self.remote_address)
                        print((self.local_address, ' :发送ACK' + rcv_seq), file=self.infoFIle2)
                else:  # 收到传输数据结束的标志
                    print((self.local_address, ' :传输数据结束'), file=self.infoFIle2)
                    break

    # 服务器端执行函数
    def server_run(self):
        while True:
            self.send_data()  # 发送数据
            readable = select.select([self.socket], [], [], 1)[0]
            if len(readable) > 0:  # 接收ACK数据
                ack_seq = self.socket.recvfrom(self.ack_buf_size)[0].decode().split('$')[1]
                if self.send_base <= int(ack_seq) < self.next_seq:  # 收到ack，则标记为已确认且超时计数为0
                    print((self.local_address, ' :收到有用ACK' + ack_seq), file=self.infoFIle1)
                    self.ack_seqs[int(ack_seq)] = True  # 设为已接受
                    if self.send_base == int(ack_seq):  # 收到的ack为最小的窗口序号
                        self.slide_send_window()  # 则滑动窗口
                else:
                    print((self.local_address, ' :收到无用ACK' + ack_seq), file=self.infoFIle1)
            for seq in self.time_counts.keys():  # 每个未接收的分组的时长都加1
                if not self.ack_seqs[seq]:  # 若未收到ACK
                    self.time_counts[seq] += 1  # 则计次+1
                    if self.time_counts[seq] > self.time_out:  # 触发超时操作
                        self.handle_time_out(seq)  # 超时处理
            if self.send_base == len(self.data):  # 数据传输结束
                self.socket.sendto(main.make_pkt(0, 0, 'end'), self.remote_address)  # 发送传输结束包
                print((self.local_address, ' :数据传输结束'), file=self.infoFIle1)
                break
