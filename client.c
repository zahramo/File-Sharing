#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h> 
#include <netinet/in.h>
#include <signal.h>
#include <sys/types.h>  
#include <errno.h>  
#include <unistd.h> 
#include <fcntl.h> 
#include <sys/time.h> 
#include <unistd.h>

#define TRUE 1
#define FALSE -1
#define UPLOAD 1
#define DOWNLOAD -1
#define FILENAMELENGTH 50
#define FILEDATALENGTH 1024
#define NUMOFARGS 4
#define WAITFORSERVERTIME 3
#define  MAXNUMOFCLIENTS 20

typedef struct {
    int port;
} portStruct;


typedef struct {
    char downLoadedFileData[FILEDATALENGTH];
    int isExist;
} serverMessageStruct;

typedef struct {
    int listeningPort;
    int status;
} heartBeatMessageStruct;

typedef struct {
    char nameOfFile[FILENAMELENGTH];
    int commandType;
    char uploadFileData[FILEDATALENGTH];
} commandMessageStruct;

typedef struct {
    char nameOfFile[FILENAMELENGTH];
    int type; // 0: File, 1: Port
} broadcastMessageStruct;


int heartBeatPort, broadCastPort, clientPort, heartBeatSocket, numOfHeartBeatMessage, serverListeningPort, clientSocket, broadCastSendSocket, clientRecvSocket, broadCastRecvSocket, clientSendSocket,clientSockets[MAXNUMOFCLIENTS];
struct sockaddr_in heartBeatClientAddress, clientToServerSocketAddress, broadCastSendSocketAddress, clientRecvSocketAddress, broadCastRecvSocketAddress, clientSendSocketAddress;
heartBeatMessageStruct heartBeatMessage;
fd_set readfds;
char broadCastFileName[FILENAMELENGTH];
int stopBroadCasting = TRUE;

void setTimeForWaiting()
{
    struct timeval timeForWaiting;

    timeForWaiting.tv_sec = WAITFORSERVERTIME;
    timeForWaiting.tv_usec = 0;

    if (setsockopt(heartBeatSocket, SOL_SOCKET, SO_RCVTIMEO, &timeForWaiting, sizeof(timeForWaiting)) < 0){
        perror("set time out faild");
        exit(EXIT_FAILURE);
    }

}


void makeHeartBeatSocket()
{
    heartBeatSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (heartBeatSocket < 0) 
    { 
        perror("heartbeat socket failed\n"); 
        exit(EXIT_FAILURE); 
    }
    heartBeatClientAddress.sin_family = AF_INET;
    heartBeatClientAddress.sin_addr.s_addr = INADDR_ANY;
    heartBeatClientAddress.sin_port = htons(heartBeatPort);

    if (setsockopt(heartBeatSocket, SOL_SOCKET, SO_REUSEPORT, &heartBeatClientAddress, sizeof(heartBeatClientAddress)) < 0) 
    {
        perror("set heartbeat socket option faild\n");
        exit(EXIT_FAILURE);
    } 

    if(bind(heartBeatSocket, (struct sockaddr*) &heartBeatClientAddress, sizeof(heartBeatClientAddress)) < 0) 
    {
        perror("binding heartbeat socket faild\n");
        exit(EXIT_FAILURE); 
    }
    setTimeForWaiting();
}


int isServerAlive()
{   
    numOfHeartBeatMessage = recvfrom(heartBeatSocket, &heartBeatMessage, sizeof(heartBeatMessage) , 0, NULL, 0);
    heartBeatClientAddress.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    if(numOfHeartBeatMessage < 0)
    {
        return FALSE;
    }
    
    serverListeningPort = heartBeatMessage.listeningPort;
    return TRUE;
}


void prepareFileToUpload(char* fileName,commandMessageStruct* clientCommandMessage)
{
    int fd, numOfBytesRead;

    if((fd = open(fileName, O_RDONLY)) < 0)
    {
        perror("file does not exist\n");
        exit(EXIT_FAILURE);
    }

    numOfBytesRead = read(fd, clientCommandMessage->uploadFileData, sizeof(clientCommandMessage->uploadFileData));
    
    close(fd);
}


void addDownloadedFile(serverMessageStruct serverCommandMessage,commandMessageStruct clientCommandMessage)
{
    int readDownloadedFile;
    write(1, "Downloaded File Recieved\n", 25);
    write(1, &serverCommandMessage.downLoadedFileData, strlen(serverCommandMessage.downLoadedFileData));
    write(1, "\n",1);

    int fd, numOfBytesWrite;

    if((fd = open(clientCommandMessage.nameOfFile, O_RDONLY)) < 0)
    {
        if((fd = open(clientCommandMessage.nameOfFile, O_CREAT | O_RDWR | S_IRUSR | S_IWUSR)) < 0)
        {
            perror("can not create file\n");
            exit(EXIT_FAILURE);
        }
        else
        {
            numOfBytesWrite = write(fd, serverCommandMessage.downLoadedFileData, FILEDATALENGTH);
        }
    }
    
    close(fd);
}


void makeCommandMessage(char* commandType, char* fileName, commandMessageStruct* clientCommandMessage)
{
    strcpy(clientCommandMessage->nameOfFile,fileName);
    if (strcmp("upload",commandType) == 0)
    {
        prepareFileToUpload(fileName, clientCommandMessage);
        clientCommandMessage->commandType = UPLOAD;
    }
    else
    {
        clientCommandMessage->commandType = DOWNLOAD;   
    }
}


void parseCommands(int length, char command[], commandMessageStruct* clientCommandMessage)
{
    char curChar;
    char *commandType = (char *)malloc(sizeof(char)*6);
    char *fileName = (char *)malloc(sizeof(char)*1);
    int i;
    int j = 0;
    int k = 0;
    int isSpace = 0;
    for(i = 0; i < length; i++)
    {
        if(command[i] != ' ' && isSpace == 0)
        {
            if(j == 7)
            {
                commandType = (char *)realloc(commandType, sizeof(char)*8);
            }
            commandType[j] = command[i];
            j = j+1;
        }
        if(command[i] == ' ')
        {
            isSpace = 1;
        }
        if(command[i] != ' ' && isSpace == 1)
        {
            if (k > 1)
            {
                fileName = (char *)realloc(fileName, sizeof(char)*k);
            }
            fileName[k] = command[i];
            k = k+1;
        }

    }
    makeCommandMessage(commandType, fileName , clientCommandMessage);
}


int getCommands(commandMessageStruct* clientCommandMessage)
{
   // write(1,"12\n",3);
    char command[100];
    int numOfByteOfCommands = read(1, command, 100);
    //write(1,"13\n",3);
    command[numOfByteOfCommands - 1] = '\0';
    parseCommands(numOfByteOfCommands - 1, command , clientCommandMessage);
    return TRUE;
}




int doesFileExist(char* fileName)
{
    int fd, numOfBytesRead;

    if((fd = open(fileName, O_RDONLY)) < 0)
    {
       return FALSE;
    }
    close(fd);
    return TRUE;    
}


void makeClientSocket()
{
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) 
    { 
        perror("client socket failed\n"); 
        exit(EXIT_FAILURE); 
    }
    clientToServerSocketAddress.sin_family = AF_INET;
    clientToServerSocketAddress.sin_addr.s_addr = INADDR_ANY;
    clientToServerSocketAddress.sin_port = htons(serverListeningPort);
}




 int addConnectionsTOFDSET()
{
    int i, clientSocket;
    FD_ZERO(&readfds);   
    FD_SET(broadCastRecvSocket, &readfds);  
    FD_SET(STDIN_FILENO, &readfds);

    int maxClientSocket = broadCastRecvSocket;
    clientSockets[0] = STDIN_FILENO;
    clientSockets[1] = broadCastRecvSocket;
            
    for (i = 0; i < MAXNUMOFCLIENTS; i++)   
    {   
        clientSocket = clientSockets[i];   
        if(clientSocket > 0)
        {
            FD_SET(clientSocket, &readfds);
        }   
        if(clientSocket > maxClientSocket) 
        {
            maxClientSocket = clientSocket; 
        }  
    }
    
    return maxClientSocket;    
}


void broadCastToFindFile()
{
    int bytesTransmitted;
    broadcastMessageStruct broadCastMessage;
    strcpy(broadCastMessage.nameOfFile, broadCastFileName);
    broadCastMessage.type = 0;


    bytesTransmitted = sendto(broadCastSendSocket, &broadCastMessage, sizeof(broadCastMessage), 0, (struct sockaddr *)&broadCastSendSocketAddress, sizeof(broadCastSendSocketAddress));
    if (bytesTransmitted < 0)
    {
        perror("sending broadcast failed\n");
        exit(EXIT_FAILURE);
    }
    //printf("%d\n",bytesTransmitted);
    write(1, "broadcasting\n",13);
    if(stopBroadCasting != TRUE)
    {
        signal(SIGALRM, broadCastToFindFile);
        alarm(1);
    }
    else
    {
        close(broadCastSendSocket);
    }
}





void handleClientRequest()
{
    commandMessageStruct clientCommandMessage;
    char command[100];
    int numOfByteOfCommands;
    //printf("now we are in handleClientRequest func \n ");
    if (FD_ISSET(STDIN_FILENO, &readfds))   
    {    
        //printf("now we are in handleClientRequest func and an avent :)) \n ");
        numOfByteOfCommands = read(STDIN_FILENO, command, 100);
        command[numOfByteOfCommands - 1] = '\0';
        parseCommands(numOfByteOfCommands - 1, command , &clientCommandMessage);
        stopBroadCasting = FALSE;
        strcpy(broadCastFileName, clientCommandMessage.nameOfFile);
        //printf("%s\n",broadCastFileName);
        
        broadCastToFindFile();
    }
}


void sendClientPort()
{
    broadcastMessageStruct clientPortMessage;
    sprintf(clientPortMessage.nameOfFile, "%d", clientPort);
    clientPortMessage.type = 1;
    int bytesTransmitted;
    
    bytesTransmitted = sendto(broadCastSendSocket, &clientPortMessage, sizeof(clientPortMessage), 0, (struct sockaddr *)&broadCastSendSocketAddress, sizeof(broadCastSendSocketAddress));

    if (bytesTransmitted < 0)
    {
        perror("sending client port failed\n");
        exit(EXIT_FAILURE);
    }
    //printf("bytes : %d\n",bytesTransmitted);
    write(1,"My Port Is: ",13);
    write(1,clientPortMessage.nameOfFile,strlen(clientPortMessage.nameOfFile));
    write(1,"\n",2);
    write(1,"I Sent My Port To Other Client\n",32);

}


void makeClientRecvSocket(int otherClientPort)
{
    clientRecvSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientRecvSocket < 0) 
    { 
        perror("client recv socket failed\n"); 
        exit(EXIT_FAILURE); 
    }
    clientRecvSocketAddress.sin_family = AF_INET;
    clientRecvSocketAddress.sin_addr.s_addr = INADDR_ANY;
    clientRecvSocketAddress.sin_port = htons(otherClientPort); 
}

void getPermission(broadcastMessageStruct file)
{
    int addressLen;
    char permissionMsg;
    commandMessageStruct commandMessage;
    addressLen = sizeof(clientSendSocketAddress);
    clientSendSocket = accept(clientSendSocket,  (struct sockaddr *)&clientSendSocketAddress, (socklen_t*)&addressLen);
    write(1, "Client Connected To Client Successfully.\n", 42);
    if (( recv(clientSendSocket, &permissionMsg, 1, 0)) <= 0)   
    {   
        write(1, "Peer has been disconnected.\n", 29);
    }    
    else 
    {
        if(permissionMsg == '1')
        {
            write(1,"I Am Going To Send File To Other Client\n",41);
            prepareFileToUpload(file.nameOfFile, &commandMessage);
         // printf("Send data: %s\n", commandMessage.uploadFileData);
            int a = send(clientSendSocket, commandMessage.uploadFileData,
                        strlen(commandMessage.uploadFileData),0);
         //   printf("Num of bytes: %d\n", a);
        }
    }
}

void addrecievingFile(char* fileName,char* fileData)
{
    int readDownloadedFile;
    write(1, "Downloaded File Recieved\n", 25);
    write(1, fileData, strlen(fileData));
    write(1, "\n",1);

    int fd, numOfBytesWrite;

    if((fd = open(fileName, O_RDONLY)) < 0)
    {
       // printf("%s\n", fileName);
        if((fd = open(fileName, O_CREAT | O_RDWR | S_IRUSR | S_IWUSR)) < 0)
        {
            perror("can not create file\n");
            exit(EXIT_FAILURE);
        }
        else
        {
            numOfBytesWrite = write(fd, fileData, FILEDATALENGTH);
        }
    }
    
    close(fd);
}

void givePermission(int otherClientPort)
{
    char permissionMsg = '1';
    makeClientRecvSocket(otherClientPort);
    if (connect(clientRecvSocket, (struct sockaddr *) &clientRecvSocketAddress, sizeof(clientRecvSocketAddress)) < 0)
    {
        perror("connection to other client faild\n");
        close(clientRecvSocket);
        exit(EXIT_FAILURE); 
    }
    write(1, "I Connected To Other Client(Who has The File I Want) Successfully.\n", 68);
    
    send(clientRecvSocket, &permissionMsg, 1,0);
    write(1, "I Give Permission To Other Client(Who has The File I Want)\n",60);
    char recieveFile[FILEDATALENGTH];
    int a = recv(clientRecvSocket, recieveFile, strlen(recieveFile),0);
    recieveFile[a] = '\0';
   // printf("Num of bytes: %d\n", a);
    //printf("data of file is:%s\n********\n",recieveFile);
    addrecievingFile(broadCastFileName,recieveFile);
}

void handleOtherClientsRequests(commandMessageStruct* commandMessage)
{
    int i,numOfFileNameBytes;
    char recvClientPort[10];
    broadcastMessageStruct recvFile;
    
    if (FD_ISSET(broadCastRecvSocket, &readfds))   
    {  
        write(1,"There Is An Event On Broadcast Recv Socket\n",44);
        if(stopBroadCasting == TRUE)
        {
            write(1,"I'm Not Broadcasting For a File Now\n",37);
            numOfFileNameBytes = recvfrom(broadCastRecvSocket, &recvFile, sizeof(recvFile) , 0, NULL, 0);
            if(numOfFileNameBytes < 0)
            {
                perror("file name not recieved\n");
                exit(EXIT_FAILURE);
            }
            if (recvFile.type == 1){
                return;
            }
            write(1,"I Recieve A File Name From Other Client\n File Name Is: ",56);
            write(1,recvFile.nameOfFile,strlen(recvFile.nameOfFile));
            write(1,"\n",2);
            if(doesFileExist(recvFile.nameOfFile) == TRUE)
            {
                write(1,"I Have The File Requested By Other Client\n",43);
                sendClientPort();
                getPermission(recvFile);
            }
        }
        else 
        {
            write(1,"I'm Broadcasting For a File Now\n",37);
            numOfFileNameBytes = recvfrom(broadCastRecvSocket, &recvFile, sizeof(recvFile) , 0, NULL, 0);
            if(numOfFileNameBytes < 0)
            {
                perror("client port not recieved\n");
                exit(EXIT_FAILURE);
            }
            if (recvFile.type == 0)
                return;
            write(1,"Sombody Has The File I Need, I Get His Client Port\n",52);
            write(1,"His Client Port Is: ",21);
            write(1,recvFile.nameOfFile,strlen(recvFile.nameOfFile));
            write(1,"\n",2);
            stopBroadCasting = TRUE;
            write(1,"I Found A File That I Want, So I Stop Broadcasting\n",52);
            close(broadCastSendSocket);
            givePermission(atoi(recvFile.nameOfFile));
        }
    }
}

void makeBroadCastSendSocket()
{
    broadCastSendSocket = socket(AF_INET, SOCK_DGRAM, 0);

    broadCastSendSocketAddress.sin_family = AF_INET;
    broadCastSendSocketAddress.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    //htonl(INADDR_BROADCAST)
    broadCastSendSocketAddress.sin_port = htons(broadCastPort);

    if (broadCastSendSocket < 0) 
    { 
        perror("broadCastSendSocket failed\n"); 
        exit(EXIT_FAILURE); 
    }
    if (setsockopt(broadCastSendSocket, SOL_SOCKET, SO_REUSEPORT, &broadCastSendSocketAddress, sizeof(broadCastSendSocketAddress)) < 0) 
    {
        perror("set broadCastSendSocket option faild\n");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(broadCastSendSocket, SOL_SOCKET, SO_BROADCAST, &broadCastSendSocketAddress, sizeof(broadCastSendSocketAddress)) < 0) 
    {
        perror("set broadCastSendSocket option2 faild\n");
        exit(EXIT_FAILURE);
    }
    
}


void makebroadCastRecvSocket()
{
    broadCastRecvSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (broadCastRecvSocket < 0) 
    { 
        perror("client Listen socket failed\n"); 
        exit(EXIT_FAILURE); 
    }
    broadCastRecvSocketAddress.sin_family = AF_INET;
    broadCastRecvSocketAddress.sin_addr.s_addr = INADDR_ANY;
    broadCastRecvSocketAddress.sin_port = htons(broadCastPort);
    if (setsockopt(broadCastRecvSocket, SOL_SOCKET, SO_REUSEPORT, &broadCastRecvSocketAddress, sizeof(broadCastRecvSocketAddress)) < 0) 
    {
        perror("set client listen socket option faild\n");
        exit(EXIT_FAILURE);
    }  
    if(bind(broadCastRecvSocket, (struct sockaddr*) &broadCastRecvSocketAddress, sizeof(broadCastRecvSocketAddress)) < 0) 
    {
        perror("binding broadcast socket faild\n");
        exit(EXIT_FAILURE); 
    }    
}

void makeClientSendSocket()
{
    clientSendSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSendSocket < 0) 
    { 
        perror("client send socket failed\n"); 
        exit(EXIT_FAILURE); 
    }
    clientSendSocketAddress.sin_family = AF_INET;
    clientSendSocketAddress.sin_addr.s_addr = INADDR_ANY;
    clientSendSocketAddress.sin_port = htons(clientPort);
    if (bind(clientSendSocket, (struct sockaddr *)&clientSendSocketAddress, 
            sizeof(clientSendSocketAddress))<0){   
        perror("bind failed");   
        exit(EXIT_FAILURE);   
    }   
    struct timeval timeForWaiting;
    timeForWaiting.tv_sec = WAITFORSERVERTIME;
    timeForWaiting.tv_usec = 0;



    if (setsockopt(clientSendSocket, SOL_SOCKET, SO_RCVTIMEO, &timeForWaiting, sizeof(timeForWaiting)) < 0){
        perror("set time out faild");
        exit(EXIT_FAILURE);
    }
     
    if (listen(clientSendSocket, 3) < 0){   
        perror("listen");   
        exit(EXIT_FAILURE);   
    }   
}

void handleConnectionClientToClient(commandMessageStruct clientCommandMessage)
{
    makebroadCastRecvSocket();
    makeClientSendSocket();
    makeBroadCastSendSocket();

    int activity, i, maxSd; 
    

    for (i = 0; i < MAXNUMOFCLIENTS; i++)   
    {   
        clientSockets[i] = 0;   
    }
    write(1,"Now Client Is Waiting For Other Client's Requests..\n",53);   
         
    while(TRUE)   
    {   
        maxSd = addConnectionsTOFDSET(); 
        
        activity = select(maxSd + 1, &readfds, NULL, NULL, NULL);
        

        if (activity < 0 && (errno!=EINTR))   
        {   
            perror("select error\n");
            exit(EXIT_FAILURE);
        }  
        handleOtherClientsRequests(&clientCommandMessage);
        handleClientRequest();
    }   
}

void connectClientToServer(commandMessageStruct clientCommandMessage)
{
    serverMessageStruct serverCommandMessage;
    //getCommands();
    if (connect(clientSocket, (struct sockaddr *) &clientToServerSocketAddress, sizeof(clientToServerSocketAddress)) < 0)
    {
        perror("connection to server faild\n");
        close(clientSocket);
        exit(EXIT_FAILURE); 
    }
    write(1, "Client Connected To Server Successfully.\n", 41);
    
    send(clientSocket, &clientCommandMessage, sizeof(clientCommandMessage),0);
    if(clientCommandMessage.commandType == DOWNLOAD)
    {
        recv(clientSocket, &serverCommandMessage, sizeof(serverCommandMessage),0);
        if(serverCommandMessage.isExist)
        {
            addDownloadedFile(serverCommandMessage ,clientCommandMessage);   
        }
        else
        {
            handleConnectionClientToClient(clientCommandMessage);
        }
        
        
    }
}

void handleConnectionClientToServer(commandMessageStruct clientCommandMessage)
{
    makeClientSocket();
    connectClientToServer(clientCommandMessage);
}



int main(int argc, char const *argv[])
{
    int quit = FALSE;
    if (argc != NUMOFARGS)
    {
        perror("incorrect input\n");
        exit(EXIT_FAILURE);
    }
    heartBeatPort = atoi(argv[1]);
    broadCastPort = atoi(argv[2]);
    clientPort = atoi(argv[3]);

    makeHeartBeatSocket();
    write(1, "Please Enter Your Command [ upload/download + file name ]\n",58);
    
    commandMessageStruct clientCommandMessage;
    while (getCommands(&clientCommandMessage))
    {
        if(isServerAlive() == TRUE)
        {   
            write(1, "Server Is Alive \n", 17);
            handleConnectionClientToServer(clientCommandMessage);
            close(clientSocket);
        }
        else
        {
            write(1, "Server Is Not Alive, If You Want To Download A File From Other Clients, Please Enter Your Command [ download + file name ] \n", 125);
            handleConnectionClientToClient(clientCommandMessage);
            
        }
    }
    
}
