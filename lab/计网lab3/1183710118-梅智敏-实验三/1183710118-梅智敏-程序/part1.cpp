/*
 * THIS FILE IS FOR IP TEST
 * Part1 : IPV4 分组收发
 */

#include "sysInclude.h"
#include <stdio.h>
#include <malloc.h>

// system support

extern void ip_DiscardPkt(char *pBuffer, int type);

extern void ip_SendtoLower(char *pBuffer, int length);

extern void ip_SendtoUp(char *pBuffer, int length);

extern unsigned int getIpv4Address();

// implemented by students
// 接收分组函数
int stud_ip_recv(char *pBuffer, unsigned short length)
{
    // 第0个字节的前4位：ip version
    int version = pBuffer[0] >> 4;      
    // 第0个字节的后4位：  head length
    int head_len = pBuffer[0] & 0xf;    
    // 第8个字节：TTL                 
    short ttl = (unsigned short)pBuffer[8];                    
    // 第10个字节 ：校验和
    short checksum = ntohs(*(unsigned short *)(pBuffer + 10)); 
    // 第16个字节：目的ip地址
    int destination = ntohl(*(unsigned int *)(pBuffer + 16));  

    // 检查版本号
    if (version != 4)
    {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_VERSION_ERROR);
        return 1;
    }
    // 检查头部长度
    if (head_len < 5)
    {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_HEADLEN_ERROR);
        return 1;
    }
    // 检查TTL
    if (ttl == 0)
    {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_TTL_ERROR);
        return 1;
    }
    // 检查目的地址是不是本机地址or广播地址
    if (destination != getIpv4Address() && destination != 0xffff)
    {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_DESTINATION_ERROR);
        return 1;
    }
    // 检查校验和
    unsigned long sum = 0;
    unsigned long temp = 0;
    int i;
    // 每16bits进行二进制求和
    for (i = 0; i < head_len * 2; i++)
    {
        temp += (unsigned char)pBuffer[i * 2] << 8;
        temp += (unsigned char)pBuffer[i * 2 + 1];
        sum += temp;
        temp = 0;
    }
    unsigned short low_word = sum & 0xffff;
    unsigned short high_word = sum >> 16;
    // 校验和有误，则丢包
    if (low_word + high_word != 0xffff)
    {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_CHECKSUM_ERROR);
        return 1;
    }
    // 无误则交付给上层
    ip_SendtoUp(pBuffer, length);
    return 0;
}

// 发送分组函数
int stud_ip_Upsend(char *pBuffer, unsigned short len, unsigned int srcAddr,
                   unsigned int dstAddr, byte protocol, byte ttl)
{
    // 申请空间并分配内存
    short ip_length = len + 20;
    char *buffer = (char *)malloc(ip_length * sizeof(char));
    // 置0
    memset(buffer, 0, ip_length);
    // 设置默认值
    buffer[0] = 0x45;
    buffer[8] = ttl;
    buffer[9] = protocol;
    // 转换为网络字节序
    unsigned short network_length = htons(ip_length);
    memcpy(buffer + 2, &network_length, 2);

    // 将目的地址和源地址拷贝到相应位置
    unsigned int src = htonl(srcAddr);
    unsigned int dst = htonl(dstAddr);
    memcpy(buffer + 12, &src, 4);
    memcpy(buffer + 16, &dst, 4);

    // 计算校验和
    unsigned long sum = 0;
    unsigned long temp = 0;
    int i;
    // 每16bits进行二进制求和
    for (i = 0; i < 20; i += 2)
    {
        temp += (unsigned char)buffer[i] << 8;
        temp += (unsigned char)buffer[i + 1];
        sum += temp;
        temp = 0;
    }
    unsigned short l_word = sum & 0xffff;
    unsigned short h_word = sum >> 16;
    unsigned short checksum = l_word + h_word;
    checksum = ~checksum;
    unsigned short header_checksum = htons(checksum);
    // 将校验和拷贝到相应位置
    memcpy(buffer + 10, &header_checksum, 2);
    // 将上层数据报拷贝到相应位置
    memcpy(buffer + 20, pBuffer, len);

    // 发送
    ip_SendtoLower(buffer, len + 20);
    return 0;
}
