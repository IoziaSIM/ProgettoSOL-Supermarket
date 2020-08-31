#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/socket.h> 
#include <sys/types.h>
#include <sys/un.h>
#include "./config.h"

#define UNIX_PATH_MAX 108   
#define SOCKNAME "./mysock"

static int fd_skt = -1;
static int fd_c;
static int superm_pid;
static int dir_pid;
static config* txt;
   
volatile sig_atomic_t sigquit = 0;
volatile sig_atomic_t supermercato = 0;

static void handler (int signum)
{

	if (signum == SIGHUP) 
	    	kill(superm_pid, SIGHUP);

	if (signum == SIGQUIT) {
		sigquit = 1;
		kill(superm_pid, SIGQUIT);
	}

	if(signum == SIGUSR1)
		kill(superm_pid, SIGUSR1);   //cliente può uscire
	
	if(signum == SIGUSR2)
		supermercato = 1;    //supermercato chiude
}

void RunSocket ()
{
	char buf[6];
	struct sockaddr_un sock;
	
	strncpy(sock.sun_path, SOCKNAME, UNIX_PATH_MAX);
	sock.sun_family=AF_UNIX;
	
	if ((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {		
		fprintf(stderr,"Server: Impossibile creare la socket");
        	exit(EXIT_FAILURE);
    	}  

   	if((bind(fd_skt, (struct sockaddr *)&sock, sizeof(sock))) == -1) {
    		fprintf(stderr,"Server: bind error");
        	exit(EXIT_FAILURE);
    	}

    	if((listen(fd_skt, SOMAXCONN)) == -1) {
    		fprintf(stderr,"Server: listen error");
        	exit(EXIT_FAILURE);
    	}

    	if((fd_c = accept(fd_skt,NULL,0)) == -1) {
    		fprintf(stderr,"Server: accept error");
        	exit(EXIT_FAILURE);
    	}	

    	/* SCAMBIO PID */
    	if((read(fd_c, (char*) buf, sizeof(buf))) == -1) {
    		fprintf(stderr,"Server: read error");
    		exit(EXIT_FAILURE);
    	}

    	superm_pid = atoi(buf);
    	sprintf(buf, "%d", dir_pid);
	
	
	if((write (fd_c, (char*) buf, sizeof(buf))) == -1) {
		fprintf(stderr,"Server: write error");
        	exit(EXIT_FAILURE);
	}

}
 
void *man_casse(void* arg) 
{
	
	int data[txt->K];
	char buf[7];
	int chiudi;
	
	while(sigquit == 0 && supermercato == 0) {
	
		chiudi = 0;

		if(read(fd_c, (int*) data, sizeof(data)) == -1)  {
			fprintf(stderr, "man_casse: Read fallita!\n");
			exit(EXIT_FAILURE);
		} 
		
		
		if(sigquit == 0 && supermercato == 0) { 

			for(int i=0; i < txt->K; i++) {

				if (data[i] == 0 || data[i] == 1) {
					chiudi ++;
					if (chiudi >= txt->S1) {
						strcpy(buf, "Chiudi");
						kill(superm_pid, SIGUSR2);
						if((write (fd_c, buf, sizeof(buf))) == -1) {
							fprintf(stderr,"man_casse: Write chiusura fallita!");
        						exit(EXIT_FAILURE);
						}
						break;
					}
				}
			
				else if (data[i] >= txt->S2) {
					strcpy(buf, "Apri");
					kill(superm_pid, SIGUSR2);
					if((write (fd_c, buf, sizeof(buf))) == -1) {
						fprintf(stderr,"man_casse: Write apertura fallita!");
        					exit(EXIT_FAILURE);
					}
					break;
				}

			}
		}

	}
	
	pthread_exit(NULL);
}

int main (int argc, const char* argv[]) 
{
	pthread_t managercasse;
	struct sigaction sa;
	pid_t pid;

	/* INIZIALIZZAZIONE */
	if (argc!=2) {
        	fprintf(stderr,"Usage: %s /config1.txt oppure /config2.txt \n", argv[0]);
        	exit(EXIT_FAILURE);
    	}

    
    	if (strcmp(argv[1],"./config1.txt") == 0  || strcmp(argv[1],"./config2.txt") == 0) {
        	if ((txt = test(argv[1])) == NULL)
            		exit(EXIT_FAILURE);
    	}
    
    	else {
        	fprintf(stderr,"%s: You need to insert the config file as argoument \n",argv[0]);
        	exit(EXIT_FAILURE);
    	}

	switch(pid = fork())  {
		case -1: /* padre errore */
			{
				fprintf(stderr,"Fork fallita!\n");
        			exit(EXIT_FAILURE);
			}
		
		case 0: /* figlio */
			{ 
				/* AVVIA PROCESSO SUPERMERCATO */
				execl ("./supermercato.o", "supermercato.o", argv[1], NULL);
				fprintf(stderr,"Esecuzione fallita\n");
				exit(EXIT_FAILURE); 
			}
		
		default: /* padre */
			{
				dir_pid = getpid();
				
				/* GESTIONE SEGNALI */
				memset(&sa, 0, sizeof(sa));
				sa.sa_handler = handler;
				if(sigaction(SIGQUIT, &sa, NULL))
					fprintf(stderr,"Handler error");
				if(sigaction(SIGHUP, &sa, NULL))
					fprintf(stderr,"Handler error");
				if(sigaction(SIGUSR1, &sa, NULL))
					fprintf(stderr,"Handler error");
	
				RunSocket();

				/* CREAZIONE THREAD CASSE */
				if((pthread_create(&managercasse, NULL, man_casse, NULL)) != 0) {		
					fprintf(stderr,"Creazione thread manager casse fallita!");
   			     		exit(EXIT_FAILURE);
   				}
   			
   				if ((pthread_join(managercasse,NULL) != 0))
       				fprintf(stderr,"Join thread manager casse fallita!");
       			
       			
				int status;
				
       			if((waitpid(pid, &status, 0) == -1)) {
       				fprintf(stderr,"Creazione thread manager casse fallita!");
   			     		exit(EXIT_FAILURE);
       			}
       			
       			printf("motivo1 %d\n", WIFEXITED(status)); 
       			printf("motivo %d\n", WTERMSIG(status)); 

			}
    	}
    
    	close(fd_skt);
	close(fd_c);
	return 0;
}
