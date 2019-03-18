#include <stdio.h>
#include <winsock2.h>
#include <windows.h>
#include <pthread.h>

pthread_t threadReading;
pthread_t threadWriting;

int exitProgram = 0;

int IsExit(char * string){
    if (*string == '#' && (*string+1) == 'E' && (*string+2) == 'x' && (*string+3) == 'i' && (*string+4) == 't'){
        return 1;
    }
    else {
        return 0;
    }
}

void * ThreadReading(void * temp) {
    printf("--> Thread reading started\n");
    SOCKET s;
    s = (SOCKET)temp;
    int recv_size;
    char server_reply[2000];
    while(exitProgram == 0) {
        if ((recv_size = recv(s , server_reply , 2000 , 0)) != SOCKET_ERROR) {
            server_reply[recv_size] = '\0';
            printf(server_reply);
        }
    }
    return NULL;
}

void * ThreadWriting(void * temp) {
    SOCKET s;
    s = (SOCKET)temp;
    printf("--> Thread writing started\n");
    while(exitProgram == 0){
        char * message = malloc(sizeof(char)*2000);
        fgets(message,2000,stdin);
        send(s , message , strlen(message) , 0);
        if(IsExit(message) == 1){
            Sleep(2000);
            exitProgram = 1;
        }
    }
    return NULL;
}

int main(int argc , char *argv[]) {
    WSADATA wsa;
    SOCKET s;
    struct sockaddr_in server;
    printf("--> Initialising Winsock...\n");
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0){
        printf("Failed. Error Code : %d",WSAGetLastError());
        return 1;
    }
    printf("--> Initialised.\n");

    if((s = socket(AF_INET , SOCK_STREAM , 0 )) == INVALID_SOCKET) {
        printf("Could not create socket : %d" , WSAGetLastError());
        return 1;
    }
    printf("--> Socket created.\n");

    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons( 8080 );

    if (connect(s , (struct sockaddr *)&server , sizeof(server)) < 0){
        puts("connect error");
        return 1;
    }
    printf("--> Starting writing thread\n");
    pthread_create(&threadWriting,NULL,ThreadWriting, (void *)s);
    printf("--> Starting reading thread\n");
    pthread_create(&threadReading,NULL,ThreadReading,(void *)s);
    while (exitProgram == 0){
        continue;
    }
    printf("Closing Client");
    return 0;
}


