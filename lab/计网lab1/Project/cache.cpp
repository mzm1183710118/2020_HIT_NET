#include <iostream>
#include <winsock2.h>
#include <string.h>
using namespace std;
#define MAXSIZE 65507 //�������ݱ��ĵ���󳤶�


// �����ػ����ļ���Data�ֶδ���tempDate
boolean ParseDate(char *buffer, char *field, char *tempDate) {
    char *p, *ptr, temp[5];
    const char *delim = "\r\n";
    ZeroMemory(temp, 5);
    p = strtok(buffer, delim);
    int len = strlen(field) + 2;
    while (p) {
        if (strstr(p, field) != NULL) {
            // ��ȡ����Date
            memcpy(tempDate, &p[len], strlen(p) - len);
            return TRUE;
        }
        p = strtok(NULL, delim);
    }
    return TRUE;
}

//����HTTP�����ģ����ӡ�If-Modified-Since���ֶ�
void makeNewHTTP(char *buffer, char *value) {
    const char *field = "Host";
    const char *newfield = "If-Modified-Since: ";
    //const char *delim = "\r\n";
    char temp[MAXSIZE];
    ZeroMemory(temp, MAXSIZE);
    char *pos = strstr(buffer, field);
    int i = 0;
    for (i = 0; i < strlen(pos); i++) {
        temp[i] = pos[i];
    }
    *pos = '\0';
    //����If-Modified-Since�ֶ�
    while (*newfield != '\0') {
        *pos++ = *newfield++;
    }
    while (*value != '\0') {
        *pos++ = *value++;
    }
    *pos++ = '\r';
    *pos++ = '\n';
    for (i = 0; i < strlen(temp); i++) {
        *pos++ = temp[i];
    }
}

//����url���챾�ػ����ļ���
void makeFilename(char *url, char *filename) {
    while (*url != '\0') {
        if (*url != '/' && *url != ':' && *url != '.') {
            *filename++ = *url;
        }
        url++;
    }
    //���ػ�����ļ���
    strcat(filename, ".txt");
}

//������д�뱾�ػ����ļ����Ա���һ�η���ʱֱ�ӵ���
void makeCache(char *buffer, char *url) {
    char *p, *ptr, num[10], tempBuffer[MAXSIZE + 1];
    const char * delim = "\r\n";
    ZeroMemory(num, 10);
    ZeroMemory(tempBuffer, MAXSIZE + 1);
    // ��buffer����ת��tempBuffer����׼�����뱾���ļ�
    memcpy(tempBuffer, buffer, strlen(buffer));
    p = strtok(tempBuffer, delim);//��ȡ��һ��
    memcpy(num, &p[9], 3);
    //���������� or û�л��棬����������״̬��200����˸��±��ػ���
    if (strcmp(num, "200") == 0) {
        char filename[100] = { 0 };
        //�����ļ���
        makeFilename(url, filename);
        FILE *out;
        //д�뱾�ػ����ļ�
        out = fopen(filename, "w");
        fwrite(buffer, sizeof(char), strlen(buffer), out);
        fclose(out);
        cout << "-------------------------------------"<< endl;
        cout << "����ҳ����ɹ���" << endl;
    }
}

//�ӱ��ػ����ļ��л�ȡ����
void getCache(char *buffer, char *filename) {
    char *p, num[10], tempBuffer[MAXSIZE + 1];
    const char * delim = "\r\n";
    ZeroMemory(num, 10);
    ZeroMemory(tempBuffer, MAXSIZE + 1);
    memcpy(tempBuffer, buffer, strlen(buffer));
    p = strtok(tempBuffer, delim);//��ȡ��һ��
    memcpy(num, &p[9], 3);
    // ����������״̬��304����ʾ�����л����ļ���δ����
    // ���proxyֱ�ӽ����ػ����ļ������ݷ��͸��ͻ���
    if (strcmp(num, "304") == 0) {
        cout << "-------------------------------------"<< endl;
        cout << "�Ѿ��ӱ��ػ����ļ���ȡ����"<< endl;
        ZeroMemory(buffer, strlen(buffer));
        FILE *in = NULL;
        if ((in = fopen(filename, "r")) != NULL) {
            fread(buffer, sizeof(char), MAXSIZE, in);
            fclose(in);
        }
    }

}