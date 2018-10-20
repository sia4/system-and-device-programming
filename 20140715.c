#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

typedef struct {
	int count;
	pthread_mutex_t cs;
	int finished;
} Counter;
	

int n, k;
double numer, denum, result;
Counter counter;
sem_t sem_num, sem_den;

static void * numerator(void *);
static void * denumerator(void *);

int main (int argc, char *argv[]) {

	int ret_code;
	pthread_t th_num, th_den;

	if(argc != 3) {
		fprintf(stderr, "Missing parameters. Usage: program n k\n");
		exit(1);	
	}

	/* Init all */
	n = atoi(argv[1]);
	k = atoi(argv[2]);
	numer = denum = result = 1;
	counter.count = counter.finished = 0;
	pthread_mutex_init(&counter.cs, NULL);
	sem_init(&sem_num, 0, 1);
	sem_init(&sem_den, 0, 1);	

	/* Create threads */
	ret_code = pthread_create(&th_num, NULL, numerator, NULL);
	if(ret_code != 0) {
		fprintf(stderr, "Error creating thread.\n");
		exit(1);
	}

	ret_code = pthread_create(&th_den, NULL, denumerator, NULL);
	if(ret_code != 0) {
		fprintf(stderr, "Error creating thread.\n");
		exit(1);
	}

	/* Wait for thread terminating */
	ret_code = pthread_join (th_num, NULL);
	if(ret_code != 0) {
		fprintf(stderr, "Error creating thread.\n");
		exit(1);
	}
	ret_code = pthread_join (th_den, NULL);
	if(ret_code != 0) {
		fprintf(stderr, "Error creating thread.\n");
		exit(1);
	}

	printf("Binomial coefficient: %d\n", (int)result);
	sem_destroy(&sem_num);
	sem_destroy(&sem_den);

	return (0);
}

static void * numerator(void * params) {

	int supp;

	supp = n;

	while(supp >= (n - k + 1)) {

		sem_wait(&sem_num);		
		if(supp == (n - k + 1)) {
			numer = supp;
			supp--;
		}else {
			numer = supp * (supp - 1);
			supp -= 2;
		}

		//printf("Partial numer = %f\n", numer);
		pthread_mutex_lock(&counter.cs);
		counter.count++;
		if((counter.count % 2) == 0 || counter.finished > 0) {
			result *= numer/denum;
			//printf("sono dentro numer: %f %f %f\n", result, numer, denum);
			pthread_mutex_unlock(&counter.cs);
			sem_post(&sem_num);
			sem_post(&sem_den);
		}
		pthread_mutex_unlock(&counter.cs);
	}

	sem_wait(&sem_num);
	numer = 1;
	pthread_mutex_lock(&counter.cs);
	counter.finished++;
	pthread_mutex_unlock(&counter.cs);
	
	return (0);
}

static void * denumerator(void * params) {

	int supp = 1;
	
	while(supp <= k) {

		sem_wait(&sem_den);

		if(supp == k) {
			denum = supp;
			supp++;
		}else{
			denum = supp * (supp + 1);
			supp += 2;
		}
		//printf("Partial denum : %f\n", denum);
		pthread_mutex_lock(&counter.cs);
		counter.count++;
		if((counter.count % 2) == 0 || counter.finished > 0) {
			
			result *= numer/denum;
			//printf("sono dentro denum: %f %f %f\n", result, numer, denum);			
			pthread_mutex_unlock(&counter.cs);
			sem_post(&sem_num);
			sem_post(&sem_den);
		}
		pthread_mutex_unlock(&counter.cs);

	}

	sem_wait(&sem_den);
	denum = 1;
	pthread_mutex_lock(&counter.cs);
	counter.finished++;
	pthread_mutex_unlock(&counter.cs);
	
	return (0);


}
