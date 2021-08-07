#include <winsock2.h>
#include <process.h>
#include <string.h>
#include <iostream>
#include "cache.cpp"

using namespace std;
#pragma comment(lib,"Ws2_32.lib")

#define MAXSIZE 65507 //发送数据报文的最大长度
#define HTTP_PORT 80 //http 服务器端口
#define INVALID_WEBSITE "http://www.qq.com/"          //被屏蔽的网站
#define FISHING_WEB_SRC "http://sjtu.edu.cn/"    //钓鱼的源网址
#define FISHING_WEB_DEST "http://jwts.hit.edu.cn/"    //钓鱼的目的网址
#define FISHING_WEB_HOST "jwts.hit.edu.cn"            //钓鱼目的地址的主机名

//Http 重要头部数据
struct HttpHeader{
    char method[4]; // POST 或者 GET，注意有些为 CONNECT，本实验暂 不考虑
    char url[1024]; // 请求的 url
    char host[1024]; // 目标主机
    char cookie[1024 * 10]; //cookie
    HttpHeader(){
        ZeroMemory(this,sizeof(HttpHeader));
    }
};

BOOL InitSocket();
void ParseHttpHead(char *buffer,HttpHeader * httpHeader);
BOOL ConnectToServer(SOCKET *serverSocket,char *host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);

//proxy上的Socket
SOCKET ProxyServer;
//Socket需要绑定的地址变量
sockaddr_in ProxyServerAddr;
const int ProxyPort = 8080;

//缓存相关参数
boolean haveCache = FALSE;
boolean needCache = TRUE;

struct ProxyParam{
    SOCKET clientSocket;
    SOCKET serverSocket;
};

int main(int argc, char* argv[]) {

    cout << "Proxy is running "<<endl;
    cout << "正在初始化套接字......" << endl;
    if(!InitSocket()){
        cout << "socket 初始化失败"<< endl;
        return -1;
    }
    cout << "初始化成功，正在监听PORT: " <<ProxyPort <<endl;
    SOCKET acceptSocket = INVALID_SOCKET;
    ProxyParam *lpProxyParam;
    HANDLE hThread;
    //代理服务器不断监听
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
// Qualifier: 初始化套接字
//************************************
BOOL InitSocket(){
    //加载套接字库（必须）
    WORD wVersionRequested;
    //WSADATA结构体中主要包含了系统所支持的Winsock版本信息
    WSADATA wsaData;
    //套接字加载时错误提示
    int err;
    //版本 2.2
    wVersionRequested = MAKEWORD(2, 2);
    //加载 dll 文件 Scoket 库
    err = WSAStartup(wVersionRequested, &wsaData);
    if(err != 0){
        //找不到 winsock.dll
        cout << "load winsock failed, the error ID is : "<<WSAGetLastError() <<endl;
        return FALSE;
    }
    //LOBYTE()得到一个16bit数最低（最右边）那个字节
    //HIBYTE()得到一个16bit数最高（最左边）那个字节
    //判断打开的是否是2.2版本
    if(LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) !=2)   {
        cout << "can not find the right winsock version" << endl;
        WSACleanup();
        return FALSE;
    }
    //AF_INET,PF_INET	IPv4 Internet协议
    //SOCK_STREAM	Tcp连接，提供序列化的、可靠的、双向连接的字节流。支持带外数据传输
    ProxyServer = socket(AF_INET, SOCK_STREAM, 0);
    if(INVALID_SOCKET == ProxyServer){
        cout << "creat socket is failed , the error ID is: " <<WSAGetLastError() << endl;
        return FALSE;
    }
    ProxyServerAddr.sin_family = AF_INET;       //使用主机+port地址格式
    ProxyServerAddr.sin_port = htons(ProxyPort);      //将整型变量从主机字节顺序转变成网络字节顺序

    //屏蔽用户
    //只要不是从该地址访问代理服务器的客户端，都会被该代理服务器屏蔽
    //ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;//任何IP地址均可访问
    //ProxyServerAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//仅本机IP地址可访问
    ProxyServerAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.122");  //本机IP不可访问
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
// Qualifier: 线程执行函数
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

    //解析http首部
    ParseHttpHead(CacheBuffer, httpHeader);

    //缓存
    char *DateBuffer;
    DateBuffer = (char*)malloc(MAXSIZE);
    ZeroMemory(DateBuffer, strlen(Buffer) + 1);
    memcpy(DateBuffer, Buffer, strlen(Buffer) + 1);
    char filename[100];
    ZeroMemory(filename, 100);
    // 根据url地址构造txt文件名
    makeFilename(httpHeader->url, filename);
    char *field = "Date";
    char date_str[30];  //保存字段Date的值
    ZeroMemory(date_str, 30);
    ZeroMemory(fileBuffer, MAXSIZE);
    FILE *in;
    // 在本地查询是否存在对应的缓存文件，若有，则解析Date并给Http报文段增加“if-modified-Since”字段
    if ((in = fopen(filename, "rb")) != NULL) {
        cout << "当前访问的页面有本地缓存文件！ " << endl;
        fread(fileBuffer, sizeof(char), MAXSIZE, in);
        fclose(in);
        // 将缓存文件中的Data字段存入date_str
        ParseDate(fileBuffer, field, date_str);
        cout << "该本地缓存文件的版本为： " <<date_str << endl;
        // 给HTTP报文段增加“if-modified-Since”字段
        makeNewHTTP(Buffer, date_str);
        haveCache = TRUE;
        goto success;
    }

    //网站过滤：屏蔽一个网站
    //对请求过来的 HTTP 报文头部进行检测，提取出其中的访问地址 url ，检测其是否为要被屏蔽的网址
    if (strcmp (httpHeader->url, INVALID_WEBSITE) == 0) {
        cout << "-----------------------------------------------"<< endl;
        cout << "该网站已经被屏蔽，访问失败！"<< endl;
        goto error;
    }

    //钓鱼：将访问网址转到其他网站
    if (strstr(httpHeader->url, FISHING_WEB_SRC) != NULL) {
        cout << "-----------------------------------------------"<< endl;
        cout << "钓鱼成功！已从源网址："<<FISHING_WEB_SRC << " 转到目的网址："<<FISHING_WEB_DEST << endl;
        // 通过更改 HTTP 头部字段的 url 和 host 来实现钓鱼功能
        // 第三个参数表示字符串的长度，故加1
        memcpy(httpHeader->host, FISHING_WEB_HOST, strlen(FISHING_WEB_HOST) + 1);
        memcpy(httpHeader->url, FISHING_WEB_DEST, strlen(FISHING_WEB_DEST));
    }
    delete CacheBuffer;
    delete DateBuffer;

    success:
    if(!ConnectToServer(&((ProxyParam *)lpParameter)->serverSocket,httpHeader->host)) {
        cout << "连接目标服务器失败"<< endl;
        goto error;
    }
    cout << "代理连接服务器成功！"<< endl;
    //将客户端发送的 HTTP 数据报文直接转发给目标服务器
    send(((ProxyParam *) lpParameter)->serverSocket, Buffer, strlen(Buffer) + 1, 0);
    //等待目标服务器返回数据
    recvSize = recv(((ProxyParam *)lpParameter)->serverSocket,Buffer,MAXSIZE,0);
    if(recvSize <= 0){
        cout << "返回目标服务器的数据失败! " << endl;
        goto error;
    }
    //有缓存时，判断返回的状态码是否是304，若是则将缓存的内容发送给客户端
    if (haveCache == TRUE) {
        getCache(Buffer, filename);
    }
    if (needCache == TRUE) {
        makeCache(Buffer, httpHeader->url);  //缓存报文
    }
    //将目标服务器返回的数据直接转发给客户端
    send(((ProxyParam *) lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);

    //错误处理
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
// Qualifier: 解析 TCP 报文中的 HTTP 头部，将url host等信息存入httpHeader
// Parameter: char * buffer
// Parameter: HttpHeader * httpHeader
//************************************
void ParseHttpHead(char *buffer,HttpHeader * httpHeader){
    char *p;
    const char * delim = "\r\n";
    p = strtok(buffer,delim); // 第一次调用，第一个参数为被分解的字符串
    if(p[0] == 'G'){
        //GET 方式
        memcpy(httpHeader->method,"GET",3);
        memcpy(httpHeader->url,&p[4],strlen(p) -13); //'Get' 和 'HTTP/1.1' 各占 3 和 8 个，再加上俩空格，一共13个
    }
    else if(p[0] == 'P'){
        //POST 方式
        memcpy(httpHeader->method,"POST",4);
        memcpy(httpHeader->url,&p[5],strlen(p) - 14); //'Post' 和 'HTTP/1.1' 各占 4 和 8 个，再加上俩空格，一共14个
    }
    printf("访问的url是 ： %s\n",httpHeader->url);
    // 第二次调用，需要将第一个参数设为 NULL
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
// Qualifier: 根据主机创建目标服务器套接字，并连接
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