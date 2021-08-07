#include <iostream>
#include <winsock2.h>
#include <string.h>
using namespace std;
#define MAXSIZE 65507 //发送数据报文的最大长度


// 将本地缓存文件的Data字段存入tempDate
boolean ParseDate(char *buffer, char *field, char *tempDate) {
    char *p, *ptr, temp[5];
    const char *delim = "\r\n";
    ZeroMemory(temp, 5);
    p = strtok(buffer, delim);
    int len = strlen(field) + 2;
    while (p) {
        if (strstr(p, field) != NULL) {
            // 获取日期Date
            memcpy(tempDate, &p[len], strlen(p) - len);
            return TRUE;
        }
        p = strtok(NULL, delim);
    }
    return TRUE;
}

//改造HTTP请求报文，增加“If-Modified-Since”字段
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
    //插入If-Modified-Since字段
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

//根据url构造本地缓存文件名
void makeFilename(char *url, char *filename) {
    while (*url != '\0') {
        if (*url != '/' && *url != ':' && *url != '.') {
            *filename++ = *url;
        }
        url++;
    }
    //本地缓存的文件名
    strcat(filename, ".txt");
}

//将内容写入本地缓存文件，以备下一次访问时直接调用
void makeCache(char *buffer, char *url) {
    char *p, *ptr, num[10], tempBuffer[MAXSIZE + 1];
    const char * delim = "\r\n";
    ZeroMemory(num, 10);
    ZeroMemory(tempBuffer, MAXSIZE + 1);
    // 将buffer内容转入tempBuffer，以准备存入本地文件
    memcpy(tempBuffer, buffer, strlen(buffer));
    p = strtok(tempBuffer, delim);//提取第一行
    memcpy(num, &p[9], 3);
    //如果缓存过期 or 没有缓存，服务器返回状态码200，因此更新本地缓存
    if (strcmp(num, "200") == 0) {
        char filename[100] = { 0 };
        //构造文件名
        makeFilename(url, filename);
        FILE *out;
        //写入本地缓存文件
        out = fopen(filename, "w");
        fwrite(buffer, sizeof(char), strlen(buffer), out);
        fclose(out);
        cout << "-------------------------------------"<< endl;
        cout << "该网页缓存成功！" << endl;
    }
}

//从本地缓存文件中获取对象
void getCache(char *buffer, char *filename) {
    char *p, num[10], tempBuffer[MAXSIZE + 1];
    const char * delim = "\r\n";
    ZeroMemory(num, 10);
    ZeroMemory(tempBuffer, MAXSIZE + 1);
    memcpy(tempBuffer, buffer, strlen(buffer));
    p = strtok(tempBuffer, delim);//提取第一行
    memcpy(num, &p[9], 3);
    // 服务器返回状态码304，表示本地有缓存文件且未过期
    // 因此proxy直接将本地缓存文件的内容发送给客户端
    if (strcmp(num, "304") == 0) {
        cout << "-------------------------------------"<< endl;
        cout << "已经从本地缓存文件获取对象"<< endl;
        ZeroMemory(buffer, strlen(buffer));
        FILE *in = NULL;
        if ((in = fopen(filename, "r")) != NULL) {
            fread(buffer, sizeof(char), MAXSIZE, in);
            fclose(in);
        }
    }

}