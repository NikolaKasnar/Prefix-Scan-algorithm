#include <stdio.h>
#include <stdlib.h>
#include <math.h> // Potrebno dodati "-lm" pri kompilaciji koda, inace se dobiva greska pri kompilaciji jer ne prepoznaje log2 funkciju
#include <pthread.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int counter = 0;
int limit;

void synchronize()
{
    pthread_mutex_lock(&mutex);
    counter++;
    if (counter == limit)
    {
        counter = 0;
        pthread_cond_broadcast(&cond);
    }
    else
    {
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);
}

typedef struct zbrajanjeNiza
{
	int num_threads;
	int MYPROC; //broj dretve
	int n;
	int *x;
	int *y;
	int *local_y;
} zbrajanjeNiza;

void *zbrajanje(void *ptr){

    zbrajanjeNiza *data = (zbrajanjeNiza*) ptr;
    int num_threads = data->num_threads;
	int MYPROC = data->MYPROC;
	int n = data->n;
	int *x = data->x;
	int *y = data->y;
	int *local_y = data->local_y;

    /* Moramo izracunati pocetni i zavrsnji indeks bloka polja koji obradjuje ova dretva. */
	int block;
	int first;
	int last;
	int i;

    int small = n / num_threads;
    int large = small + 1;
    int num_large = n % num_threads;
    block = (MYPROC < num_large ? large : small);
    if ( MYPROC < num_large ){
        first = MYPROC * large;
    }
    else{
        first = num_large * large + (MYPROC-num_large) * small;
    }

    last = first + block;


    int k=(int)log2((double)(last-first));
    int m=num_threads/2; // Za prvo penjanje je m=num_threads zbog velicine bloka
    int step=1;
    int j;

    // U y spremim sume svakog bloka ovisno o dretvi(izracunate sekvencijalno), nakon toga izgleda ovako: 7777666666666666
    for(i=first; i<=last-1; i++){
        y[MYPROC]+=x[i];
    }
    synchronize();// Sve dretve trebaju zavrsiti prije nego krenemo na algoritam

    k=(int)log2((double)(num_threads));; // Predefiniram k da bude kao u algoritmu
    
    // printf("%d", k);

    //if(MYPROC==15){
    //    for (i = 0; i < m; i++)
    //    {
    //        printf("%d::%d: ", MYPROC, i);
    //        printf("%d\n", local_y[i]);
    //    }
    //}

    
    // Prvo penjanje
    for (i = 0; i <= m-1; i++)
    {
        if (MYPROC==i){
            local_y[2*i+1]=y[2*i]+y[2*i+1];
        }
    }
    synchronize();

    for (j = 2; j <= k; j++)
    {
        m=m/2;
        //if(j>2){
        //    step=2*step;// step ostaje 1 za drugi krug penjanja jer je local_y duplo manji od y
        //}
        step=2*step;
        for (int i = 1; i <= m; i++)
        {
            if (MYPROC==i*step-1)
            {
                local_y[2*MYPROC+1]= local_y[2*MYPROC-step+1]+local_y[2*MYPROC+1];
            }
        }
        synchronize(); //cekamo da sve dretve zavrse racunanje svog bloka
    }

    // Prvo spustanje
    for (j = 1; j <= k-2; j++)
    {
        m=2*m;
        step=step/2;
        for(i=2; i<=m; i++)
        {
            if (MYPROC==i*step-1)
            {
                local_y[2*MYPROC-step+1]= local_y[2*MYPROC-2*step+1]+local_y[2*MYPROC-step+1] ;
            }
        }
        synchronize();
    }

    m=2*m;

    for(i=0; i <= m-1; i++)
    {
        if(MYPROC==0)
        {
            local_y[0]=y[0];
        }
        else
        {
            if(MYPROC==i)
            {
                local_y[2*MYPROC]= local_y[2*MYPROC-1]+y[2*MYPROC];
            }
        }
    }
    synchronize();
    
    // Sekvencijalni prefix i scan algoritam z niz y
    // Prvo spremim sve brojeve iz x u local_y2 ovisno o tome koliki blok dana dretva nadgleda
    int tmp=first;
    int local_y2[(last-first)]; // Polje local_y2 je velicine blocka koji dretva racuna
    for(i=0; i<(last-first); i++){
        local_y2[i]=x[tmp++];
    }

    // if(MYPROC==0){
    //     for(i=0; i<(last-first); i++){
    //         printf("%d", local_y2[i]);
    //     }
    //     printf(" test:::\n%d", first);
    // }

    j=0;
    for(i=first; i<last; i++){
        if(MYPROC==0){
            if(i==0){
                y[0]=0+local_y2[j];// U slucaju MYPROC==0 ne postoji "prva" parcijalna suma u local_y, ona je jednaka 0
                j++;
            }
            else{
                y[j]=y[j-1]+local_y2[j];// Zbrajam ono sto se nalazi u prehodnom clanu od y sa sljedecim clanom local_y2
                j++;
            }
        }
        else{
            if(i==first){
                y[i]=local_y[MYPROC-1]+local_y2[j];// Zbrajam parcijalne sume iz local_y sa local_y2
                j++;
            }
            else{
                y[i]=y[i-1]+local_y2[j];
                j++;
            }
        }
    }

}


main(){

    // Unesite koliku duljinu od x zelite:

	int n = 100;

    // Unesite koliko dretvi zelite:
    
    int num_threads = 16;

	pthread_t *threads;
	zbrajanjeNiza *arg;
	int *x, *y, *local_y;
	int i;

	limit = num_threads;
	threads = (pthread_t*) malloc((num_threads-1)*sizeof(pthread_t));
	x = (int*) malloc(n*sizeof(int));
	y = (int*) malloc(n*sizeof(int));
	local_y = (int*) malloc(num_threads*sizeof(int));
	arg = (zbrajanjeNiza*) malloc(num_threads*sizeof(zbrajanjeNiza));

    /* Punimo elemente niza x i y( u zadatku za provjeru nas pita da x napunimo sa jedinicama)*/
	for ( i = 0; i < n; i++ )
	{
		x[i] = 1;
        y[i]=0;
	}

    for ( i = 0; i < num_threads-1; i++ )
    {
        /* Punimo podatke u strukturu koja je argument dretvene funkcije. */
		(arg+i+1)->num_threads = num_threads;
		(arg+i+1)->MYPROC = i + 1;
		(arg+i+1)->n = n;
		(arg+i+1)->x = x;
		(arg+i+1)->y = y;
		(arg+i+1)->local_y = local_y;
        /* Stvaramo nezavisne dretve koje ce izvrsavati funkciju. */
        pthread_create( &threads[i], NULL, zbrajanje, (void*) (arg+i+1));
     }

	/* I glavna dretva izvrsava istu funkciju. */
	arg->num_threads = num_threads;
	arg->MYPROC = 0;
	arg->n = n;
	arg->x = x;
	arg->y = y;
	arg->local_y = local_y;
	zbrajanje( (void*) arg );

	/* Glavna dretva ceka da dretve zavrse sa radom. */
	for ( i = 0; i < num_threads-1; i++ )
	{
		pthread_join( threads[i], NULL);
	}

    printf("local_y na kraju izgleda ovako:\n");
    for (i = 0; i < num_threads; i++)
    {
        printf("%d: %d\n", i, local_y[i]);
    }

    printf("\nIspis od y:\n");
    for (i = 0; i < n; i++)
    {
        printf("%d: %d\n", i, y[i]);
    }
    
    

    free(threads);
	free(x);
	free(y);
	free(local_y);
	free(arg);
	exit(0);
}