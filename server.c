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


#define LISTENINGPORT 6099
#define TRUE 1
#define FALSE -1
#define  MAXNUMOFCLIENTS 20
#define UPLOAD 1
#define DOWNLOAD -1
#define FILENAMELENGTH 50
#define FILEDATALENGTH 1024
#define NUMOFARGS 2


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
    char downLoadedFileData[FILEDATALENGTH];
    int isExist;
} serverMessageStruct;


commandMessageStruct clientCommandMessage;
serverMessageStruct serverCommandMessage;
struct sockaddr_in heartBeatserverListenAddress, serverListenAddress;
heartBeatMessageStruct heartBeatMessage;
int heartBeatSocket, heartBeatPort, serverListenSocket, clientSockets[MAXNUMOFCLIENTS];
fd_set readfds;


void makeHeartBeatSocket()
{
    heartBeatSocket = socket(AF_INET, SOCK_DGRAM, 0);

    heartBeatserverListenAddress.sin_family = AF_INET;
    heartBeatserverListenAddress.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    //htonl(INADDR_BROADCAST)
    heartBeatserverListenAddress.sin_port = htons(heartBeatPort);

    if (heartBeatSocket < 0) 
    { 
        perror("heartbeat socket failed\n"); 
        exit(EXIT_FAILURE); 
    }
    if (setsockopt(heartBeatSocket, SOL_SOCKET, SO_REUSEADDR, &heartBeatserverListenAddress, sizeof(heartBeatserverListenAddress)) < 0) 
    {
        perror("set heartbeat socket option faild\n");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(heartBeatSocket, SOL_SOCKET, SO_BROADCAST, &heartBeatserverListenAddress, sizeof(heartBeatserverListenAddress)) < 0) 
    {
        perror("set heartbeat socket option2 faild\n");
        exit(EXIT_FAILURE);
    }
    
     
}


void sendHeartBeat()
{
    int bytesTransmitted;
    heartBeatMessage.listeningPort = LISTENINGPORT;
    heartBeatMessage.status = 1;

    bytesTransmitted = sendto(heartBeatSocket, &heartBeatMessage, sizeof(heartBeatMessage), 0, (struct sockaddr *)&heartBeatserverListenAddress, sizeof(heartBeatserverListenAddress));
    if (bytesTransmitted < 0)
    {
        perror("sending heartbeat failed\n");
        exit(EXIT_FAILURE);
    }
    
    write(1, "I'm alive \n",11);
    signal(SIGALRM, sendHeartBeat);
    alarm(1);
}


void handleHeartBeat()
{
    makeHeartBeatSocket();
    sendHeartBeat();
}


void makeServerListenSocket()
{
    serverListenSocket = socket(AF_INET , SOCK_STREAM , 0);

    if(serverListenSocket < 0)   
    {   
        perror("server listen socket failed");   
        exit(EXIT_FAILURE);   
    }

    serverListenAddress.sin_family = AF_INET;   
    serverListenAddress.sin_addr.s_addr = INADDR_ANY;   
    serverListenAddress.sin_port = htons(LISTENINGPORT);   

    if (bind(serverListenSocket, (struct sockaddr *)&serverListenAddress, sizeof(serverListenAddress)) < 0)   
    {   
        perror("server listen bind failed\n");   
        exit(EXIT_FAILURE);   
    }

    if (listen(serverListenSocket, 10) < 0)   
    {   
        perror("server listen faild");   
        exit(EXIT_FAILURE);   
    }
    //?
    write(1, "Now Server Is Listening On Port ", 31);
    //write(1, LISTENINGPORT, sizeof(LISTENINGPORT));
}


int addConnectionsTOFDSET()
{
    int i, clientSocket;
    FD_ZERO(&readfds);   
    FD_SET(serverListenSocket, &readfds);   
    int maxClientSocket = serverListenSocket;
            
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


void acceptNewConnection()
{
    int i, addressLen, newSocket;
    
    if (FD_ISSET(serverListenSocket, &readfds))   
    {    
        addressLen = sizeof(serverListenAddress);
        newSocket = accept(serverListenSocket,  (struct sockaddr *)&serverListenAddress, (socklen_t*)&addressLen);
        
        if (newSocket < 0)   
        {   
            perror("accept connection faild\n");   
            exit(EXIT_FAILURE);   
        }
        //?
        write(1, "Client Connected To Server Successfully.\n", 41);
        printf("New Connection: socket fd is %d  , port : %d \n" , newSocket , ntohs(serverListenAddress.sin_port));   
         
        for (i = 0; i < MAXNUMOFCLIENTS; i++)   
        {    
            if(clientSockets[i] == 0)   
            {   
                clientSockets[i] = newSocket;     
                break;   
            }   
        }   
    }
}


void addUploadedFile()
{
    int readUploadedFile;

    write(1, "Data Recieved\n", 14);
    write(1, &clientCommandMessage.uploadFileData, strlen(clientCommandMessage.uploadFileData));
    write(1, "\n",1);

    int fd, numOfBytesWrite;

    if((fd = open(clientCommandMessage.nameOfFile, O_RDONLY)) < 0)
    {
        if((fd = open(clientCommandMessage.nameOfFile, O_CREAT | O_RDWR | S_IRUSR | S_IWUSR)) < 0)
        {
            perror("Can Not Create File\n");
            exit(EXIT_FAILURE);
        }
        else
        {
            numOfBytesWrite = write(fd, &clientCommandMessage.uploadFileData, strlen(clientCommandMessage.uploadFileData));
        }
    }
    else
    {
        write(1, "This File Already Exist\n", 24);
    }

    close(fd);
}


void prepareDownloadedFile()
{
    int fd, numOfBytesRead;

    if((fd = open(clientCommandMessage.nameOfFile, O_RDONLY)) < 0)
    {
        write(1, "File Does Not Exist In Server's Files\n", 38);
        serverCommandMessage.isExist = 0;
    }
    else
    {
        numOfBytesRead = read(fd, serverCommandMessage.downLoadedFileData, FILEDATALENGTH);
        
        write(1, &serverCommandMessage.downLoadedFileData, strlen(serverCommandMessage.downLoadedFileData));
        write(1, "\n",1);
    
        close(fd);
        serverCommandMessage.isExist = 1;
    }
}


void talkToClients()
{
    int i, sd, readFromClient;

    for (i = 0; i < MAXNUMOFCLIENTS; i++)   
    {   
        sd = clientSockets[i];  
        if (FD_ISSET(sd , &readfds))   
        {   
            if ((readFromClient = recv(sd, &clientCommandMessage, sizeof(clientCommandMessage), 0)) == 0)   
            {   
                write(1,"A Client Disconnected\n",24);
                close(sd);   
                clientSockets[i] = 0;   
            }    
            else 
            {   
                write(1, "Command Recieved\n", 24);
                write(1, "Name Of File Is: ",18);
                write(1, &clientCommandMessage.nameOfFile, strlen(clientCommandMessage.nameOfFile));
                write(1, "\n", 1);

                if(clientCommandMessage.commandType == UPLOAD)
                {
                    write(1, "Type Of Command Is: UPLOAD\n",27);
                    addUploadedFile();
                }
                else if(clientCommandMessage.commandType == DOWNLOAD)
                {
                    write(1, "Type Of Command Is: DOWNLOAD\n",29);
                    prepareDownloadedFile();
                    send(sd, &serverCommandMessage, sizeof(serverCommandMessage),0);
                }    
            }   
        }   
    }   

}

void handleConnections()
{
    int activity, i, maxSd;           
    
    for (i = 0; i < MAXNUMOFCLIENTS; i++)   
    {   
            clientSockets[i] = 0;   
    } 

    write(1,"Now Server Is Waiting For Connections..\n",41);   
         
    while(TRUE)   
    {   
        maxSd = addConnectionsTOFDSET();   
        activity = select(maxSd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0 && (errno!=EINTR))   
        {   
            perror("select error");
            exit(EXIT_FAILURE);
        }  
       
        acceptNewConnection();
        talkToClients();     
    }   
}


int main(int argc, char const *argv[])
{
    if (argc != NUMOFARGS)
    {
        perror("incorrect input\n");
        exit(EXIT_FAILURE);
    }

    makeServerListenSocket();
    
    heartBeatPort = atoi(argv[1]);
    handleHeartBeat();
    handleConnections();    
    close(serverListenSocket);
    while(TRUE){};
    return 0;  
}











