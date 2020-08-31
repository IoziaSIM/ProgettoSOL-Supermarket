#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <sys/socket.h> 
#include <sys/types.h>
#include <sys/un.h>
#include "./coda.h"
#include "./dati_cassa.h"
#include "./config.h"


#define MAX_LEN 256
#define UNIX_PATH_MAX 108   
#define SOCKNAME "./mysock"


static pthread_mutex_t* mtxcassa;     // MUTEX +
static checkout** cassa;     //ARRAY DI PUNTATORI PER LE CASSE

static pthread_mutex_t* mtxexit;   // MUTEX 
static int* exit_cassa;   // ARRAY PER CAPIRE SE LE CASSE DEVONO CHIUDERE O NO 

static pthread_mutex_t* mtxlen;  
static int* len_code; 

static pthread_cond_t* checkout_cond;   /* ARRAY DI CONDITION PER SVEGLIARE LA CASSA CON CODA VUOTA */
static pthread_cond_t* client_cond; /* ARRAY DI CONDITION PER SVEGLIARE I CLIENTI SERVITI O CACCIATI */
static pthread_cond_t* alg_cond;

static pthread_mutex_t* mtxalgoritmo;
static pthread_mutex_t* mtxcliente;
static pthread_mutex_t* mtxcambio;  

static int* algoritmo;  // ARRAY PER CAPIRE SE E' SCATTATO L'ALGORITMO
static int* cambio_cassa;   //ARRAY PER FAR CAPIRE ALL'ALGORITMO CHE HO CAMBIATO CASSA

static time_t sec;

static checkout* em_exit;

static pthread_mutex_t mtxattivo = PTHREAD_MUTEX_INITIALIZER;
static int cl_attivi;      //CONTEGGIA QUANTI CLIENTI SONO ATTIVI

static long fd_skt = -1;

static pid_t superm_pid;
static pid_t dir_pid;

static pthread_mutex_t mtxfile = PTHREAD_MUTEX_INITIALIZER;
static FILE* final;

static config* txt;

// SEGNALI
volatile sig_atomic_t sighup = 0;    
volatile sig_atomic_t sigquit = 0;
volatile sig_atomic_t sigcliente = 0;
volatile sig_atomic_t sigcasse = 0;

void allocate_all()
{
	/* CREAZIONE ARRAY DI PUNTATORI */
	if ((cassa = malloc(txt->K * sizeof(queue*)))==NULL) {
   		fprintf(stderr, "Allocazione code casse fallita!\n");
   		exit(EXIT_FAILURE);  
  	}
    
  	for (int i=0;i<txt->K; i++) 
  		cassa[i]=checkout_init();
   

  /* CREAZIONE MUTEX CODE E INIZIALIZZAZIONE */
  	if((mtxcassa=malloc(txt->K * sizeof(pthread_mutex_t))) == NULL){
    		fprintf(stderr, "Allocazione mutex casse fallita!\n");
    	    	exit(EXIT_FAILURE);  
   	}
    
  	for (int i=0;i<txt->K;i++){
   		if (pthread_mutex_init(&mtxcassa[i], NULL) != 0) {
      			fprintf(stderr, "Inizializzazione mutex casse fallita!\n");
      			exit(EXIT_FAILURE);                   
  		}
  	}
  
      	
  	/* CREAZIONE ARRAY DI USCITA */
  	if ((exit_cassa = malloc(txt->K * sizeof(int))) == NULL) {
   		fprintf(stderr, "Allocazione array uscita fallita!\n");
   		exit(EXIT_FAILURE);  
  	}	
  
  	for (int i=0;i<txt->K;i++) 
  		exit_cassa[i]=0;
    	
    	
  	/* CREAZIONE MUTEX */
  	if((mtxexit = malloc(txt->K * sizeof(pthread_mutex_t))) == NULL){
   		fprintf(stderr, "Allocazione mutex array uscita fallita!\n");
   		exit(EXIT_FAILURE);  
  	}
    
  	for (int i=0;i<txt->K;i++){
  	 	if (pthread_mutex_init(&mtxexit[i], NULL) != 0) {
     			fprintf(stderr, "Inizializzazione mutex array delle uscite fallita!\n");
     			exit(EXIT_FAILURE);                   
  		}
  	}
  	
  	/* CREAZIONE ARRAY DI CASSE */
	if ((len_code = malloc(txt->K * sizeof(int)))==NULL) {
   		fprintf(stderr, "Allocazione lunghezza code fallita!\n");
   		exit(EXIT_FAILURE);  
  	}
    
  	for (int i=0;i<txt->K; i++) 
  		len_code[i]=-1;
   

  	/* CREAZIONE MUTEX CASSE */
  	if((mtxlen=malloc(txt->K * sizeof(pthread_mutex_t))) == NULL){
    		fprintf(stderr, "Allocazione mutex lunghezza code fallita!\n");
    	    	exit(EXIT_FAILURE);  
   	}
    
  	for (int i=0;i<txt->K;i++){
   		if (pthread_mutex_init(&mtxlen[i], NULL) != 0) {
      			fprintf(stderr, "Inizializzazione mutex lunghezza code fallita!\n");
      			exit(EXIT_FAILURE);                   
  		}
  	}
    	
    	
    	/* INIZIALIZZAZIONE CONDIZIONE DELLE CASSE */
  	if((checkout_cond=malloc(txt->K * sizeof(pthread_cond_t))) == NULL) {
   		fprintf(stderr, "Allocazione condition variable delle casse fallita!\n");
   		exit(EXIT_FAILURE); 
  	}
    
  	for (int i=0;i<txt->K;i++){
   		if (pthread_cond_init(&checkout_cond[i], NULL) != 0) {
   			fprintf(stderr, "Inizializzazione condition variable delle casse fallita!\n");
   			exit(EXIT_FAILURE);                   
    		}
  	} 
  
 
   	
  	/* INIZIALIZZAZIONE CONDIZIONE DEL CLIENTE */
  	if((client_cond=malloc(txt->K * sizeof(pthread_cond_t))) == NULL){
   		fprintf(stderr, "Allocazione condition variable del cliente fallita!\n");
   		exit(EXIT_FAILURE); 
  	}
    
  	for (int i=0;i<txt->K;i++){
   		if (pthread_cond_init(&client_cond[i], NULL) != 0) {
   			fprintf(stderr, "Inizializzazione condition variable del cliente fallita!\n");
   			exit(EXIT_FAILURE);                   
   		}
  	}
  	
  	em_exit=checkout_init();
  

  	if((alg_cond=malloc(txt->C * sizeof(pthread_cond_t))) == NULL){
   		fprintf(stderr, "Allocazione condition variable del cliente fallita!\n");
   		exit(EXIT_FAILURE); 
  	}
    
  	for (int i=0;i<txt->C;i++){
   		if (pthread_cond_init(&alg_cond[i], NULL) != 0) {
   			fprintf(stderr, "Inizializzazione condition variable algoritmo fallita!\n");
   			exit(EXIT_FAILURE);                   
   		}
  	}
  
    		
    	/* CREAZIONE ARRAY ALGORITMO */
  	if ((algoritmo = malloc(txt->C * sizeof(int))) == NULL) {
  		fprintf(stderr, "Allocazione array algoritmo fallita!\n");
  		exit(EXIT_FAILURE);  
  	}
   	
  	for (int i=0;i<txt->C;i++) 
    		algoritmo[i]=0;
   
      	/* CREAZIONE MUTEX */
  	if((mtxalgoritmo = malloc(txt->C * sizeof(pthread_mutex_t))) == NULL ){
   		fprintf(stderr, "Allocazione mutex algoritmo fallita!\n");
   		exit(EXIT_FAILURE);  
  	}
    
  	for (int i=0;i<txt->C;i++){
  	 	if (pthread_mutex_init(&mtxalgoritmo[i], NULL) != 0) {
  	 	fprintf(stderr, "Inizializzazione mutex array algoritmo fallita!\n");
  	 	exit(EXIT_FAILURE);                   
  	 	}
  	}
     
      	
  	/* CREAZIONE ARRAY CAMBIO CASSA */
  	if ((cambio_cassa = malloc(txt->C * sizeof(int))) == NULL) {
   		fprintf(stderr, "Allocazione array cambiocassa fallita!\n");
   		exit(EXIT_FAILURE);  
  	}	
    	
  	for (int i=0;i<txt->C;i++) 
    		cambio_cassa[i]=0;
    		
    	/* CREAZIONE MUTEX */
  	if((mtxcambio = malloc(txt->C * sizeof(pthread_mutex_t))) == NULL ){
   		fprintf(stderr, "Allocazione mutex algoritmo fallita!\n");
   		exit(EXIT_FAILURE);  
  	}
    
  	for (int i=0;i<txt->C;i++){
  	 	if (pthread_mutex_init(&mtxcambio[i], NULL) != 0) {
  	 	fprintf(stderr, "Inizializzazione mutex array algoritmo fallita!\n");
  	 	exit(EXIT_FAILURE);                   
  	 	}
  	}
    		
    		
    	/* CREAZIONE MUTEX CODE E INIZIALIZZAZIONE */
  	if((mtxcliente = malloc(txt->C * sizeof(pthread_mutex_t))) == NULL){
    		fprintf(stderr, "Allocazione mutex casse fallita!\n");
    	    	exit(EXIT_FAILURE);  
   	}
    
  	for (int i=0;i<txt->K;i++){
   		if (pthread_mutex_init(&mtxcliente[i], NULL) != 0) {
      			fprintf(stderr, "Inizializzazione mutex casse fallita!\n");
      			exit(EXIT_FAILURE);                   
  		}
  	}
  
      
}


void reallocate_array(int index, int size)
{
  
  	/* CREAZIONE ARRAY ALGORITMO */
  	if ((algoritmo = realloc(algoritmo, size * sizeof(int))) == NULL) {
    		fprintf(stderr, "Reallocazione array algoritmo fallita!\n");
    		exit(EXIT_FAILURE);  
  	}
    
  	for (int i=index;i<size;i++) 
    		algoritmo[i]=0;
    
  /* CREAZIONE MUTEX */
  	if((mtxalgoritmo = realloc(mtxalgoritmo, size * sizeof(pthread_mutex_t))) == NULL ){
    		fprintf(stderr, "Reallocazione mutex algoritmo fallita!\n");
    		exit(EXIT_FAILURE);  
  	}
    
  	for (int i=index;i<size;i++){
    		if (pthread_mutex_init(&mtxalgoritmo[i], NULL) != 0) {
   			fprintf(stderr, "Inizializzazione mutex array algoritmo fallita!\n");
    			exit(EXIT_FAILURE);                   
    		}
 	}
        
  	/* CREAZIONE ARRAY CAMBIO CASSA */
  	if ((cambio_cassa = realloc(cambio_cassa, size * sizeof(int))) == NULL) {
    		fprintf(stderr, "Reallocazione array cambiocassa fallita!\n");
    		exit(EXIT_FAILURE);  
  	}
      
  	for (int i=index;i<size;i++) 
        	cambio_cassa[i]=0;
        	
        /* CREAZIONE MUTEX */
  	if((mtxcambio = realloc(mtxcambio, size * sizeof(pthread_mutex_t))) == NULL ){
   		fprintf(stderr, "Reallocazione mutex di cambio cassa fallita!\n");
   		exit(EXIT_FAILURE);  
  	}
    
  	for (int i=0;i<size;i++){
  	 	if (pthread_mutex_init(&mtxcambio[i], NULL) != 0) {
  	 	fprintf(stderr, "Inizializzazione mutex di cambio cassa fallita!\n");
  	 	exit(EXIT_FAILURE);                   
  	 	}
  	}
        	
        	
        if((alg_cond=realloc(alg_cond, size * sizeof(pthread_cond_t))) == NULL){
   		fprintf(stderr, "Reallocazione condition variable algoritmo fallita!\n");
   		exit(EXIT_FAILURE); 
  	}
    
  	for (int i=index;i<size;i++){
   		if (pthread_cond_init(&alg_cond[i], NULL) != 0) {
   			fprintf(stderr, "Inizializzazione condition variable algoritmo fallita!\n");
   			exit(EXIT_FAILURE);                   
   		}
  	}
  	
  	if((mtxcliente = realloc(mtxcliente, size * sizeof(pthread_mutex_t))) == NULL ){
    		fprintf(stderr, "Reallocazione mutex cliente fallita!\n");
    		exit(EXIT_FAILURE);  
  	}
    
  	for (int i=index;i<size;i++){
    		if (pthread_mutex_init(&mtxcliente[i], NULL) != 0) {
   			fprintf(stderr, "Inizializzazione mutex cliente fallita!\n");
    			exit(EXIT_FAILURE);                   
    		}
 	}
  
}


void deallocate_all()
{
  	for(int i=0;i < txt->K;i++)
    		free(cassa[i]);
  	free(cassa);
  	free(mtxcassa);
  	free(exit_cassa);
  	free(mtxexit);
  	free(len_code);
  	free(mtxlen);
 	free(checkout_cond);
  	free(client_cond);
  	free(alg_cond);
  	free(mtxcliente);
  	free(algoritmo);
  	free(mtxalgoritmo);
  	free(cambio_cassa);
  	free(em_exit);
}


static void handler (int signum)
{

	if (signum == SIGHUP) sighup = 1;
	if (signum == SIGQUIT) sigquit = 1;
	if (signum == SIGUSR1) sigcliente = 1;
	if (signum == SIGUSR2) sigcasse = 1;
}


static void ConnectSocket ()
{	
	char buf[6];
	struct sockaddr_un sock;
	
	sock.sun_family=AF_UNIX;
	strncpy(sock.sun_path, SOCKNAME, UNIX_PATH_MAX); 

        if ((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {		/* CREAZIONE SOCKET */
		fprintf(stderr,"Client: Impossibile creare la socket");
        	exit(EXIT_FAILURE);
        }  
        
        while (connect(fd_skt, (const struct sockaddr *)&sock, sizeof(sock)) == -1 ) {  /* CONNESSIONE AL SERVER */
        	/* non esiste il file */
                if (errno == ENOENT) 
        		sleep(1);
        	else 
                       exit(EXIT_FAILURE); 
               
        }

        /* SCAMBIO PID */
        sprintf(buf, "%d", superm_pid);
        
        if((write(fd_skt, (char*) buf, sizeof(buf))) == -1) {
                fprintf(stderr,"Client: Impossibile creare la socket");
                exit(EXIT_FAILURE);
        }

        if((read(fd_skt, (char*) buf, sizeof(buf))) == -1) {
                fprintf(stderr,"Server: read error");
                exit(EXIT_FAILURE);
        }

	dir_pid = atoi(buf);

}


void *news_t (void* arg)
{	
	pthread_mutex_lock(&mtxattivo);
  	while(sigquit == 0|| !(sighup == 1 && cl_attivi == 0)) {
        	
        	pthread_mutex_unlock(&mtxattivo);
   		struct timespec t={(txt->News_t / 1000),((txt->News_t % 1000)*1000000)}; 
     		nanosleep(&t,NULL);
        	
        	pthread_mutex_lock(&mtxattivo);        	
        	if (sigquit == 0 || !(sighup == 1 && cl_attivi == 0)) {  
        		
        		/*INVIO INFORMAZIONE AL DIRETTORE */
        		for(int i=0; i < txt->K; i++)
 				pthread_mutex_lock(&mtxlen[i]);
 			if (write(fd_skt, (int*) len_code, sizeof(len_code)) == -1)
                		fprintf(stderr, "news_t: Write fallita!");
			for(int i=0; i < txt->K; i++)
 				pthread_mutex_lock(&mtxlen[i]);

        	}
 	}
	pthread_mutex_unlock(&mtxattivo);
	pthread_exit(NULL);
} 



int decision_algorithm (int n_cassa, customer_data* cl)
{
	int i, attual_t, j=n_cassa;
	
	pthread_mutex_lock(&mtxcassa[j]);
	attual_t = cassa[j]->t_stimato - cl->spesa; 
	for(i=0; i< txt->K; i++) {
		if(i == j) continue;
		pthread_mutex_lock(&mtxcassa[i]);
		if(cassa[i]->t_stimato < attual_t) {
			pthread_mutex_unlock(&mtxcassa[j]);
			j=i;
			attual_t = cassa[i]->t_stimato;
			continue;
		}
		pthread_mutex_lock(&mtxcassa[i]);
	}
	pthread_mutex_unlock(&mtxcassa[j]);
	return j;
}


void *algorithm_t (void* arg)
{
	customer_data* cl = ((customer_data*)arg);;
	int id = cl->id;
	
	pthread_mutex_lock(&mtxcliente[id]);
	while(cl->loading == 0 && sigquit == 0)  {
			
		pthread_mutex_unlock(&mtxcliente[id]);	
		/* ATTENDI */
		struct timespec ms= {(txt->S/1000), ((txt->S%1000)*1000000)};
		nanosleep(&ms, NULL);
		
		pthread_mutex_lock(&mtxcliente[id]);
		if(cl->loading == 1 || sigquit == 1) break;
		pthread_mutex_unlock(&mtxcliente[id]);
		
		pthread_mutex_lock(&mtxcambio[id]);
		if(cambio_cassa[id] == 0) {
			
			/*MANDA IL SEGNALE */	
			pthread_mutex_lock(&mtxalgoritmo[id]);
			algoritmo[id] = 1;
			pthread_cond_signal(&client_cond[id]);
			pthread_cond_wait(&alg_cond[id], &mtxalgoritmo[id]);
			pthread_mutex_unlock(&mtxalgoritmo[id]);
		}
		
		else 
			cambio_cassa[id] = 0;	
			
		pthread_mutex_unlock(&mtxcambio[id]);
		pthread_mutex_lock(&mtxcliente[id]);			
	}
	
	pthread_mutex_unlock(&mtxcliente[id]);
	pthread_exit(NULL);
}


void *cliente_t (void* arg)
{
	pthread_t algo;
	struct timespec store;
    	struct timespec store2;
    	struct timespec store3;
    	struct timespec store4;
    	long time1,time2,time3=-1,time4=-1;
    	int* alr_visited;
    	int randomtime, n_cassa, check = 0, id;
    	unsigned int seed;
  
	/* IMPOSTO DATI CLIENTE */
    	customer_data* cl  = malloc(sizeof(customer_data)); 
    	customer_setup(cl,(int) (intptr_t) (arg)); 
    	id = cl->id;
    	
    	seed = id + sec; 
	
	/* PRENDO IL TEMPO DI ENTRATA DEL CLIENTE */
    	clock_gettime(CLOCK_REALTIME, &store);
    	time1 = (store.tv_sec)*1000 + (store.tv_nsec) / 1000000;
	   	
    	/*TEMPO PER GLI ACQUISTI */
	while ((randomtime = rand_r(&seed) % (txt->T)) < 10); 
    	struct timespec ms = {(randomtime/1000),((randomtime%1000)*1000000)};
    	nanosleep(&ms,NULL);
    	
	/* NUMERO DI PRODOTTI */    	
	cl->n_acquisti = rand_r(&seed)%(txt->P); 
	cl->spesa = (txt->Prod_t) * cl->n_acquisti; 
    	
    	if ((cl->n_acquisti) != 0 && sigquit == 0) {    /* SE HA ACQUISTATO QUALCOSA */
    	
    		/* ALLOCAZIONE ARRAY DI CODE VISITATE */
		if((alr_visited = malloc(txt->K * sizeof(int))) == 0) {
			fprintf(stderr, "Allocazione code visitate fallito!");
			exit(EXIT_FAILURE);
		}
		
		for(int i = 0; i < txt->K; i++) 
			alr_visited[i] = 0;
    	
	    	/* SCELTA CASUALE CASSA */
	    	while(check == 0 && sigquit == 0) {     	
	       	n_cassa = rand_r(&seed) % (txt->K); 
	       	printf("cassa %d\n", n_cassa); fflush(stdout);
			pthread_mutex_lock(&mtxcassa[n_cassa]);
	       	if (cassa[n_cassa]->open != 0) {
	       		check = 1;
	       	}
	        	pthread_mutex_unlock(&mtxcassa[n_cassa]);
	    	}
	        
	    	if(sigquit == 0) {
	        	
	        	/* PRENDO IL TEMPO CHE STA IN CODA */
	        	clock_gettime(CLOCK_REALTIME, &store3); 
	        	time3 = (store3.tv_sec)*1000 + (store3.tv_nsec) / 1000000;
	        	
	        	/* TENGO TRACCIA DELLE CODE VISITATE */
	       	alr_visited[n_cassa] = 1;
	        	
	        	/* IL CLIENTE SI AGGIUNGE NELLA PRIMA CODA */
	        	pthread_mutex_lock(&mtxcassa[n_cassa]);
	        	pthread_mutex_lock(&mtxlen[n_cassa]);
			customer_push(&cassa[n_cassa],&cl);     
			len_code[n_cassa]++;
			pthread_mutex_unlock(&mtxlen[n_cassa]);
			cassa[n_cassa]->t_stimato += cassa[n_cassa]->randomtime + txt->Prod_t * cl->n_acquisti;
	              	pthread_mutex_unlock(&mtxcassa[n_cassa]);
	              	
	        	pthread_cond_signal(&checkout_cond[n_cassa]); /* SEGNALE PER SVEGLIARE CASSA */
	                
	        	/* CREAZIONE THREAD ALGORITMO */
	        	if (pthread_create(&algo, NULL, algorithm_t, (void*) (intptr_t) (id)) != 0) {
            			fprintf(stderr,"Creazione thread algoritmo fallito");
            			exit(EXIT_FAILURE);
	        	}
	        
	        	pthread_mutex_lock(&mtxcliente[id]);
	        	if(sigquit == 0)    
	        		pthread_cond_wait(&client_cond[n_cassa],&mtxcliente[id]); /* SEGNALE PER CAMBIARE CASSA */
	        	
			while(cl->loading == 0 && sigquit == 0) {

				pthread_mutex_lock(&mtxalgoritmo[id]);
	        		if(algoritmo[id] == 1) {
	                		int new_cassa = decision_algorithm (n_cassa, cl);
	                		if(new_cassa != n_cassa) {
	                			pthread_mutex_lock(&mtxcassa[n_cassa]);
	                			pthread_mutex_lock(&mtxlen[n_cassa]);
	                			customer_remove(&cassa[n_cassa], cl);
	                			len_code[n_cassa]--; 
	                			pthread_mutex_unlock(&mtxlen[n_cassa]);
	                			pthread_mutex_unlock(&mtxcassa[n_cassa]);  
	                			pthread_mutex_lock(&mtxcassa[new_cassa]);
	                			pthread_mutex_lock(&mtxlen[new_cassa]);
	                			customer_push(&cassa[new_cassa], &cl);
	                			len_code[new_cassa]++;
	                			pthread_mutex_unlock(&mtxlen[new_cassa]);
	                			pthread_mutex_unlock(&mtxcassa[new_cassa]);
						n_cassa = new_cassa;
						if(alr_visited[n_cassa] == 0) 
							alr_visited[n_cassa] = 1;
						pthread_cond_signal(&checkout_cond[n_cassa]); 	/* SVEGLIO LA CASSA */  
					}		
					algoritmo[id] = 0;
					pthread_cond_signal(&alg_cond[id]);   /* SVEGLIO L'ALGORITMO */
				}
				pthread_mutex_unlock(&mtxalgoritmo[id]);
				
	                 	pthread_mutex_lock(&mtxcassa[n_cassa]);
	               	if(cassa[n_cassa]->open == 0) {
	               		pthread_mutex_lock(&mtxcambio[id]);
	               		cambio_cassa[id] = 1; 
	               		pthread_mutex_lock(&mtxcambio[id]);
	               		int new_cassa = decision_algorithm (n_cassa, cl);
	               		pthread_mutex_lock(&mtxcassa[new_cassa]);
	               		pthread_mutex_lock(&mtxlen[new_cassa]);
	                		customer_push(&cassa[new_cassa], &cl);
	                		len_code[n_cassa]++;
	                		pthread_mutex_unlock(&mtxlen[new_cassa]);
	                		pthread_mutex_unlock(&mtxcassa[new_cassa]);
	                		n_cassa = new_cassa;
					if(alr_visited[n_cassa] == 0) 
						alr_visited[n_cassa] = 1;
					pthread_cond_signal(&checkout_cond[n_cassa]); /* SVEGLIO LA CASSA */
	             		}
	             		pthread_mutex_unlock(&mtxcassa[n_cassa]);
	               	
	               	pthread_cond_wait(&client_cond[n_cassa], &mtxcliente[id]);   

			}
			
			pthread_mutex_unlock(&mtxcliente[id]);
				
			if ((pthread_join(algo, NULL)) == -1 ) {
                		fprintf(stderr,"Join algoritmo fallita!");
			}
				
			clock_gettime(CLOCK_REALTIME, &store4); 
        		time4 = (store4.tv_sec)*1000 + (store4.tv_nsec) / 1000000;
        		
        		pthread_mutex_lock(&mtxcliente[id]);
        		if(cl->paid == 0)
        			pthread_cond_wait(&client_cond[n_cassa], &mtxcliente[id]); 
       		
    		}
      			
      		if(cl->paid == 1 || sigquit == 1) {

			pthread_mutex_unlock(&mtxcliente[id]);
			clock_gettime(CLOCK_REALTIME, &store2); 
			time2 = (store2.tv_sec)*1000 + (store2.tv_nsec) / 1000000;
			cl->tot_time = time2 - time1;
			cl->queue_time = time4 - time3;
			for(int i = 0; i < txt->K; i++) 
				if(alr_visited[i] == 1)
					cl->casse_visit++;
			free(alr_visited);
			
        		pthread_mutex_lock(&mtxfile);
    			fprintf(final,"CLIENTE -> | id customer:%d | n. bought products:%d | time in the supermarket: %0.3f s | time in queue: %0.3f s | n. queues checked: %d | \n",cl->id, cl->n_acquisti, (double) cl->tot_time/1000, (double) cl->queue_time/1000, cl->casse_visit);
    			pthread_mutex_unlock(&mtxfile);  
    			free(cl);    
    			pthread_mutex_lock(&mtxattivo);
        		cl_attivi--;
        		pthread_mutex_unlock(&mtxattivo);     	
	        	pthread_exit(NULL);
        	}
		
	}
		
	else { /* SE NON HA ACQUISTATO NULLA */
		
		if(sigquit == 0) {
			customer_push(&em_exit, &cl);
		
			/* AVVISA IL DIRETTORE */
			kill(SIGUSR1, dir_pid);
			while(sigcliente == 0);

			customer_remove(&em_exit, cl);
			clock_gettime(CLOCK_REALTIME, &store2); 
			time2 = (store2.tv_sec)*1000 + (store2.tv_nsec) / 1000000;
			cl->tot_time = time2 - time1;
        		fprintf(final,"CLIENTE -> | id cliente:%d | n. prodotti acquistati:%d | tempo tot. nel supermercato: %0.3f s | tempo speso in coda: %0.3f s | n. di code visitate: %d | \n",cl->id, cl->n_acquisti, (double) cl->tot_time/1000, (double) cl->queue_time/1000, cl->casse_visit);
        		free(cl); 
        		pthread_mutex_lock(&mtxattivo);
        		cl_attivi--;
        		pthread_mutex_unlock(&mtxattivo);              	
	    		pthread_exit(NULL); 
	    	}
	}
}	


void *cassiere_t (void* arg)
{
	struct timespec store;
	struct timespec store2;
  	long time1, time2;
  	checkout_data* info = ((checkout_data*)arg);
  	int servicetime, nprod, esci = 0, id = info->id;  
	unsigned int seed = id + sec; 
	
	/* GENERO NUMERO RANDOMICO PER CASSIERE */
	if(cassa[id]-> randomtime == 0)
		while ((cassa[id]->randomtime = rand_r(&seed) % 80) < 20);  //genera un intero in millisecondi

	/* PRENDO TEMPO DI ATTIVAZIONE CASSA */
  	clock_gettime(CLOCK_REALTIME, &store); 
  	time1 = (store.tv_sec)*1000 + (store.tv_nsec) / 1000000;

	while(1) {
    	
  	/* CONTROLLO CHIUSURA */
   	pthread_mutex_lock(&mtxexit[id]);
    	if (exit_cassa[id] == 1)
    		esci = 1;
    	pthread_mutex_unlock(&mtxexit[id]);
    	
	pthread_mutex_lock(&mtxlen[id]);
        	
   	while(len_code[id] == 0 && esci == 0 && sigquit == 0 && !(sighup == 1 && cl_attivi == 0)) { /* IN CASO CODA SENZA CLIENTI */
     			printf("1111\n"); fflush(stdout);
     			pthread_cond_wait(&checkout_cond[id], &mtxlen[id]);
     			printf("2222\n"); fflush(stdout);
     	}
     	pthread_mutex_unlock(&mtxlen[id]);
            		
      	/* CONTROLLO CHIUSURA */
      	pthread_mutex_lock(&mtxexit[id]);
      	if (exit_cassa[id] == 1) 
      		esci = 1;
      	pthread_mutex_unlock(&mtxexit[id]);

    	
        	
    	if (esci == 0 && sigquit == 0 && !(sighup == 1 && cl_attivi == 0)) {    /* IN CASO CHE SIA TUTTO OK */
        		
        	pthread_mutex_lock(&mtxcassa[id]);
        	pthread_mutex_lock(&mtxlen[id]);
      		/* PRENDE CLIENTE DALLA CODA E LO AVVISO CHE STA PAGANDO */
		customer_data* first = customer_pop(&cassa[id]); 
		len_code[id]--;
		pthread_mutex_unlock(&mtxlen[id]);
      		cassa[id]->t_stimato -= cassa[id]->randomtime + txt->Prod_t * first->n_acquisti;
      		pthread_mutex_unlock(&mtxcassa[id]); 
      		pthread_mutex_lock(&mtxcliente[first->id]);
		first->loading = 1;
		pthread_mutex_unlock(&mtxcliente[first->id]);
		pthread_cond_broadcast(&client_cond[id]);     
			
		/* VERIFICA ACQUISTI */
		nprod = first-> n_acquisti;
		servicetime = cassa[id]->randomtime + (txt->Prod_t * nprod); //genera intero in millisecondi
		struct timespec ms = {(servicetime/1000),((servicetime%1000)*1000000)}; /* 1: il tempo in secondi, 2: il resto del tempo in nanosecondi(10^-9)*/
		nanosleep(&ms, NULL);  
		
		/*AGGIORNA DATI CASSA */
		info->n_prodotti += nprod;
      		info->n_clienti++;
      		info->service_time += servicetime;
			
		/* SVEGLIA IL CLIENTE SERVITO */
		pthread_mutex_lock(&mtxcliente[first->id]);
		first->paid = 1;
		pthread_mutex_unlock(&mtxcliente[first->id]);
		pthread_cond_broadcast(&client_cond[id]);  
		
	}
        	
    		if(esci == 1 || sigquit == 1 || (sighup == 1 && cl_attivi == 0)) {	/* IN CASO DI CHIUSURA CASSA*/    
            		
      			if(esci == 1) {

       			pthread_mutex_lock(&mtxexit[id]);
        			exit_cassa[id] = 0;
        			pthread_mutex_unlock(&mtxexit[id]);
            		}
   			
      			/* PRENDO ORARIO DI CHIUSURA CASSA */
     			clock_gettime(CLOCK_REALTIME, &store2);
      			time2 = (store2.tv_sec)*1000 + (store2.tv_nsec) / 1000000;
            		
      			/* AGGIORNO DATI SUPERMERCATO */
      			info->time += time2-time1;
     			info->n_chiusure++;

      			pthread_mutex_lock(&mtxcassa[id]);
    			cassa[id]->open = 0; 
       	 	reset_queue(&cassa[id]);     
        		pthread_mutex_unlock(&mtxcassa[id]);
        		pthread_cond_broadcast(&client_cond[id]);  
            		
      			pthread_exit(NULL);

  		}
  	}
        
}    


void *g_clienti (void* arg)
{
	pthread_t* cliente;
	int i,j;
	
	/* CREAZIONE THREAD CLIENTI */
	if((cliente = malloc(txt->C * sizeof(pthread_t))) == 0) {
		fprintf(stderr, "Allocazione thread cliente fallito");
		exit(EXIT_FAILURE);
	}
	
	/* ATTIVAZIONE THREAD CASSE APERTE */
	for (i=0;i<txt->C;i++) {
		if (pthread_create(&cliente[i], NULL, cliente_t, (void*) (intptr_t) i) != 0) {
            		fprintf(stderr,"Attivazione thread cliente fallito!");
            		exit(EXIT_FAILURE);
            	}	
        	pthread_mutex_lock(&mtxattivo);
        	cl_attivi++;
        	pthread_mutex_unlock(&mtxattivo);
        }
        
    	/* ENTRATA NUOVI CLIENTI */
    	while(sigquit == 0 && sighup == 0 ) {
    		pthread_mutex_lock(&mtxattivo);
      		if(cl_attivi <= txt->C - txt->E) {
       		reallocate_array(i, i+txt->E);
       		for(j=i;j<i+txt->E;j++) {
       			if(sigquit == 0 && sighup == 0 ) {
       				if((cliente = realloc(cliente, i+txt->E * sizeof(pthread_t))) == 0) {
						fprintf(stderr, "Allocazione thread cliente fallito");
						exit(EXIT_FAILURE);
					}
					
					if (pthread_create(&cliente[i], NULL, cliente_t, (void*) (intptr_t) i) != 0) {
        	   				fprintf(stderr,"Attivazione thread cliente fallito!");
        	   				exit(EXIT_FAILURE);
         	  			}	
       				cl_attivi++;
       			}
            		}
       		i=j;
        	}
        	pthread_mutex_unlock(&mtxattivo);
    	} 

    	for (j=0;j<i+1;j++)
       	if (pthread_join(cliente[j],NULL) == -1 ) 
            		fprintf(stderr,"Join cliente fallita!");

    	free(cliente);
    	pthread_exit(NULL);

}
	

void *g_casse (void* arg)
{
	pthread_t* cassiere;
	pthread_t d_news;
  	char buf[7];
  	int i;
	
	/* CREAZIONE THREAD CASSIERE */
	if((cassiere = malloc(txt->K * sizeof(pthread_t))) == 0) {
		fprintf(stderr, "Allocazione thread cassiere fallito");
		exit(EXIT_FAILURE);
	}
	
	/* ATTIVAZIONE THREAD CASSE APERTE */
	for (i = 0; i < txt->Open; i++) {
		pthread_mutex_lock(&mtxcassa[i]);
    		cassa[i]->open = 1;
    		pthread_mutex_unlock(&mtxcassa[i]);
    		pthread_mutex_lock(&mtxlen[i]);
    		len_code[i]=0;
    		pthread_mutex_unlock(&mtxlen[i]);
		if (pthread_create(&cassiere[i], NULL, cassiere_t, &((checkout_data*)arg)[i]) != 0) {
      			fprintf(stderr,"Attivazione thread cassiere fallito");
      			exit(EXIT_FAILURE);
    		}
  	} 
  	
  	/* ATTIVAZIONE THREAD NEWS DEL DIRETTORE */
  	if (pthread_create(&d_news, NULL, news_t, NULL) != 0) {    
   		fprintf(stderr,"Creazione thread news fallita!");
   		exit(EXIT_FAILURE);
  	}
  		        
  	while(sigquit == 0 || !(sighup == 1 && cl_attivi == 0)) {
        	
        	if(sigcasse == 0)
        		continue;
        		
  		if((read(fd_skt, (char*) buf, sizeof(buf))) == -1) {
    			fprintf(stderr,"g_casse: Read fallita!");
    			exit(EXIT_FAILURE);
    		}
    		
    		
    		if (sigquit == 0 || !(sighup == 1 && cl_attivi == 0)) {
    			
    			if(strcmp(buf, "Apri") == 0) {
    				
    				for(i=0; i< txt->K; i++) {
    					pthread_mutex_lock(&mtxcassa[i]);
    					if(cassa[i]->open == 0) {
    						cassa[i]->open = 1; 
    						pthread_mutex_unlock(&mtxcassa[i]);
          					break;
       	 			}	
        				pthread_mutex_unlock(&mtxcassa[i]);  
        			}
        				if (pthread_create(&cassiere[i], NULL, cassiere_t, &((checkout_data*)arg)[i]) != 0) {
       	  				fprintf(stderr,"Attivazione thread cassiere fallito");
          					exit(EXIT_FAILURE);
	      				}
			}
	
      			else if (strcmp(buf, "Chiudi") == 0) {
        			
        			for(i=0; i<txt->K; i++)
        				if(len_code[i] >= txt->S2)
        					break;
        			
        			pthread_mutex_lock(&mtxexit[i]);			  	
      				exit_cassa[i] = 1;
      				pthread_mutex_lock(&mtxexit[i]);
    				pthread_cond_signal(&checkout_cond[i]); 
       	
    			}
    		}
    		
    		else 
    			break;
    	}

  	/* ASPETTO LA TERMINAZIONE DI TUTTE LE CASSE */	
  	for (i=0;i<txt->K;i++){
    		pthread_cond_signal(&checkout_cond[i]);
    		if (pthread_join(cassiere[i],NULL) == -1 )
     			fprintf(stderr,"Join cassiere fallita!");
  	}
         
  	free(cassiere);
  	pthread_exit(NULL); 	
}

int main(int argc, char* argv[])
{
	checkout_data* data;
	int tot_clienti=0, tot_prodotti=0;
	sec = time(NULL);
	pthread_t gestorecasse;
	pthread_t gestoreclienti;
	struct sigaction sa;
	
	/* INIZIALIZZAZIONE */
	if (argc!=2) {
       	fprintf(stderr,"Usage: %s /config1.txt oppure /config2.txt \n",argv[0]);
        	exit(EXIT_FAILURE);
 	}

   	if (strcmp(argv[1],"./config1.txt") == 0  || strcmp(argv[1],"./config2.txt") == 0) {
        	if ((txt = test(argv[1])) == NULL)
            		exit(EXIT_FAILURE);
    	}
    
    	else {
        	fprintf(stderr,"%s: Inserire un file di configurazione come argomento \n",argv[0]);
        	exit(EXIT_FAILURE);
    	}

	superm_pid = getpid(); 
	
	/* GESTIONE SEGNALI */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handler;
	if(sigaction(SIGQUIT, &sa, NULL))
		fprintf(stderr,"Errore SIGQUIT");
	if(sigaction(SIGHUP, &sa, NULL))
		fprintf(stderr,"Errore SIGHUP");
	if(sigaction(SIGUSR1, &sa, NULL))
		fprintf(stderr,"Errore SIGUSR1");

	
	/* CREAZIONE SOCKET */	
	ConnectSocket();

	if ((final = fopen("final.log", "w")) == NULL) { 
		fprintf(stderr, "Apertura file .log fallita!");
		exit(EXIT_FAILURE);
    	}

	/* ALLOCAZIONE DATABASE CASSE */
	if((data = malloc (txt->K * sizeof(checkout_data))) == 0) {
		fprintf(stderr, "Allocazione array di dati delle casse fallita!\n");
	    	exit(EXIT_FAILURE);
	}
	
	for (int i = 0; i < txt->K; i++)	
		checkout_setup(&data[i], i);

	allocate_all();
	
	/* CREAZIONE THREAD GESTORE CASSE */
	if((pthread_create(&gestorecasse, NULL, g_casse, (void*) data)) != 0) {		
		fprintf(stderr,"Creazione thread gestore casse fallita!");
        	exit(EXIT_FAILURE);
    	}
    	
    	/* CREAZIONE THREAD GESTORE CLIENTI */
	if((pthread_create(&gestoreclienti, NULL, g_clienti, NULL)) != 0) {		
		fprintf(stderr,"Creazione thread gestore clienti fallita!");
        	exit(EXIT_FAILURE);
    	}
    	
    	/* ATTENDO THREAD CASSE */
    	if (pthread_join(gestorecasse,NULL) != 0) 
            fprintf(stderr,"Join thread gestore casse fallita!");
    	
	/* ATTENDO THREAD CLIENTI */
    	if (pthread_join(gestoreclienti,NULL) != 0 ) 
            fprintf(stderr,"Join thread gestore clienti fallita!");
            	
    	for (int i=0; i < txt->K; i++) {
    		
        	fprintf(final, "CASSIERE -> | id:%d | n. prodotti elaborati :%d | n. clienti serviti:%d | tempo tot. di apertura: %0.3f s | tmepo medio di servizio: %0.3f s | n. di chiusure :%d |\n",data[i].id, data[i].n_prodotti, data[i].n_clienti, (double) data[i].time/1000, (data[i].service_time/data[i].n_clienti)/1000, data[i].n_chiusure);
     		tot_clienti += data[i].n_clienti;
        	tot_prodotti += data[i].n_prodotti;
   	}

    	fprintf(final, "TOTALE CLIENTI SERVITI: %d\n", tot_clienti);
    	fprintf(final, "TOTALE PRODOTTI ACQUISTATI: %d\n", tot_prodotti);

    	close(fd_skt);
    	fclose(final);
    	free(data);
    	deallocate_all(); 
    	free(txt); 
    	kill(dir_pid, SIGUSR1); 
	
	return 0;	
}
