/*
* THIS FILE IS FOR IP FORWARD TEST
*/
#include "sysInclude.h"
#include <vector>
using std::vector;
#include <iostream>
using std::cout;
// system support
extern void fwd_LocalRcv(char *pBuffer, int length);

extern void fwd_SendtoLower(char *pBuffer, int length, unsigned int nexthop);

extern void fwd_DiscardPkt(char *pBuffer, int type);

extern unsigned int getIpv4Address( );

// implemented by students
// ·����ṹ��
struct routeTableItem
{
	unsigned int destIP;     // Ŀ��IP
	unsigned int mask;       // ����
	unsigned int masklen;    // ���볤��
	unsigned int nexthop;    // ��һ��
};
// vector����������·�ɱ�
vector<routeTableItem> route_table; 

void stud_Route_Init()
{
	route_table.clear();
	return;
}

void stud_route_add(stud_route_msg *proute)
{
	// �½�һ���ṹ����� 
	routeTableItem newTableItem;
	// ��һ���޷��ų��������������ֽ�˳��ת��Ϊ�����ֽ�˳��
	newTableItem.masklen = ntohl(proute->masklen); 
	newTableItem.mask = (1<<31)>>(ntohl(proute->masklen)-1);
	newTableItem.destIP = ntohl(proute->dest);
	newTableItem.nexthop = ntohl(proute->nexthop);
	// ��ӽ�·�ɱ���
	route_table.push_back(newTableItem);
	return;
}

int stud_fwd_deal(char *pBuffer, int length)
{

 	//�洢TTL
	int TTL = (int)pBuffer[8]; 
	int headerChecksum = ntohl(*(unsigned short*)(pBuffer+10)); 
	int DestIP = ntohl(*(unsigned int*)(pBuffer+16));
    int headsum = pBuffer[0] & 0xf; 

	//�жϷ����ַ�뱾����ַ�Ƿ���ͬ
	if(DestIP == getIpv4Address()) 
	{
		//�� IP �����Ͻ������ϲ�Э�� 
		fwd_LocalRcv(pBuffer, length);
		return 0;
	}

	//TTL< = 0��ʾ�����ݱ��Ѿ�ʧЧ
	if(TTL <= 0) 
	{
		fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_TTLERROR); //���� IP ����
		return 1;
	}

	//����ƥ��λ
	bool Match = false;
	unsigned int longestMatchLen = 0;
	int bestMatch = 0;
	// �ж������Ƿ�ƥ��
	for(int i = 0; i < route_table.size(); i ++)
	{
		if(route_table[i].masklen > longestMatchLen && route_table[i].destIP == (DestIP & route_table[i].mask))
		{
			bestMatch = i;
			Match = true;
			longestMatchLen = route_table[i].masklen;
		}
	}

	if(Match) //ƥ��ɹ�
	{
		char *buffer = new char[length];
            memcpy(buffer,pBuffer,length);
            buffer[8]--; //TTL - 1
		int sum = 0;
            unsigned short int localCheckSum = 0;
            for(int j = 1; j < 2 * headsum +1; j ++)
            {
                if (j != 6){ 
                    sum = sum + (buffer[(j-1)*2]<<8)+(buffer[(j-1)*2+1]);
                    sum %= 65535; 
                }
            }
            //���¼���У���
           	localCheckSum = htons(~(unsigned short int)sum);
		memcpy(buffer+10, &localCheckSum, sizeof(unsigned short));
		// ������һ��Э��
		fwd_SendtoLower(buffer, length, route_table[bestMatch].nexthop);
		return 0;
	}
	else //ƥ��ʧ��
	{
		fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_NOROUTE); //���� IP ����
		return 1;
	}
	return 1;
}
