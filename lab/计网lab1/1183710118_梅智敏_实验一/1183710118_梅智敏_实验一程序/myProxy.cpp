#include <winsock2.h>
#include <process.h>
#include <string.h>
#include <iostream>
#include "cache.cpp"

using namespace std;
#pragma comment(lib,"Ws2_32.lib")

#define MAXSIZE 65507 //�������ݱ��ĵ���󳤶�
#define HTTP_PORT 80 //http �������˿�
#define INVALID_WEBSITE "http://www.qq.com/"          //�����ε���վ
#define FISHING_WEB_SRC "http://sjtu.edu.cn/"    //�����Դ��ַ
#define FISHING_WEB_DEST "http://jwts.hit.edu.cn/"    //�����Ŀ����ַ
#define FISHING_WEB_HOST "jwts.hit.edu.cn"            //����Ŀ�ĵ�ַ��������

//Http ��Ҫͷ������
struct HttpHeader{
    char method[4]; // POST ���� GET��ע����ЩΪ CONNECT����ʵ���� ������
    char url[1024]; // ����� url
    char host[1024]; // Ŀ������
    char cookie[1024 * 10]; //cookie
    HttpHeader(){
        ZeroMemory(this,sizeof(HttpHeader));
    }
};

BOOL InitSocket();
void ParseHttpHead(char *buffer,HttpHeader * httpHeader);
BOOL ConnectToServer(SOCKET *serverSocket,char *host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);

//proxy�ϵ�Socket
SOCKET ProxyServer;
//Socket��Ҫ�󶨵ĵ�ַ����
sockaddr_in ProxyServerAddr;
const int ProxyPort = 8080;

//������ز���
boolean haveCache = FALSE;
boolean needCache = TRUE;

struct ProxyParam{
    SOCKET clientSocket;
    SOCKET serverSocket;
};

int main(int argc, char* argv[]) {

    cout << "Proxy is running "<<endl;
    cout << "���ڳ�ʼ���׽���......" << endl;
    if(!InitSocket()){
        cout << "socket ��ʼ��ʧ��"<< endl;
        return -1;
    }
    cout << "��ʼ���ɹ������ڼ���PORT: " <<ProxyPort <<endl;
    SOCKET acceptSocket = INVALID_SOCKET;
    ProxyParam *lpProxyParam;
    HANDLE hThread;
    //������������ϼ���
    while(true){
        acceptSocket = accept(ProxyServer,NULL,NULL);
        lpProxyParam = new ProxyParam;
        if(lpProxyParam == NULL){
            continue;
        }
        lpProxyParam->clientSocket = acceptSocket;
        hThread = (HANDLE)_beginthreadex(NULL, 0, &ProxyThread,(LPVOID)lpProxyParam, 0, 0);
        CloseHandle(hThread);
        Sleep(500);
    }
}

//************************************
// Method:    InitSocket
// FullName:  InitSocket
// Access:    public
// Returns:   BOOL
// Qualifier: ��ʼ���׽���
//************************************
BOOL InitSocket(){
    //�����׽��ֿ⣨���룩
    WORD wVersionRequested;
    //WSADATA�ṹ������Ҫ������ϵͳ��֧�ֵ�Winsock�汾��Ϣ
    WSADATA wsaData;
    //�׽��ּ���ʱ������ʾ
    int err;
    //�汾 2.2
    wVersionRequested = MAKEWORD(2, 2);
    //���� dll �ļ� Scoket ��
    err = WSAStartup(wVersionRequested, &wsaData);
    if(err != 0){
        //�Ҳ��� winsock.dll
        cout << "load winsock failed, the error ID is : "<<WSAGetLastError() <<endl;
        return FALSE;
    }
    //LOBYTE()�õ�һ��16bit����ͣ����ұߣ��Ǹ��ֽ�
    //HIBYTE()�õ�һ��16bit����ߣ�����ߣ��Ǹ��ֽ�
    //�жϴ򿪵��Ƿ���2.2�汾
    if(LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) !=2)   {
        cout << "can not find the right winsock version" << endl;
        WSACleanup();
        return FALSE;
    }
    //AF_INET,PF_INET	IPv4 InternetЭ��
    //SOCK_STREAM	Tcp���ӣ��ṩ���л��ġ��ɿ��ġ�˫�����ӵ��ֽ�����֧�ִ������ݴ���
    ProxyServer = socket(AF_INET, SOCK_STREAM, 0);
    if(INVALID_SOCKET == ProxyServer){
        cout << "creat socket is failed , the error ID is: " <<WSAGetLastError() << endl;
        return FALSE;
    }
    ProxyServerAddr.sin_family = AF_INET;       //ʹ������+port��ַ��ʽ
    ProxyServerAddr.sin_port = htons(ProxyPort);      //�����ͱ����������ֽ�˳��ת��������ֽ�˳��

    //�����û�
    //ֻҪ���ǴӸõ�ַ���ʴ���������Ŀͻ��ˣ����ᱻ�ô������������
    //ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;//�κ�IP��ַ���ɷ���
    //ProxyServerAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//������IP��ַ�ɷ���
    ProxyServerAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.122");  //����IP���ɷ���
    if(bind(ProxyServer,(SOCKADDR*)&ProxyServerAddr,sizeof(SOCKADDR)) == SOCKET_ERROR){
        cout << "bind socket is failed " << endl;
        return FALSE;
    }
    if(listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR){
        cout << "listen Port"<< ProxyPort <<" is failed "<< endl;
        return FALSE;
    }
    return TRUE;
}
//************************************
// Method:    ProxyThread
// FullName:  ProxyThread
// Access:    public
// Returns:   unsigned int __stdcall
// Qualifier: �߳�ִ�к���
// Parameter: LPVOID lpParameter
//************************************
unsigned int __stdcall ProxyThread(LPVOID lpParameter){
    char Buffer[MAXSIZE], fileBuffer[MAXSIZE];
    char *CacheBuffer;
    HttpHeader* httpHeader = new HttpHeader();
    ZeroMemory(Buffer,MAXSIZE);
    SOCKADDR_IN clientAddr;
    int recvSize;
    recvSize = recv(((ProxyParam *)lpParameter)->clientSocket,Buffer,MAXSIZE,0);
    CacheBuffer = new char[recvSize + 1];
    ZeroMemory(CacheBuffer, recvSize + 1);
    memcpy(CacheBuffer, Buffer, recvSize);

    //����http�ײ�
    ParseHttpHead(CacheBuffer, httpHeader);

    //����
    char *DateBuffer;
    DateBuffer = (char*)malloc(MAXSIZE);
    ZeroMemory(DateBuffer, strlen(Buffer) + 1);
    memcpy(DateBuffer, Buffer, strlen(Buffer) + 1);
    char filename[100];
    ZeroMemory(filename, 100);
    // ����url��ַ����txt�ļ���
    makeFilename(httpHeader->url, filename);
    char *field = "Date";
    char date_str[30];  //�����ֶ�Date��ֵ
    ZeroMemory(date_str, 30);
    ZeroMemory(fileBuffer, MAXSIZE);
    FILE *in;
    // �ڱ��ز�ѯ�Ƿ���ڶ�Ӧ�Ļ����ļ������У������Date����Http���Ķ����ӡ�if-modified-Since���ֶ�
    if ((in = fopen(filename, "rb")) != NULL) {
        cout << "��ǰ���ʵ�ҳ���б��ػ����ļ��� " << endl;
        fread(fileBuffer, sizeof(char), MAXSIZE, in);
        fclose(in);
        // �������ļ��е�Data�ֶδ���date_str
        ParseDate(fileBuffer, field, date_str);
        cout << "�ñ��ػ����ļ��İ汾Ϊ�� " <<date_str << endl;
        // ��HTTP���Ķ����ӡ�if-modified-Since���ֶ�
        makeNewHTTP(Buffer, date_str);
        haveCache = TRUE;
        goto success;
    }

    //��վ���ˣ�����һ����վ
    //����������� HTTP ����ͷ�����м�⣬��ȡ�����еķ��ʵ�ַ url ��������Ƿ�ΪҪ�����ε���ַ
    if (strcmp (httpHeader->url, INVALID_WEBSITE) == 0) {
        cout << "-----------------------------------------------"<< endl;
        cout << "����վ�Ѿ������Σ�����ʧ�ܣ�"<< endl;
        goto error;
    }

    //���㣺��������ַת��������վ
    if (strstr(httpHeader->url, FISHING_WEB_SRC) != NULL) {
        cout << "-----------------------------------------------"<< endl;
        cout << "����ɹ����Ѵ�Դ��ַ��"<<FISHING_WEB_SRC << " ת��Ŀ����ַ��"<<FISHING_WEB_DEST << endl;
        // ͨ������ HTTP ͷ���ֶε� url �� host ��ʵ�ֵ��㹦��
        // ������������ʾ�ַ����ĳ��ȣ��ʼ�1
        memcpy(httpHeader->host, FISHING_WEB_HOST, strlen(FISHING_WEB_HOST) + 1);
        memcpy(httpHeader->url, FISHING_WEB_DEST, strlen(FISHING_WEB_DEST));
    }
    delete CacheBuffer;
    delete DateBuffer;

    success:
    if(!ConnectToServer(&((ProxyParam *)lpParameter)->serverSocket,httpHeader->host)) {
        cout << "����Ŀ�������ʧ��"<< endl;
        goto error;
    }
    cout << "�������ӷ������ɹ���"<< endl;
    //���ͻ��˷��͵� HTTP ���ݱ���ֱ��ת����Ŀ�������
    send(((ProxyParam *) lpParameter)->serverSocket, Buffer, strlen(Buffer) + 1, 0);
    //�ȴ�Ŀ���������������
    recvSize = recv(((ProxyParam *)lpParameter)->serverSocket,Buffer,MAXSIZE,0);
    if(recvSize <= 0){
        cout << "����Ŀ�������������ʧ��! " << endl;
        goto error;
    }
    //�л���ʱ���жϷ��ص�״̬���Ƿ���304�������򽫻�������ݷ��͸��ͻ���
    if (haveCache == TRUE) {
        getCache(Buffer, filename);
    }
    if (needCache == TRUE) {
        makeCache(Buffer, httpHeader->url);  //���汨��
    }
    //��Ŀ����������ص�����ֱ��ת�����ͻ���
    send(((ProxyParam *) lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);

    //������
    error:
    cout << "close socket"<< endl;
    Sleep(500);
    closesocket(((ProxyParam*)lpParameter)->clientSocket);
    closesocket(((ProxyParam*)lpParameter)->serverSocket);
    delete  lpParameter;
    _endthreadex(0);
}
//************************************
// Method:    ParseHttpHead
// FullName:  ParseHttpHead
// Access:    public
// Returns:   void
// Qualifier: ���� TCP �����е� HTTP ͷ������url host����Ϣ����httpHeader
// Parameter: char * buffer
// Parameter: HttpHeader * httpHeader
//************************************
void ParseHttpHead(char *buffer,HttpHeader * httpHeader){
    char *p;
    const char * delim = "\r\n";
    p = strtok(buffer,delim); // ��һ�ε��ã���һ������Ϊ���ֽ���ַ���
    if(p[0] == 'G'){
        //GET ��ʽ
        memcpy(httpHeader->method,"GET",3);
        memcpy(httpHeader->url,&p[4],strlen(p) -13); //'Get' �� 'HTTP/1.1' ��ռ 3 �� 8 �����ټ������ո�һ��13��
    }
    else if(p[0] == 'P'){
        //POST ��ʽ
        memcpy(httpHeader->method,"POST",4);
        memcpy(httpHeader->url,&p[5],strlen(p) - 14); //'Post' �� 'HTTP/1.1' ��ռ 4 �� 8 �����ټ������ո�һ��14��
    }
    printf("���ʵ�url�� �� %s\n",httpHeader->url);
    // �ڶ��ε��ã���Ҫ����һ��������Ϊ NULL
    p = strtok(NULL,delim);
    while(p){
        switch(p[0]){
            case 'H'://Host
                memcpy(httpHeader->host,&p[6],strlen(p) - 6);
                break;
            case 'C'://Cookie
                if(strlen(p) > 8){
                    char header[8];
                    ZeroMemory(header,sizeof(header));
                    memcpy(header,p,6);
                    if(!strcmp(header,"Cookie")){
                        memcpy(httpHeader->cookie,&p[8],strlen(p) -8);
                    }
                }
                break;
            default:
                break;
        }
        p = strtok(NULL,delim);
    }
}
//************************************
// Method:    ConnectToServer
// FullName:  ConnectToServer
// Access:    public
// Returns:   BOOL
// Qualifier: ������������Ŀ��������׽��֣�������
// Parameter: SOCKET * serverSocket
// Parameter: char * host
//************************************
BOOL ConnectToServer(SOCKET *serverSocket,char *host){
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(HTTP_PORT);
    HOSTENT *hostent = gethostbyname(host);
    if(!hostent){
        return FALSE;
    }
    in_addr Inaddr = *( (in_addr*) *hostent->h_addr_list);
    serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
    *serverSocket = socket(AF_INET,SOCK_STREAM,0);
    if(*serverSocket == INVALID_SOCKET){
        return FALSE;
    }
    if(connect(*serverSocket,(SOCKADDR *)&serverAddr,sizeof(serverAddr)) == SOCKET_ERROR){
        closesocket(*serverSocket);
        return FALSE;
    }
    return TRUE;
}