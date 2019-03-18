#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <winsock2.h>
#include <stdlib.h>
#include "sqlite3.h"

// General Variables

int nbClient = 0;
int ring_nb = 0;
pthread_t * threadServer;
int exitServer = 0;
sqlite3 * db;
char * name_database = "users.db";
char * err_msg = 0;
char * query;
sqlite3_stmt * res;
int step;

typedef struct {
    char * pseudo;
    char * clientIp;
    char * clientPort;
    SOCKET socket;
    pthread_t * thread_client;
}client_th;

typedef struct {
    char * pseudo;
    char * ip;
    SOCKET socket;
}ring_user;

client_th listClient[50];
ring_user listRing[300];

char * ConcatString(char *s1,char *s2){
     char *s3=NULL;
     s3=(char *)malloc((strlen(s1)+strlen(s2))*sizeof(char));
     strcpy(s3,s1);
     strcat(s3,s2);
     return s3;
}

int SameString(char * string1, char * string2){
    int output = 1;
    for (int i = 0 ; i < 9 ; i++) {
        if (*(string1+i) != *(string2+i)){
            output = 0;
        }
    }
    return output;
}

void AddClient(client_th client){
    listClient[nbClient] = client;
    nbClient++;
    printf("--> Starting to check ring (total : %d)\n",ring_nb);
    printf("The new client is <ip> /%s/ <pseudo> /%s/\n",client.clientIp,client.pseudo);
    for (int i = 0 ; i < ring_nb ; i++){
        printf("Checking Ring %d <ip> /%s/ <pseudo> /%s/\n",i,listRing[i].ip,listRing[i].pseudo);
        char * message;
        char * t0 = "A client has just joined the server : ";
        if (client.pseudo != NULL && listRing[i].pseudo != NULL){
            printf("Checking if pseudos are the same... ");
            if (SameString(listRing[i].pseudo,client.pseudo) == 1){
                message = ConcatString(t0,client.pseudo);
                send(listRing[i].socket , message , strlen(message) , 0);
                printf("True\n");
            }
        }
        if (client.clientIp != NULL && listRing[i].ip != NULL){
            printf("Checking if ips are the same... ");
            if (SameString(listRing[i].ip,client.clientIp) == 1){
                message = ConcatString(t0,client.clientIp);
                send(listRing[i].socket , message , strlen(message) , 0);
                printf("True\n");
            }
        }
    }
    printf("\n");
}

void SendToAll(char * message, SOCKET s) {
    for (int i = 0 ; i < nbClient ; i++) {
        if(listClient[i].socket != s) {
            send(listClient[i].socket , message , strlen(message) , 0);
        }
    }
}

void DeleteClient(SOCKET socket) {
    for(int i = 0 ; i < nbClient ; i++) {
        client_th client = listClient[i];
        if (client.socket == socket) {
            nbClient--;
            for(int k = i ; k < nbClient; k++){
                listClient[k] = listClient[k+1];
            }
        }
    }
}

char * GetPseudoBySocket(SOCKET s) {
    for(int i = 0 ; i < nbClient ; i++){
        if (listClient[i].socket == s) {
            return listClient[i].pseudo;
        }
    }
    return NULL;
}

SOCKET GetSocketByPseudo(char * pseudo) {
    printf("Looking for socket with pseudo %s / ",pseudo);
    for(int i = 0 ; i < nbClient ; i++){
        if (*listClient[i].pseudo == *pseudo) {
            printf("Found socket\n");
            return listClient[i].socket;
        }
    }
    return NULL;
}

char * GetUser(int i){
    char * message;
    char * pseudo;
    char * ip;
    char * t1;
    char * t2;
    char * t3;
    client_th client;
    client = listClient[i];
    pseudo = client.pseudo;
    ip = client.clientIp;
    t1 = "IP : ";
    t2 = " / Pseudo : ";
    t3 = "\n";
    message = ConcatString(ConcatString(ConcatString(ConcatString(t1,ip),t2),pseudo),t3);
    printf("%s",message);
    return message;
}

int Private(char * string){
    if (*string == '#' && *(string+1) == 'P' && *(string+2) == 'r' && *(string+3) == 'i' && *(string+4) == 'v' && *(string+5) == 'a' && *(string+6) == 't' && *(string+7) == 'e') {
        if (*(string+9) == '-' && *(string+10) == 'p'){
            return 2;
        }
        if (*(string+9) == '-' && *(string+10) == 'i'){
            return 3;
        }
        return 1;
    }
    else {
        return 0;
    }
}

int ListUser(char * string){
    if (*string == '#' && *(string+1) == 'L' && *(string+2) == 'i' && *(string+3) == 's' && *(string+4) == 't' && *(string+5) == 'U'){
        return 1;
    }
    else {
        return 0;
    }
}

int Public(char * string){
    if (*string == '#' && *(string+1) == 'P' && *(string+2) == 'u' && *(string+3) == 'b' && *(string+4) == 'l' && *(string+5) == 'i' && *(string+6) == 'c'){
        return 1;
    }
    else {
        return 0;
    }
}

int Exit(char * string){
    if (*string == '#' && *(string+1) == 'E' && *(string+2) == 'x' && *(string+3) == 'i' && *(string+4) == 't'){
        return 1;
    }
    else {
        return 0;
    }
}

int Help(char * string){
    if (*string == '#' && *(string+1) == 'H' && *(string+2) == 'e' && *(string+3) == 'l' && *(string+4) == 'p'){
        return 1;
    }
    else {
        return 0;
    }
}

int Ring(char * string){
    if (*string == '#' && *(string+1) == 'R' && *(string+2) == 'i' && *(string+3) == 'n' && *(string+4) == 'g'){
        if (*(string+6) == '-' && *(string+7) == 'i') {
            return 2;
        }
        if (*(string+6) == '-' && *(string+7) == 'p') {
            return 3;
        }
        return 1;
    }
    else {
        return 0;
    }
}

// Threads

void * ThreadClient (void * temp) {
    printf("Debut d'un nouveau thread\n");
    SOCKET newSocket;
    newSocket = (SOCKET)temp;
    char * message;
    int recv_size;
    int priv;
    int command;
    priv = 0;
    SOCKET socketPrivate;
    char * pseudoPrivate = malloc(sizeof(char)*20);
    while (1) {
        char * server_reply[2000];
        if((recv_size = recv(newSocket , server_reply , 2000 , 0)) != SOCKET_ERROR) {
            server_reply[recv_size] = '\0';
            char * data;
            data = server_reply;
            message = data;
            if (ListUser(data) == 1) {
                message = "Listing all connected users : \n";
                printf("Listing users : %d users\n",nbClient);
                send(newSocket , message , strlen(message) , 0);
                for(int i = 0 ; i < nbClient ; i++) {
                    message = GetUser(i);
                    send(newSocket , message , strlen(message) , 0);
                }
                continue;
            }
            if (Public(data) == 1) {
                priv = 0;
                message = "--> Your messages are now public\n";
                send(newSocket , message , strlen(message) , 0);
                continue;
            }
            if (Help(data) == 1) {
                message = "--> Listing available commands\n";
                send(newSocket , message , strlen(message) , 0);
                message = "#Help to list all available commands\n";
                send(newSocket , message , strlen(message) , 0);
                message = "#Exit to leave the server\n";
                send(newSocket , message , strlen(message) , 0);
                message = "#Private to send private message\n";
                send(newSocket , message , strlen(message) , 0);
                message = "    -i <index> index of the user (available with the command #ListU\n";
                send(newSocket , message , strlen(message) , 0);
                message = "    -p <pseudo> pseudo of the user (available with the command #ListU\n";
                send(newSocket , message , strlen(message) , 0);
                message = "#Public to send public message\n";
                send(newSocket , message , strlen(message) , 0);
                message = "#Ring to receive an alert when a user is connect\n";
                send(newSocket , message , strlen(message) , 0);
                message = "    -i <ip> ip of the user \n";
                send(newSocket , message , strlen(message) , 0);
                message = "    -p <pseudo> pseudo of the user\n";
                send(newSocket , message , strlen(message) , 0);
                continue;
            }
            if ((command = Private(data)) != 0) {
                priv = 1;
                if (command == 3) {
                    int id = *(data+12);
                    printf("Private to Id : %d",id);
                    pseudoPrivate = listClient[(int)*(data+12)].pseudo;
                    printf("Found pseudo %s",listClient[(int)*(data+12)].pseudo);
                    socketPrivate = listClient[(int)*(data+12)].socket;
                }
                if (command == 1 || command == 2){
                    pseudoPrivate = (data + 12);
                    socketPrivate = GetSocketByPseudo(pseudoPrivate);
                }
                message = "--> Your messages are now private\n";
                send(newSocket , message , strlen(message) , 0);
                continue;
            }
            if (*data != '#' && priv == 0) {
                char * pseudo;
                char * output;
                char * t1 = " > ";
                char * t2 = "\n";
                pseudo = GetPseudoBySocket(newSocket);
                printf("Sending message from %s to all : %s\n",GetPseudoBySocket(newSocket),data);
                output = ConcatString(ConcatString(ConcatString(pseudo,t1),message),t2);
                SendToAll(output,newSocket);
                continue;
            }
            if (*data != '#' && priv == 1) {
                printf("Pseudo Private %s\n",pseudoPrivate);
                char * pseudo;
                char * output;
                char * t1 = " Wisp > ";
                char * t2 = "\n";
                pseudo = GetPseudoBySocket(newSocket);
                output = ConcatString(ConcatString(ConcatString(pseudo,t1),message),t2);
                printf("Sending message to %s : %s",pseudoPrivate,output);
                send(socketPrivate , output , strlen(output) , 0);
                continue;
            }
            if (Exit(data) == 1) {
                message = "--> You have been disconnected from the server\n";
                send(newSocket , message , strlen(message) , 0);
                DeleteClient(newSocket);
                break;
            }
            if ((command = Ring(data)) != 0){
                if (ring_nb > 500) {
                    ring_nb = 0;
                }
                printf("--> Ring command : %d\n",command);
               if (command == 2) {
                    ring_user ru;
                    ru.pseudo = NULL;
                    ru.ip = (data+9);
                    ru.socket = newSocket;
                    listRing[ring_nb] = ru;
                    message = "--> Ring created\n";
                    send(newSocket , message , strlen(message) , 0);
                    ring_nb++;
                }
                if (command == 1 || command == 3){
                    ring_user ru;
                    ru.pseudo = (data+9);
                    ru.ip = NULL;
                    ru.socket = newSocket;
                    listRing[ring_nb] = ru;
                    message = "--> Ring created\n";
                    send(newSocket , message , strlen(message) , 0);
                    ring_nb++;
                }
                continue;
            }
        }
    }
    closesocket(newSocket);
    return NULL;
}

void * ThreadServer () {
    char * command;
    while (exitServer == 0) {
        scanf("%s",&command);
        if (Exit(command) == 1) {
            exitServer = 1;
        }
    }
    return NULL;
}

int TestingConnection(SOCKET newSocket, char * clientIp , int clientPort) {
    if (newSocket == INVALID_SOCKET) {
        printf("accept failed with error code : %d" , WSAGetLastError());
        return 0;
    }
    else {
        printf("\nConnection accepted");
        printf("Client is connected from %s : %d\n",clientIp,clientPort);
        return 1;
    }
}

char * GettingPseudo(SOCKET newSocket) {
    char * message, serverReply[2000];
    int recvSize;
    message = "Type your pseudo\n";
    send(newSocket , message , strlen(message) , 0);
    while(1) {
        if((recvSize = recv(newSocket , serverReply , 2000 , 0)) != SOCKET_ERROR) {
            char * pseudo = malloc(sizeof(char)*20);
            serverReply[recvSize] = '\0';
            printf("%s joined the server\n",serverReply);
            pseudo = serverReply;
            return pseudo;
        }
    }
}

void LoadDatabase(){
    query = "DROP TABLE IF EXISTS user; CREATE TABLE user (id INT, username TEXT, password TEXT); INSERT INTO user VALUES (1,\"T\",\"pw\");";
    int rc = sqlite3_open(name_database, &db);
    printf("\nRC open Database = %d\n",rc);
    rc = sqlite3_exec(db, query, 0, 0, &err_msg);
    printf("\nRC sqltite_exec = %d -> %s \n\n",rc,query);
    sqlite3_finalize(res);
    sqlite3_close(db);
}

//Main

int main(int argc , char *argv[]){
    LoadDatabase();
    WSADATA wsa;
    SOCKET s, newSocket;
    struct sockaddr_in server, client;
    int c;
    printf("\nInitialising Winsock...");
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0){
        printf("Failed. Error Code : %d",WSAGetLastError());
        return 1;
    }
    printf("Initialised.\n");
    //Create a socket
    if((s = socket(AF_INET , SOCK_STREAM , 0 )) == INVALID_SOCKET)
    {
    printf("Could not create socket : %d" , WSAGetLastError());
    }
    printf("Socket created.\n");
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(8080);
    //Bind
    if( bind(s ,(struct sockaddr *)&server , sizeof(server)) == SOCKET_ERROR){
        printf("Bind failed with error code : %d" , WSAGetLastError());
    }
    puts("Bind done");
   //Listen to incoming connections
    listen(s , 3);
    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
    pthread_create(threadServer, NULL, ThreadServer, NULL);
    while((newSocket = accept(s , (struct sockaddr *)&client, &c)) != INVALID_SOCKET) {
        if (TestingConnection(newSocket,inet_ntoa(client.sin_addr),ntohs(client.sin_port)) == 1) {
            char * pseudo = malloc(sizeof(char)*20);
            char * message;
            char * pw;
            int b1 = 0;
            int recvSize;
            int pseudoSecured;
            pseudoSecured = 0;
            while (pseudoSecured == 0) {
                int rc = sqlite3_open(name_database, &db);
                pseudo = GettingPseudo(newSocket);
                query = "SELECT `password` FROM `user` WHERE `username` = @username";
                rc = sqlite3_prepare_v2(db, query, -1, &res, 0);
                rc = sqlite3_bind_text(res, 1, "T", -1, SQLITE_STATIC);
                printf("%s / %s\n",query,pseudo);
                if((step = sqlite3_step(res) == SQLITE_ROW)) {
                    char * serverReply[2000];
                    printf("Existing user\n");
                    char * test = sqlite3_column_text(res, 0);
                    printf("password of %s : %s\n",pseudo, test);
                    message = "This user name is already registered, enter the password :";
                    send(newSocket , message , strlen(message) , 0);
                    recvSize = recv(newSocket , serverReply , 2000 , 0);
                    if (recvSize != -1) {
                        b1 = 1;
                    }
                    while(b1 == 0) {
                        recvSize = recv(newSocket , serverReply , 2000 , 0);
                        if (recvSize != -1) {
                            b1 = 1;
                        }
                        continue;
                    }
                    printf("Password received\n");
                    pw = serverReply;
                    if (*pw == *test){
                        printf("Password confirmed\n");
                        pseudoSecured = 1;
                    }
                    else {
                        printf("Log in failed");
                    }
                }
                else {
                    printf("New user");
                    pseudoSecured = 1;
                }
            }
            sqlite3_finalize(res);
            sqlite3_close(db);
            client_th client_t;
            client_t.pseudo = pseudo;
            client_t.clientIp = inet_ntoa(client.sin_addr);
            client_t.clientPort = (char*)ntohs(client.sin_port);
            client_t.thread_client = pseudo;
            client_t.socket = newSocket;
            printf("Adding a client\n");
            AddClient(client_t);
            printf("Client added\n");
            message = "You are connected to the server\n";
            send(newSocket , message , strlen(message) , 0);
            printf("Lancement d'un thread\n");
            pthread_create(client_t.thread_client, NULL, ThreadClient, (void *) newSocket );
        }
    }
    closesocket(s);
    WSACleanup();
    return 0;
}
