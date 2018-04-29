#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define USER_FILE_PATH "users.txt"

// Fxn prototype
static void *clientHandler(void *ptr);
static void listenConnections();
static int splitMessage(char *ptr);
static int authenticate(char* n, char* p);
static void sendMessage();
static void receiveFile(int cs);
char* date_type_string(char *buff);
void logInfo(char *n, char* m);

// Variables
int sock, connectionSize;
struct sockaddr_in server;
char *g_usr;

pthread_mutex_t lock_logFile;
pthread_mutex_t lock_authFile;
pthread_mutex_t lock_file;

int main(int argc, char *argv[] ){
	if(argc < 2){
		printf("Port is required follow the format:\n");
		printf("./server PORT e.g. ./server 8080\n");
		exit(EXIT_FAILURE);
	}
	int PORT = atoi(argv[1]);
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1){
		printf("Socket have failed.\n");
		exit(EXIT_FAILURE);
	}
	printf("Server:> Setting up the server...\n");
	pthread_mutex_init(&lock_logFile,NULL);
	pthread_mutex_init(&lock_authFile,NULL);
	pthread_mutex_init(&lock_file,NULL);

	// Set the socket
	server.sin_port = htons(PORT);
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	// Bind the socket
	if(bind(sock, (struct sockaddr*) &server, sizeof(server)) < 0){
		perror("Server:> Could not bind socket");
		exit(EXIT_FAILURE);
	}
	while(1 == 1){
		listenConnections();
	}
	return 0;
}

// Function that listens for connections from the clients.
void listenConnections(){
	connectionSize = sizeof(struct sockaddr_in);
	listen(sock,3);
	printf("Server:> Waiting for client...\n");
	struct sockaddr_in client;
	int clientSock = accept(sock, (struct sockaddr *)&client, (socklen_t*)&connectionSize);
	if(clientSock < 0){
		printf("Server:> Cannot establish connection\n");
	}else{
		// Connection have been established
		pthread_t clientThread;
		printf("\nServer:> Client: %d connected.\n", clientSock);
		int *i = malloc(sizeof(*i));
		*i = clientSock;
		int threadReturn = pthread_create(&clientThread, NULL, clientHandler, (void*) i);
		if(threadReturn){
			printf("Server:> Failed to create a client thread. Code: %d\n", threadReturn);
		}
	}
}

// Function to handle the client thread.
void *clientHandler(void *cs){
	char message[512];
	int READSIZE = 0;
	memset(message,0,512);
	int clientSock = *((int*) cs);
	int authenticated = 0;
	printf("Client %d:> ", clientSock);
	while(strcmp(message,"EXIT") != 0){
		READSIZE = recv(clientSock, message, 512,0);
		if(READSIZE == 0){
			printf("Client %d:> Disconnected\n", clientSock);
			memset(message,0,512);
			strcpy(message,"EXIT");
		}else{
			// Check client message
			int response = splitMessage(message);
			char auth[2];
			if(response == 0){
				// Valid cridentials
				strcpy(auth, "0");
				sendMessage(clientSock,auth);
				authenticated = 1;
				memset(message,0,512);
				strcpy(message,"EXIT");
			}else if(response == -1){
				// Authentication failed
				strcpy(auth,"1");
				sendMessage(clientSock,auth);
				printf("Client %d:> Failed to authenticate, disconnecting.\n", clientSock);
				memset(message,0,512);
				strcpy(message,"EXIT");
			}
		}
	}
	// Check if user was authenticated
	if(authenticated == 1){
		// If so receive the file.
		receiveFile(clientSock);
	}
}

// Function to receive and save the file
void receiveFile(int clientSock){
	char fileBuffer[512];
	char fileName[64];
	char fileDest[128];
	// Receive file name
	recv(clientSock,fileName,64,0);
	printf("\n\nClient %d:> Receiving file: %s\n", clientSock, fileName);
	sendMessage(clientSock, "OK");
	recv(clientSock,fileDest,128,0);
	sendMessage(clientSock, "OK");
	printf("Client %d:> File department: %s\n", clientSock, fileDest);
	strcat(fileDest,"/");
	strcat(fileDest, fileName);
	pthread_mutex_lock(&lock_file);
	FILE *fp = fopen(fileDest, "w");
	if(fp == NULL){
		// Error
		char info[128];
		strcpy(info,"Tried to modify: ");
		strcat(info,fileName);
		strcat(info," at location: ");
		strcat(info,fileDest);
		logInfo(g_usr,fileDest);
	}else{
		bzero(fileBuffer,512);
		int blockSize = 0;
		int i =0;

		while((blockSize = recv(clientSock,fileBuffer,512,0)) > 0){
			printf("Client %d:> Data received: %d = %d\n",clientSock,i,blockSize);
			if(strcmp(fileBuffer,"EXIT") == 0){
				break;
			}else{
				int writeSz = fwrite(fileBuffer, sizeof(char), blockSize, fp);
				bzero(fileBuffer,512);
				i++;
			}
			sendMessage(clientSock,"OK");
		}
		printf("Client %d:> ********TRANSFER COMPLETED********\n", clientSock);
		char info[128];
		strcpy(info, "Modified file: ");
		strcat(info, fileName);
		strcat(info, " at department: ");
		strcat(info, fileDest);
		logInfo(g_usr,info);
		sendMessage(clientSock, "OK");
	}
	pthread_mutex_unlock(&lock_file);
	fclose(fp);
}

// Function to write to the client
void sendMessage(int c_sockID, char *response){
	if(write(c_sockID, response, sizeof(response)) == -1){
		// Error
		printf("Client %d:> Write() Error.\n",c_sockID);
	}
}

// Function to split the client message
int splitMessage(char *pStr){
	char *token;
	char login[64];
	char password[64];
	char str[512];
	strcpy(str, pStr);

	token = strtok(str, "|");
	if(strcmp(token, "AUTH") == 0){
		// Authentication
		token = strtok(NULL, "|");
		strcpy(login, token);
		token = strtok(NULL, "|");
		strcpy(password,token);
		int valid = authenticate(login, password);
		if(valid == 0){
			// Valid cridentials
			printf("Cridentials valid.");
			return 0;
		}else{
			// Authentication failed
			printf("Cridentials invalid.");
			return -1;
		}
	}

	printf("Done\n");
	return 1;
}


// Function that authent/icate the user.
int authenticate(char *login, char *password){
	printf("\nAuthenticating user: %s\n", login);
	g_usr = login;
	pthread_mutex_lock(&lock_authFile);
	FILE *file;
	file = fopen(USER_FILE_PATH,"r");
	char line[512];
	char ch = getc(file);
	int i =0;
	// Iterate through the file
	while( ch != EOF){
		if(ch != '\n'){
			line[i++] = ch;
		}else{
			line[i] = '|';
			line[i+1] = '\0';

			i = 0;
			char *token;
			char fileLogin[64];
			char filePassword[64];
			token = strtok(line, "|");
			strcpy(fileLogin, token);
			token = strtok(NULL, "|");
			strcpy(filePassword, token);

			// Compare login 
			if(strcmp(login, fileLogin) == 0){
				// User found, check password
				if(strcmp(password,filePassword) ==0){
					// Password correct
					logInfo(login,"Authenticated");
					fclose(file);
					pthread_mutex_unlock(&lock_authFile);
					return 0;
				}else{
					// Invalid password
					logInfo(login,"Invalid Password");
					fclose(file);
					pthread_mutex_unlock(&lock_authFile);
					return -1;
				}
			}
		}
		ch = getc(file);
	}
	logInfo(login,"Invalid Login");
	fclose(file);
	pthread_mutex_unlock(&lock_authFile);
	return -1;
}

// Function to generate a timestamp
char *date_type_string(char *buff){
	time_t the_time;
	struct tm *timeInfo;
	time(&the_time);
	timeInfo = localtime(&the_time);
	strftime(buff, 80, "%d_%m_%Y", timeInfo);
	return buff;
}
void logInfo(char *user, char *message){
	// Variables
	pthread_mutex_lock(&lock_logFile); // Lock the log file 
	FILE *fp = fopen("logs/logFile.txt", "a"); // open the file
	char buffer[256];
	char date[50];
	char *timestamp = date_type_string(date);
	if(fp == NULL){
		printf("Failed to open the log file.\n");
		pthread_mutex_unlock(&lock_logFile);
		return;
	}
	fseek(fp, 0, SEEK_END); // Move to the end of the file
	fprintf(fp,"\nUser: %s | ",user);
	fprintf(fp,"%s | ", message);
	fprintf(fp,"%s",timestamp);
	pthread_mutex_unlock(&lock_logFile); // Unlock the file
	fclose(fp);
}
