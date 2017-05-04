#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <arpa/inet.h>
#define TRUE 1
#define BUFF_QRT 512
#define BUFF_HALF 2048
#define BUFF_SIZE 4096
#define READ_END 0
#define WRITE_END 1



typedef struct{
	char *IP;
	int port;
	char *status;
	struct sockaddr_in cli_addr;
	int msgsock;
	socklen_t clilen;
	int pipefd[2];
	int pipefd2[2];
	pid_t workerProcess;
}CLIENT;

int cNum;
CLIENT *cList;

void *commandHandler();
void listClients();
void listProcesses(char *ip, char *port, int check);
void killClient(char *ip, char *port, int all);
void killProcessWithID(int pid);
void broadcast(char *msg, int length);

static void signal_handler(int signo) {
	if (signo == SIGCHLD) {
		int status;
		pid_t tempPID = waitpid(-1, &status, WNOHANG);
		if (tempPID != 0) {
			killProcessWithID(tempPID);
		}
	}
	if (signo == SIGINT) {
		killClient(NULL, NULL, 1);
		exit(1);
	}
}



void main() {
	cNum = -1;
	cList = (CLIENT *)malloc(20*sizeof(CLIENT));
		
	if (signal(SIGCHLD, signal_handler) == SIG_ERR) {
		write(STDOUT_FILENO, "Cannot handle SIGCHLD!\n", 23);
	}
	if (signal(SIGINT, signal_handler) == SIG_ERR) {
		write(STDOUT_FILENO, "Cannot handle SIGINT!\n", 22);
	}
	
	int sock, length;
	struct sockaddr_in server;
	//int msgsock;
	char buff[BUFF_SIZE];
	int rval;
	int i;
	int count;

	
	//create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("opening stream socket");
		exit(-1);
	}
	
	//name socket using wildcards
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = 0;
	if (bind(sock, (struct sockaddr *) &server, sizeof(server))) {
		perror("binding stream socket");
		exit(-1);
	}
	
	//find out assigned port number and print it out
	length = sizeof(server);
	if (getsockname(sock, (struct sockaddr *) &server, &length)) {
		perror("getting socket name");
		exit(-1);
	}
	count = sprintf(buff, "Socket has port #%d\n", ntohs(server.sin_port));
	write(STDOUT_FILENO, buff, count);
	
	//start accepting connections
	listen(sock, 5);
	
	//thread to listen to stdin
	pthread_t tRead;
	int ret = pthread_create(&tRead, NULL, commandHandler, NULL);
	if (ret) {
		write(STDOUT_FILENO, "Thread creation error.\n", 23);
		exit(-1);
	}	
	
	do {
		cNum++;
		cList[cNum].clilen = sizeof(cList[cNum].cli_addr);
		cList[cNum].msgsock = accept(sock, (struct sockaddr *)&cList[cNum].cli_addr, &cList[cNum].clilen);
		cList[cNum].status = (char *)malloc(10);
		cList[cNum].IP = inet_ntoa(cList[cNum].cli_addr.sin_addr);
		cList[cNum].port = ntohs(cList[cNum].cli_addr.sin_port);
		//count = sprintf(buff, "Client IP %s and client port: %d\n", cList[cNum].IP, cList[cNum].port);
		//write(STDOUT_FILENO, buff, count);
		
		int pipeRet = pipe(cList[cNum].pipefd);
		if (pipeRet < 0) {
			perror("pipe");
			exit(-1);
		}
		pipeRet = pipe(cList[cNum].pipefd2);
		if (pipeRet < 0) {
			perror("pipe");
			exit(-1);
		}
		
		
		pid_t pid = fork();
		if (pid == 0) {		//child
			close(cList[cNum].pipefd[READ_END]);	//writes in pipefd[1]
			close(cList[cNum].pipefd2[WRITE_END]);	//reads in pipefd2[0]
			char pipeRead[5];
			char pipeWrite[5];
			sprintf(buff, "%d\n", msgsock);
			sprintf(pipeRead, "%d\n", cList[cNum].pipefd2[READ_END]);
			sprintf(pipeWrite, "%d\n", cList[cNum].pipefd[WRITE_END]);
			int check = execl("/home/tjghani/Desktop/server", "server", buff, pipeRead, pipeWrite, NULL);
			if (check == -1) {
				write(msgsock, "Server failed to launch. Try again.\n", 36);
				close(cList[cNum].msgsock);
				perror("execl");
				exit(-1);
			}
		}
		else if (pid > 0) {
			close(cList[cNum].pipefd[WRITE_END]);	//read from pipefd[0]
			close(cList[cNum].pipefd2[READ_END]);	//write from pipefd2[1]
			cList[cNum].workerProcess = pid;
			strcpy(cList[cNum].status, "active");
			
		}
		else {
			perror("fork");
			exit(-1);
		}
	} while (TRUE);
}



void *commandHandler() {
	char *input = (char *)malloc(512);
	char *inputCopy = (char *)malloc(512);
	int count;
	char *tokenized;
	char temp[512];
	write(STDOUT_FILENO, "> ", 2);
	count = read(STDIN_FILENO, input, 500);
	strcpy(inputCopy, input);
	while (1) {
		//count = read(STDIN_FILENO, input, 500);
		//input[strcspn(input, "\n")] = 0;
		tokenized = strtok(input, " \n");
		
		while (tokenized != NULL) {
			if (strcasecmp(tokenized, "list")==0) {
				tokenized = strtok(NULL, " \n");
				if (tokenized == NULL) {
					write(STDOUT_FILENO, "Invalid command.\n", 17);
				}
				else if (strcasecmp(tokenized, "all")==0) {
					tokenized = strtok(NULL, " \n");
					if (tokenized == NULL) {
						listProcesses("", "", 0);
					}
					else {
						write(STDOUT_FILENO, "Invalid command.\n", 17);
						tokenized = NULL;
					}
				}
				else if (strcasecmp(tokenized, "clients")==0) {
					tokenized = strtok(NULL, " \n");
					if (tokenized == NULL)
						listClients();
					else {
						write(STDOUT_FILENO, "Invalid command.\n", 17);
						tokenized = NULL;
					}
				}
				else {
					char *ip = tokenized;
					tokenized = strtok(NULL, " \n");
					if (tokenized != NULL) {
						char *port = tokenized;
						tokenized = strtok(NULL, " \n");
						if (tokenized == NULL)
							listProcesses(ip, port, 1);
						else {
							write(STDOUT_FILENO, "Invalid command.\n", 17);
						}
					}
					else {
						write(STDOUT_FILENO, "Invalid command.\n", 17);
					}
				}
			}
			
			else if (strcasecmp(tokenized, "kill")==0) {
				tokenized = strtok(NULL, " \n");
				if (tokenized == NULL) {
					write(STDOUT_FILENO, "Invalid command.\n", 17);
				}
				else {
					char *part1 = tokenized;
					tokenized = strtok(NULL, " \n");
					if (tokenized == NULL) {
						write(STDOUT_FILENO, "Invalid command.\n", 17);
					}
					else {
						char *part2 = tokenized;
						tokenized = strtok(NULL, " \n");
						if (tokenized == NULL)
							killClient(part1, part2, 0);
						else {
							write(STDOUT_FILENO, "Invalid command.\n", 17);
						}
					}
				}
			}

			else if (strcasecmp(tokenized, "broadcast")==0) {
				char temp[500];
				broadcast(inputCopy, count);
				tokenized = NULL;
			}
			
			else if (strcasecmp(tokenized, "help")==0) {
				tokenized = strtok(NULL, " \n");
				if (tokenized == NULL) {
					count = sprintf(temp, "\n%-25s Sends a message to all clients.\n%-25s List of valid commands.\n%-25s Display a list of connected clients.\n%-25s Display a list of active processes of all clients.\n%-25s Display a list of active processes of specified client.\n%-25s Kills all active processes and disconnects specified client.\n\n", "BROADCAST <MSG>", "HELP", "LIST CLIENTS", "LIST ALL", "LIST <IP> <PORT>", "KILL <IP> <PORT>");
					write(STDOUT_FILENO, temp, count);
				}
				else {
					write(STDOUT_FILENO, "Invalid command.\n", 17);
				}
				tokenized = NULL;
			}
			
			else {
				write(STDOUT_FILENO, "Invalid command.\n", 17);
				tokenized = NULL;
			}
		}
		input = (char *)malloc(512);
		write(STDOUT_FILENO, "> ", 2);
		count = read(STDIN_FILENO, input, 500);
		strcpy(inputCopy, input);
	}
}



void listClients() {
	int i;
	char buff[1024];
	int count = 0;
	count = sprintf(buff, "\n%-20s %-8s\n------------------------------\n", "CLIENT IP", "PORT #");
	for (i=0; i<cNum; i++) {
		if (strcasecmp(cList[i].status, "active")==0) {
			count += sprintf(buff+count, "%-20s %d\n", cList[i].IP, cList[i].port);
		}
	}
	count += sprintf(buff+count, "\n");
	write(STDOUT_FILENO, buff, count);
}



void listProcesses(char *ip, char *port, int check) {
	int i;
	int counter = 0;
	char buff[4096];
	int count = 0;
	for (i=0; i<cNum; i++) {
		if (check == 1) {
			if (strcasecmp(cList[i].IP, ip)==0 && atoi(port) == cList[i].port && strcasecmp(cList[i].status, "active")==0) {
				write(cList[i].pipefd2[WRITE_END], "list\n", 5);
				count = sprintf(buff, "\nCLIENT: %s/%s", ip, port);
				write(STDOUT_FILENO, buff, count);
				count = read(cList[i].pipefd[READ_END], buff, 4096);
				write(STDOUT_FILENO, buff, count);
				counter = 1;
				break;
			}
			else {
				write(STDOUT_FILENO, "No such active client found.\n", 29);
			}
		}
		else {
			if (strcasecmp(cList[i].status, "active") == 0) {
				count = sprintf(buff, "\nCLIENT: %s/%d", cList[i].IP, cList[i].port);
				write(cList[i].pipefd2[WRITE_END], "list\n", 5);
				int countCheck = read(cList[i].pipefd[READ_END], buff+count, 4096-count);
				if (countCheck > 0) {
					write(STDOUT_FILENO, buff, count+countCheck);
				}
				counter++;
			}
		}
	}
	if (cNum == 0 || counter == 0) {
		write(STDOUT_FILENO, "No active clients.\n", 18);
	}
	//write(STDOUT_FILENO, "\n", 1);
}



void killClient(char *ip, char *port, int all) {
	int i;
	int count;
	if (all == 0) {
		for (i=0; i<cNum; i++) {
			if (strcasecmp(cList[i].IP, ip)==0 && cList[i].port==atoi(port) && strcasecmp(cList[i].status, "active")==0) {
				count = write(cList[i].pipefd2[WRITE_END], "exit\n", 5);
				break;
			}
		}
	}
	else if (all == 1) {
		for (i=0; i<cNum; i++) {
			if (strcasecmp(cList[i].status, "active")==0) {
				count = write(cList[i].pipefd2[WRITE_END], "exit\n", 5);
			}
		}
	}
}



void killProcessWithID(int pid) {
	int i;
	for (i=0; i<cNum; i++) {
		if (cList[i].workerProcess) {
			strcpy(cList[i].status, "inactive");
			close(cList[i].pipefd[READ_END]);
			close(cList[i].pipefd2[WRITE_END]);
			break;
		}
	}	
}



void broadcast(char *msg, int length) {
	int i;
	for (i=0; i<cNum; i++) {
		if (strcasecmp(cList[i].status, "active")==0) {
			write(cList[i].pipefd2[WRITE_END], msg, length);	
		}
	}
}
