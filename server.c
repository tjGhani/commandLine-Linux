#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <pthread.h>



typedef struct{
	char *name;
	pid_t pid;
	pid_t parent;
	char *status;
	
	time_t start;
	time_t end;
	time_t tempStart;
	time_t tempEnd;
	time_t elapsedTime;
	struct tm timeStart;
	struct tm timeEnd;
}PROCESS;



PROCESS *pList;
int size;
pid_t client;
int msgsock;
int pipeRead;
int pipeWrite;



void parser(int op, char *str);
int create(char *str, pid_t parent);
void list(int writeFd);
void listAll();
void listDetails();
void killAll();
int killProcessWithID(pid_t killPID, int internal);
int killProcessWithName(char *pname, int all);
void *socketComm();



static void signal_handler(int signo) {
	if (signo == SIGCHLD) {
		int status;
		pid_t tempPID = waitpid(-1, &status, WNOHANG);
		if (tempPID != 0)
			killProcessWithID(tempPID, 0);
	}
	else if (signo == SIGINT) {
		killAll();
		write(msgsock, "Server down. Disconnected.\n", 27);
		close(pipeRead);
		close(pipeWrite);
		close(msgsock);
		exit(1);
	}
}



int main(int argc, char *argv[]) {
	pList = (PROCESS *)malloc(100*sizeof(PROCESS));
	size = 0;
	client = getppid();
	
	if (signal(SIGCHLD, signal_handler) == SIG_ERR) {
		write(STDOUT_FILENO, "Cannot handle SIGCHLD!\n", 23);
	}
	if (signal(SIGINT, signal_handler) == SIG_ERR) {
		write(STDOUT_FILENO, "Cannot handle SIGINT!\n", 22);
	}
	
	char buff[500];
	char *input = (char *)malloc(500);
	char *tokenized;
	int errCheck;
	int count;
	msgsock = atoi(argv[1]);
	pipeRead = atoi(argv[2]);
	pipeWrite = atoi(argv[3]);
	
	pthread_t tSocket;
	int ret = pthread_create(&tSocket, NULL, socketComm, NULL);
	if (ret) {
		write(STDOUT_FILENO, "Thread creation error.\n", 23);
	}
	
	char *inputS;
	
	//RUNNING SERVER LOCALLY
	char temp[500];
	
	inputS = (char *)malloc(500);
	count = read(msgsock, inputS, 500);
	//write(STDOUT_FILENO, inputS, count);
	strcpy(input, inputS);
	
	while (count > -1) {
		tokenized = strtok(inputS, " \n");
		
		while (tokenized != NULL) {
			
			//add
			if (strcasecmp(tokenized, "add") == 0) {
				parser(1, input);
			}
			
			//subtract
			else if (strcasecmp(tokenized, "sub") == 0) {
				parser(2, input);
			}
			
			//multiply
			else if (strcasecmp(tokenized, "mul") == 0) {
				parser(3, input);
			}
			
			//divide
			else if (strcasecmp(tokenized, "div") == 0) {
				parser(4, input);
			}
			
			//create process
			else if (strcasecmp(tokenized, "run") == 0) {
				tokenized = strtok(NULL, " \n");
				while (tokenized != NULL) {
					create(tokenized, getppid());
					tokenized = strtok(NULL, " \n");
				}
			}
			
			//list commands
			else if (strcasecmp(tokenized, "list") == 0) {
				tokenized = strtok(NULL, " \n");
				
				char *temppp = (char *)malloc(10);
				
				if (tokenized == NULL) {
					list(msgsock);
				}
				else if (strcasecmp(tokenized, "all") == 0) {
					tokenized = strtok(NULL, " \n");
					if (tokenized != NULL) {
						write(msgsock, "Please enter a valid command.\n", 29);
					}
					else {
						listAll();
					}
				}
				else if (strcasecmp(tokenized, "details") == 0) {
					tokenized = strtok(NULL, " \n");
					if (tokenized != NULL) {
						write(msgsock, "Please enter a valid command.\n", 30);
					}
					else {
						listDetails();
					}
				}
				else {
					write(msgsock, "Please enter a valid command.\n", 30);
				}
				
			}
			
			//kill commands
			else if (strcasecmp(tokenized, "kill") == 0) {
				tokenized = strtok(NULL, " \n");
				
				if (tokenized == NULL) {
					write(msgsock, "Please enter a complete command.\n", 33);
				}
				
				else if (strcasecmp(tokenized, "all") == 0) {
					tokenized = strtok(NULL, " \n");
					if (tokenized != NULL) {
						write(msgsock, "Please enter a valid command.\n", 29);
					}
					else {
						killAll();
					}
				}					
				
				else {
					int j = atoi(tokenized);
					if (j != 0) {
						int hi = killProcessWithID(j, 1);
						if (hi == 0) {
							write(msgsock, "No such process was found.\n", 29);
						}
					}
					else {
						char *pname = tokenized;
						tokenized = strtok(NULL, " \n");
						if (tokenized == NULL) {
							int success = killProcessWithName(pname, 0);
							if (success == 0) {
								write(msgsock, "No such active process was found.\n", 34);
							}
						}
						else if (strcasecmp(tokenized, "all") == 0) {
							int success = killProcessWithName(pname, 1);
							if (success == 0) {
								write(msgsock, "No such active processes were found.\n", 37);
							}
						}
						else {
							write(msgsock, "Please enter valid command.\n", 28);
						}
					}
				}
			}
			
			//exit
			else if (strcasecmp(tokenized, "exit") == 0) {
				tokenized = strtok(NULL, " \n");
				if (tokenized != NULL) {
					write(msgsock, "Invalid command.\n", 17);
				}
				else {
					killAll();
					close(pipeRead);
					close(pipeWrite);
					close(msgsock);
					exit(1);
				}
			}
			
			//none of the above
			else {
				write(msgsock, "Invalid command.\n", 17);
				break;
			}
			
			tokenized = strtok(NULL, " ");
		}
		
		inputS = (char *)malloc(500);
		count = read(msgsock, inputS, 500);
		//write(STDOUT_FILENO, inputS, count);
		strcpy(input, inputS);
	}

}



void parser(int op, char *str) {
	char *tokenized;
	char temp[50];
	int count = sprintf(temp, "%s\n", str);
	//write(STDOUT_FILENO, temp, count);
	tokenized = strtok(str, " \n");
	
	if (op == 1) {
		tokenized = strtok(NULL, ", ");
		int sum = 0;
		while (tokenized != NULL) {
			sum += atoi(tokenized);
			tokenized = strtok(NULL, ", ");
		}
		count = sprintf(temp, "%d\n", sum);
		write(msgsock, temp, count);
	}
	
	else if (op == 2) {
		tokenized = strtok(NULL, ", ");
		int sum = atoi(tokenized);
		tokenized = strtok(NULL, ", ");
		while (tokenized != NULL) {
			sum -= atoi(tokenized);
			tokenized = strtok(NULL, ", ");
		}
		count = sprintf(temp, "%d\n", sum);
		write(msgsock, temp, count);	
	}
	
	else if (op == 3) {
		tokenized = strtok(NULL, ", ");
		if (tokenized == NULL) {
			write(msgsock, "Please enter complete command.\n", 31);
		}
		else {
			int ans = atoi(tokenized);
			tokenized = strtok(NULL, ", ");
			while (tokenized != NULL) {
				ans *= atoi(tokenized);
				tokenized = strtok(NULL, ", ");
			}
			count = sprintf(temp, "%d\n", ans);
			write(msgsock, temp, count);
		}	
	}
	
	else if (op == 4) {
		tokenized = strtok(NULL, ", ");
		if (tokenized == NULL) {
			write(msgsock, "Please enter complete command.\n", 29);
		}
		else {
			double ans = atoi(tokenized);
			int invalid = 0;
			tokenized = strtok(NULL, ", ");
			while (tokenized != NULL) {
				if (atoi(tokenized) == 0) {
					invalid = 1;
					break;
				}
				ans /= atoi(tokenized);
				tokenized = strtok(NULL, ", ");
			}
			if (!invalid) {
				count = sprintf(temp, "%f\n", ans);
			}
			else {
				count = sprintf(temp, "Invalid values.\n");
			}
			write(msgsock, temp, count);
		}
	}
}



int create(char *str, pid_t parent) {
	char *name = (char *)malloc(100);
	int count = sprintf(name, "/usr/bin/%s", str);
	//write(STDOUT_FILENO, name, count);
	char *temp = (char *)malloc(100);
	int pipefd[2];
	
	int pipeCheck = pipe2(pipefd, O_CLOEXEC);
	
	pid_t pidR = fork();
	
	if (pidR > 0) {
		close(pipefd[1]);
		char buff[50];
		int pipeRead = read(pipefd[0], buff, 50);
		if (pipeRead > 0) {
			write(msgsock, "Failed to run process.\n", 23);
			return 0;
		}
		
		else {
			time(&pList[size].start);
			pList[size].timeStart = *localtime(&pList[size].start);
			pList[size].start = (time_t)localtime(&pList[size].start);
			time(&pList[size].tempStart);
			pList[size].name = (char *)malloc(100);
			strcpy(pList[size].name, str);
			pList[size].status = (char *)malloc(10);
			strcpy(pList[size].status, "active");
			pList[size].pid = pidR;
			pList[size].parent = parent;
			count = sprintf(temp, "pid: %d, name: %s, status: %s, parent: %d\n", pidR, pList[size].name, pList[size].status, parent);
			write(msgsock, temp, count);
			size++;
			return 1;
		}
	}
	
	else if (pidR == 0) {
		close(pipefd[0]);
		int errCheck = execl(name, str, NULL);
		if (errCheck == -1) {
			write(pipefd[1], "Exec failed.\0", 13);
		}
		exit(-1);
	}
	
	else {
		perror("fork");
		exit(-1);
	}
}



void list(int writeFd) {
	int i;
	int check = 0;
	char buffer[4096];
	int count = sprintf(buffer, "\n%-8s %-20s\n-----------------------------\n", "PID", "NAME");
	//write(writeFd, buffer, count);
	for (i=0; i<size; i++) {
		if (strcasecmp(pList[i].status, "active") == 0) {
			count += sprintf(buffer+count, "%-8d %-20s\n", pList[i].pid, pList[i].name);
			check = 1;
		}
	}
	count += sprintf(buffer+count, "\n");
	if (check == 0) {
		count += sprintf(buffer+count, "No active processes.\n\n");
	}
	write(writeFd, buffer, count);
}



void listAll() {
	int i;
	char buffer[500];
	int count = sprintf(buffer, "\n%-8s %-20s %-10s\n-------------------------------------------------\n", "PID", "NAME", "STATUS");
	write(msgsock, buffer, count);
	
	for (i=0; i<size; i++) {
		count = sprintf(buffer, "%-8d %-20s %-10s\n", pList[i].pid, pList[i].name, pList[i].status);
		write(msgsock, buffer, count);
	}
	
	write(msgsock, "\n", 1);
}



void listDetails() {
	int i;
    char buffer[1000];
    char hello[10];
    char lol[10];
    struct tm incase;
    time_t prereq;
    int count = sprintf(buffer, "\n%-8s %-20s %-10s %-12s %-12s %-12s\n--------------------------------------------------------------------------------\n", "PID", "NAME", "STATUS", "START", "END", "RUNNING TIME");
	write(msgsock, buffer, count);
	
	for (i=0; i<size; i++) {
		int check = 0;
        strftime(hello, 10, "%H:%M:%S", &pList[i].timeStart);
        strftime(lol, 10, "%H:%M:%S", &pList[i].timeEnd);
        if (strcasecmp(pList[i].status, "active") == 0) {
            time(&prereq);
            incase = *localtime(&prereq);
            strftime(lol, 10, "%H:%M:%S", &incase);
            pList[i].elapsedTime = difftime(prereq, pList[i].tempStart);
            check = 1;
        }
        count = sprintf(buffer, "%-8d %-20s %-10s %-12s %-12s %-5ld\n", pList[i].pid, pList[i].name, pList[i].status, hello, lol, pList[i].elapsedTime);
        write(msgsock, buffer, count);
        if (check == 1)
            pList[i].elapsedTime = 0;
	}
	
	write(msgsock, "\n", 1);
}



int killProcessWithID(pid_t killPID, int internal) {
	int i;
	int check = 0;
	for (i=0; i<size; i++) {
		if (pList[i].pid == killPID && strcasecmp(pList[i].status, "active") == 0 && pList[i].parent == client) {
			check = 1;
			if (internal == 1) {
				kill(killPID, SIGTERM);
			}
			time(&pList[i].end);
			pList[i].timeEnd = *localtime(&pList[i].end);
			pList[i].end = (time_t)localtime(&pList[i].end);
			time(&pList[i].tempEnd);
			pList[i].elapsedTime = difftime(pList[i].tempEnd, pList[i].tempStart);
			strcpy(pList[i].status, "inactive");
		}
	}
	return check;
}



void killAll() {
	int i;
	for (i=0; i<size; i++) {
		if (strcasecmp(pList[i].status, "active") == 0 && pList[i].parent == client) {
			kill(pList[i].pid, SIGTERM);
			time(&pList[i].end);
			pList[i].timeEnd = *localtime(&pList[i].end);
			pList[i].end = (time_t)localtime(&pList[i].end);
			time(&pList[i].tempEnd);
			pList[i].elapsedTime = difftime(pList[i].tempEnd, pList[i].tempStart);
			strcpy(pList[i].status, "inactive");
		}
	}
}



int killProcessWithName(char *pname, int all) {
	int i;
	int check = 0;
	for (i=0; i<size; i++) {
		if (strcasecmp(pname, pList[i].name) == 0 && strcasecmp(pList[i].status, "active") == 0 && pList[i].parent == client) {
			check = 1;
			kill(pList[i].pid, SIGTERM);
			time(&pList[i].end);
			pList[i].timeEnd = *localtime(&pList[i].end);
			pList[i].end = (time_t)localtime(&pList[i].end);
			time(&pList[i].tempEnd);
			pList[i].elapsedTime = difftime(pList[i].tempEnd, pList[i].tempStart);
			strcpy(pList[i].status, "inactive");
			if (all == 0)
				break;
		}
	}
	return check;
}



void *socketComm() {
	char temp[500];
	char msg[500];
	int count = read(pipeRead, temp, 500);
	strcpy(msg, temp);
	msg[count-1] = '\0';
	
	char *tokenize;
	while (count > 0) {
		tokenize = strtok(temp, " \n");
		if (strcasecmp(tokenize, "list")==0) {
			list(pipeWrite);
		}
		else if (strcasecmp(tokenize, "exit")==0) {
			killAll();
			write(msgsock, "Server down. Disconnected.\n", 27);
			close(pipeRead);
			close(pipeWrite);
			close(msgsock);
			exit(1);
		}
		else if (strcasecmp(tokenize, "broadcast")==0) {
			char send[100];
			count = sprintf(send, "Server says: %s\n", msg+10);
			write(msgsock, send, count);
		}		
	
		count = read(pipeRead, temp, 500);
		strcpy(msg, temp);
		msg[count-1] = '\0';
	}
}
