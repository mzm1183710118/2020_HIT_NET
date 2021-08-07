/*
 * THIS FILE IS FOR IP TEST
 * Part1 : IPV4 �����շ�
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
// ���շ��麯��
int stud_ip_recv(char *pBuffer, unsigned short length)
{
    // ��0���ֽڵ�ǰ4λ��ip version
    int version = pBuffer[0] >> 4;      
    // ��0���ֽڵĺ�4λ��  head length
    int head_len = pBuffer[0] & 0xf;    
    // ��8���ֽڣ�TTL                 
    short ttl = (unsigned short)pBuffer[8];                    
    // ��10���ֽ� ��У���
    short checksum = ntohs(*(unsigned short *)(pBuffer + 10)); 
    // ��16���ֽڣ�Ŀ��ip��ַ
    int destination = ntohl(*(unsigned int *)(pBuffer + 16));  

    // ���汾��
    if (version != 4)
    {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_VERSION_ERROR);
        return 1;
    }
    // ���ͷ������
    if (head_len < 5)
    {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_HEADLEN_ERROR);
        return 1;
    }
    // ���TTL
    if (ttl == 0)
    {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_TTL_ERROR);
        return 1;
    }
    // ���Ŀ�ĵ�ַ�ǲ��Ǳ�����ַor�㲥��ַ
    if (destination != getIpv4Address() && destination != 0xffff)
    {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_DESTINATION_ERROR);
        return 1;
    }
    // ���У���
    unsigned long sum = 0;
    unsigned long temp = 0;
    int i;
    // ÿ16bits���ж��������
    for (i = 0; i < head_len * 2; i++)
    {
        temp += (unsigned char)pBuffer[i * 2] << 8;
        temp += (unsigned char)pBuffer[i * 2 + 1];
        sum += temp;
        temp = 0;
    }
    unsigned short low_word = sum & 0xffff;
    unsigned short high_word = sum >> 16;
    // У��������򶪰�
    if (low_word + high_word != 0xffff)
    {
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_CHECKSUM_ERROR);
        return 1;
    }
    // �����򽻸����ϲ�
    ip_SendtoUp(pBuffer, length);
    return 0;
}

// ���ͷ��麯��
int stud_ip_Upsend(char *pBuffer, unsigned short len, unsigned int srcAddr,
                   unsigned int dstAddr, byte protocol, byte ttl)
{
    // ����ռ䲢�����ڴ�
    short ip_length = len + 20;
    char *buffer = (char *)malloc(ip_length * sizeof(char));
    // ��0
    memset(buffer, 0, ip_length);
    // ����Ĭ��ֵ
    buffer[0] = 0x45;
    buffer[8] = ttl;
    buffer[9] = protocol;
    // ת��Ϊ�����ֽ���
    unsigned short network_length = htons(ip_length);
    memcpy(buffer + 2, &network_length, 2);

    // ��Ŀ�ĵ�ַ��Դ��ַ��������Ӧλ��
    unsigned int src = htonl(srcAddr);
    unsigned int dst = htonl(dstAddr);
    memcpy(buffer + 12, &src, 4);
    memcpy(buffer + 16, &dst, 4);

    // ����У���
    unsigned long sum = 0;
    unsigned long temp = 0;
    int i;
    // ÿ16bits���ж��������
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
    // ��У��Ϳ�������Ӧλ��
    memcpy(buffer + 10, &header_checksum, 2);
    // ���ϲ����ݱ���������Ӧλ��
    memcpy(buffer + 20, pBuffer, len);

    // ����
    ip_SendtoLower(buffer, len + 20);
    return 0;
}
