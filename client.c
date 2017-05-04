#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>



int sock;
int connected;

void *commandHandler();

static void signal_handler(int signo) {
	if (signo == SIGINT) {
		if (sock != 0 && connected == 1) {
			write(sock, "exit\n", 5);
			close(sock);
		}
		else if (sock != 0) {
			close(sock);
		}
		exit(1);
	}
}



void main() {
	sock = 0;
	connected = 0;
	
	pthread_t tRead;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	
	if (signal(SIGINT, signal_handler) == SIG_ERR) {
		write(STDOUT_FILENO, "Cannot handle SIGINT!\n", 22);
	}

	struct sockaddr_in server;
	struct hostent *hp;
	char buff[1024];
	char input[500];
	char tempIn[500];
	int count;
	char *tokenized;
	
	count = sprintf(buff, "Welcome User! Enter 'help' for commands.\n> ");
	write(STDOUT_FILENO, buff, count);
	
	//read first input and compare with exit
	count = read(STDIN_FILENO, input, 500);
	input[strcspn(input, "\n")] = 0;
	strcpy(tempIn, input);
	//write(STDOUT_FILENO, input, count);
	
	while (strcasecmp(input, "exit") != 0) {
		input[strcspn(input, "\0")] = 10;
		tokenized = strtok(input, " \n");

		if (strcasecmp(tokenized, "connect") == 0) {
			if (connected == 1) {
				write(STDOUT_FILENO, "Already connected to server.\n", 29);
			}
			else {
				tokenized = strtok(NULL, " ");
				if (tokenized != NULL) {
					char *ipAdd = (char *)malloc(15);
					strcpy(ipAdd, tokenized);
					tokenized = strtok(NULL, " ");
					if (tokenized != NULL) {
						//create socket
						sock = socket(AF_INET, SOCK_STREAM, 0);
						if (sock < 0) {
							perror("opening stream socket");
							exit(-1);
						}
						//connect to socket using name specified by input
						//tokenized = strtok(NULL, " \n");
						server.sin_family = AF_INET;
						hp = gethostbyname(ipAdd);
						if (hp == 0) {
							//fprintf(stderr, "%s: unknown host\n", tokenized);
							char temp[30];
							count = sprintf(temp, "%s: unknown host\n", tokenized);
							write(STDOUT_FILENO, temp, count);
							//exit(2);
						}
						else {
							bcopy(hp->h_addr, &server.sin_addr, hp->h_length);
							//tokenized = strtok(NULL, " \n");
							server.sin_port = htons(atoi(tokenized));
							
							if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
								perror("Connecting stream socket");
								exit(-1);
							}
							else {
								write(STDOUT_FILENO, "Successfully connected.\n", 24);
								//write(STDOUT_FILENO, "> ", 2);
								connected = 1;
						
								int ret = pthread_create(&tRead, NULL, commandHandler, NULL);
								if (ret) {
									write(STDOUT_FILENO, "Thread creation error.\n", 23);
								}
							}
						}
						
					}
					else {
						write(STDOUT_FILENO, "Please enter complete command.\n", 31);
					}
				}
				else {
					write(STDOUT_FILENO, "Please enter complete command.\n", 31);
				}
			}
		}
		
		else if (strcasecmp(tokenized, "disconnect") == 0) {
			if (connected == 0) {
				write(STDOUT_FILENO, "Not connected to any server.\n", 29);
			}
			else {
				write(sock, "exit\n", 5); 
				close(sock);
				connected = 0;
				write(STDOUT_FILENO, "Successfully disconnected.\n", 27);
			}
		}
		
		else if (strcasecmp(tokenized, "help") == 0) {
			char temp[1000];
			count = sprintf(temp, "\n%-28s Connect to server.\n%-28s Disconnect from server.\n%-28s Add given space or comma separated values.\n%-28s Subtract given space or comma separated values.\n%-28s Multiply given space or comma separated values.\n%-28s Divide given space or comma separated values.\n%-28s List of valid commands.\n%-28s Display list of active processes.\n%-28s Display list of all processes.\n%-28s Display information of all processes.", "CONNECT <IP> <PORT>", "DISCONNECT", "ADD <VALUES>", "SUB <VALUES>", "MUL <VALUES>", "DIV <VALUES>", "HELP", "LIST", "LIST ALL", "LIST DETAILS");
			write(STDOUT_FILENO, temp, count);
			
			count = sprintf(temp, "\n%-28s Kills process with specified ID.\n%-28s Kills first process with specified name.\n%-28s Kills all processes with specified name.\n%-28s Kills all processes.\n%-28s Run specified process.\n%-28s Kills all processes and exits program.\n\n", "KILL <PROCESS ID>", "KILL <PROCESS NAME>", "KILL <PROCESS NAME> ALL", "KILL ALL", "RUN <PROCESS NAME>", "EXIT");
			write(STDOUT_FILENO, temp, count);
		}
		
		else if (connected == 1) {
			write(sock, tempIn, count);
		}
		
		else {
			write(STDOUT_FILENO, "Not connected to any server.\n", 29);
		}
		
		write(STDOUT_FILENO, "> ", 2);
		count = read(STDIN_FILENO, input, 500);
		input[strcspn(input, "\n")] = 0;
		strcpy(tempIn, input);
		//write(STDOUT_FILENO, tempIn, count);
	}
	
	if (connected) {
		write(sock, input, count);
	}
	
	close(sock);
	exit(1);
}



void *commandHandler() {
	while(1) {
		char output[1000];
		int count = read(sock, output, 1000);
		write(STDOUT_FILENO, output, count);
		output[count-1]='\0';
		if (strcasecmp(output, "Server down. Disconnected.")==0) {
			connected = 0;
		}
	}
}
