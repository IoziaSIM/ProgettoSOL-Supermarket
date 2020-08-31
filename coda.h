#include "./dati_cliente.h"


typedef struct checkout {
	struct queue* head;
	int open;
	int t_stimato;
	int randomtime;
} checkout;

typedef struct queue {
	customer_data* csd;    
	struct queue* next;
} queue;

checkout* checkout_init() {
	struct checkout* ch = malloc(sizeof(checkout));
	ch->head = NULL;
	ch->open = 0;
	ch->t_stimato = 0;
	ch->randomtime = 0;
	return ch;
}

int len_queue(checkout* cassa)
{
	queue* curr = cassa->head;
	int count = 0;
	if(curr!=NULL){
		count++;
		curr = curr->next;
	}
	return count;
}


customer_data* customer_pop (checkout** cassa)
{
	queue* q = (*cassa)->head;
    	(*cassa)->head = ((*cassa)->head)->next;    
    	customer_data * tmp = q->csd;
    	free(q);
    	return tmp;

}


void reset_queue(checkout** cassa)
{
	while((*cassa)->head != NULL) {
        	queue* q = (*cassa)->head;
        	(*cassa)->head = ((*cassa)->head)->next;
        	free(q);
    }    
}


void customer_push (checkout** cassa, customer_data** cl)  
{
    queue* q = malloc(sizeof(queue));
    q->csd = (*cl);
    q->next = NULL;
    queue* curr = (*cassa)->head;
    if ((*cassa)->head == NULL){
        (*cassa)->head = q;
        return;
    }
    while(curr->next != NULL) 
    	curr=curr->next;
    curr->next = q;
}


void customer_remove(checkout** cassa, customer_data* cl)     
{
	queue* curr = (*cassa)->head->next;
	queue* prec = (*cassa)->head;
	while(curr->csd->id != cl->id) {  
		prec = curr;
		curr = curr->next;
	}
	prec->next = curr->next;
	free(curr);
} 

