typedef struct checkout_data {
	int id;
   	int n_prodotti;
   	int n_clienti;
   	int time;
    	float service_time;
    	int n_chiusure;
} checkout_data;


void checkout_setup (checkout_data* cassa, int i) {
    cassa->id = i;
    cassa->n_prodotti = 0;
    cassa->n_clienti = 0;
    cassa->time = 0;
    cassa->service_time = 0;
    cassa->n_chiusure = 0; 
}

