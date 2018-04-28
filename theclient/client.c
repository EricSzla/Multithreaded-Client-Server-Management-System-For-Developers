#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

// Fxn prototype
static void makeConnection();
static void authenticate();
static int connection();
static int sendMessage();
static void transferFile();

int SID;
struct sockaddr_in server;
char clientMessage[512];
char serverMessage[512];
char fileDest[50];
char filePath[150];

int PORT;
char IP[16];

// Arg 1 = IP Arg 2 = PORT 
// Arg 3 = File Destination Arg 4 = file
int main(int argc, char *argv[] ){
	if(argc < 5){
		printf("Not enough Arguments - Please follow the format:\n./client IP, PORT, File's Destination, File \nE.g. ./client 127.0.0.1 8080 sales index.html\n");
		exit(EXIT_FAILURE);
	}

	PORT = atoi(argv[2]); // Set port
	if(PORT > 9999 || PORT < 100){
		printf("Invalid Port.\n");
		exit(EXIT_FAILURE);
	}
	strcpy(IP, argv[1]);  // Set IP
	strcpy(fileDest, argv[3]); // Set file Dest
	int isValidDest = 0;
	if((strcmp(fileDest, "root") == 0) || (strcmp(fileDest, "sales") == 0) || (strcmp(fileDest, "promotions") == 0) || (strcmp(fileDest, "offers") == 0) || (strcmp(fileDest, "marketing") == 0)){
		// Okay to continue 
		isValidDest = 1;
	}
	// Error checking for file destination
	if(isValidDest == 0){
		printf("Invalid file destination. Valid Destinations:\nsales\npromotions\noffers\nmarketing\n");
		exit(EXIT_FAILURE);
	}
	// set file to transfer
	strcpy(filePath, argv[4]);
	// Check file format to ensure .html
	int len = strlen(filePath);
	if(len < 5){
		printf("Invalid HTML file.\n");
		exit(EXIT_FAILURE);
	}
	const char *extenssion = &filePath[len-5];
	if(strcmp(extenssion, ".html") != 0){
		printf("Only .html files are allowed to be transfered.\n");
		exit(EXIT_FAILURE);
	}

	makeConnection();
	authenticate();
	printf("\nTransfering file, please wait...\n");
	transferFile();
	exit(0);
}


void makeConnection(){
	// Make socket
	SID = socket(AF_INET, SOCK_STREAM, 0);
	if(SID == -1){
		// error
		perror("Socket not created");
		exit(0);
	}else{
		// success
		printf("Connecting to a server, please wait...\n");
		// Init the server variables
		server.sin_port = htons(PORT);
		server.sin_addr.s_addr = inet_addr(IP);
		server.sin_family = AF_INET; // IPV4 
		// Connect to a server
		if(!connection()){
			// Connection failed, try again 3 times, otherwise quit the program.
			int counter = 3;
			while(counter != 0){
				if(connection() == 1){
					// Success
					return;
				}else{
					// Try again
					counter--;
					if(counter == 0){
						perror("Failed to connect to the server.");
						exit(EXIT_FAILURE);
					}
				}
			}
		}

		return;
	}
}

int connection(){
	if(connect(SID, (struct sockaddr *) &server, sizeof(server)) < 0 ){
		// Connection have failed
		return 0;
	}else{
		// Connection succesfull
		printf("Connected.\n\n");
		return 1;
	}
}

// Function that authenticate the user. It asks for input and runs it against the server to see if the cridentials are valid.
void authenticate(){
	char login[20];
	char *password;
	printf("Please authenticate:\n");
	printf("Login :");
	fgets(login,20,stdin);
	login[strcspn(login, "\n")] = 0;
	password = getpass("Password: ");
	if(password == NULL){
		perror("Invalid Password.");
		exit(0);
	}
	// Prepare AUTHENTICATION message
	// Format: AUTH|login|password
	memset(clientMessage, 0, strlen(clientMessage));
	strcpy(clientMessage, "AUTH|");
	strcat(clientMessage, login);
	strcat(clientMessage, "|");
	strcat(clientMessage, password);
	strcat(clientMessage, "|testing|");
	if(sendMessage() == 0){
		// Failed
		perror("Failed to authenticate");
		exit(0);

	}
	// Check response
	// Response format: 1/0
	if(strcmp(serverMessage, "0") != 0){
		// failed to authenticate
		printf("Authentication failed.\n");
		exit(0);
	}else{
		printf("Authenticated.\n");
	}
}

int sendMessage(){
	if(send(SID, clientMessage, strlen(clientMessage), 0) < 0){
		// Send failed.
		printf("Failed to contact the server..\n");
		return 0;
	}
	bzero(serverMessage, 512);
	if(recv(SID, serverMessage, 512, 0) < 0 ){
		// IO error
		printf("Failed to contect the server..\n");
		return 0;
	}
	return 1;
}

void transferFile(){
	printf("Uploading %s\n", filePath );
	FILE *file = fopen(filePath,"r+");
	bzero(clientMessage, 512);
	int blockSize, i =0;
	strcpy(clientMessage,filePath); // send file name first
	sendMessage();
	bzero(clientMessage,512);
	strcpy(clientMessage,fileDest); // Send destination e.g. sales
	sendMessage();
	bzero(clientMessage,512);

	if(!file){
		// Error
		printf("Failed to locate the file..\n");

		close(SID);
		exit(EXIT_FAILURE);
	}

	while((blockSize = fread(clientMessage, sizeof(char), 512, file)) > 0){
		printf("Data sent: %d = %d\n", i, blockSize);
		if(sendMessage() == 0){
			close(SID);
			exit(EXIT_FAILURE);
		}
		bzero(clientMessage,512);
		i++;
	}
	bzero(clientMessage,512);
	strcpy(clientMessage,"EXIT");
	sendMessage(); // Inform the server that upload is finished
	printf("\n******TRANSFER FINISHED******\n");
	close(SID);
	return;
}
