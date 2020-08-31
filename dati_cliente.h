typedef struct customer_data {
	int id;
    	int n_acquisti;
    	int tot_time;
    	int queue_time;
    	int casse_visit;
    	int spesa;
    	int loading;
    	int paid;
} customer_data;

void customer_setup (customer_data* csd, int i) {
    	csd->id = i;
    	csd->n_acquisti = 0;
    	csd->tot_time = 0;
    	csd->queue_time = 0;
    	csd->casse_visit = 0;
    	csd->spesa = 0; 
    	csd->loading = 0;
    	csd->paid = 0;  
}
