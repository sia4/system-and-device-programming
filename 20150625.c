#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define MAX_EL 10

typedef struct {
	int id;
	char type;
} Params;

/**
* count = 0 -> not finished yet
*  count = 1 -> finished
*  count = 2 -> merged with another th of the same type
*  count = 3 -> merged with a couple of the other type
*/

typedef struct {
	int count; 
	int id_merged;
} Counter;

typedef struct {
	int count;
	pthread_mutex_t cs;
} Collector;

Counter *counter_a, *counter_b;

/**
* Version 1: 1 lock for both arrays
* Version 2: 2 locks, one for each arrays
*/

// -- version 1
//pthread_mutex_t lock;
// -- version 2
pthread_mutex_t lock_a, lock_b;
int nThread;
Collector collector; //Used to release memory and destroy sem

Params *params;
pthread_t *th_a, *th_b;

static void *merge_files(void *);

int main (int argc, char *argv[]) {

	int i;
	int ret_code;

	if(argc != 2) {
		fprintf(stderr, "Missing parameters. Usage: program even_integer");
		exit(1);	
	}

	nThread = atoi(argv[1]);
	if((nThread % 2) != 0) {
		fprintf(stderr, "Wrong parameter. Usage: program even_integer");
		exit(1);
	}		

	/* Memory allocation */
	counter_a = (Counter *) malloc(nThread *sizeof(Counter));
	counter_b = (Counter *) malloc(nThread *sizeof(Counter));
	
	th_a = (pthread_t *)malloc(nThread * sizeof(pthread_t));
	th_b = (pthread_t *)malloc(nThread * sizeof(pthread_t));
	params = (Params *)malloc(nThread * 2 * sizeof(Params));

	/* Init mutex */
	// -- version 1
	//pthread_mutex_init(&lock, NULL);
	// -- version 2
	pthread_mutex_init(&lock_a, NULL);
	pthread_mutex_init(&lock_b, NULL);
	pthread_mutex_init(&collector.cs, NULL);

	collector.count = 0;

	/* Generating all threads */
	for(i = 0; i < nThread; i++) {
	
		printf("Create thread %d\n", i);
		/* Init counter single el (calloc?)*/
		counter_a[i].count = 0;
		counter_b[i].count = 0;

		/* Create thread Ai */
		params[i].id = i;
		params[i].type = 'A';
		ret_code = pthread_create(&th_a[i], NULL, merge_files, (void *) &params[i]);
		if(ret_code != 0) {
			fprintf(stderr, "Error creating thread.\n");
			exit(1);
		}		

		/* Create thread Bi */
		params[i+nThread].id = i;
		params[i+nThread].type = 'B';
		ret_code = pthread_create(&th_b[i], NULL, merge_files, (void *) &params[i+nThread]);
		if(ret_code != 0) {
			fprintf(stderr, "Error creating thread.\n");
			exit(1);
		}
	}

	pthread_exit(0);	
}


static void * merge_files(void *params) {

	Params *p = (Params *) params;
	int found, i, j, k;
	
	pthread_detach (pthread_self ());
	/* Sleep rand time between 0 and 3 sec */
	srand(time(NULL));
	sleep(rand()%4);

	//Process file
	//printf("Outside lock %d", p->id);

	if(p->type == 'A') {

		// -- version 1
		//pthread_mutex_lock(&lock);
		// -- version 2
		pthread_mutex_lock(&lock_a);

		//printf("Inside lock A%d", p->id);

		/* Search for an element with same type with count = 1 */
		found = 0;
		for(i = 0; i < nThread; i++) {
			if(counter_a[i].count == 1) {
				found = 1;				
				break;
			}
		}

		if(!found) { /* If not found just increase my count */
			counter_a[p->id].count++; //count = 1 -> finished
			// -- version 2
			pthread_mutex_unlock(&lock_a); /* And it just finishes */
		} else {
			counter_a[p->id].count += 2; //count = 2 -> merged
			counter_a[i].count++; //count = 2 -> merged
			counter_a[p->id].id_merged = i;
			counter_a[i].id_merged = p->id;

			printf("%c%d cats: %c%d %c%d\n", p->type, p->id, p->type, p->id, p->type, i);
			// -- version 2
			pthread_mutex_unlock(&lock_a);
			pthread_mutex_lock(&lock_b);
			/* Search for an element of the other type with count = 2*/
			found = 0;
			for(j = 0; j < nThread; j++) {
				if(counter_b[j].count == 2) {
					found = 1;
					k = counter_b[j].id_merged;		
					break;
				}
			}

			if(found) {
				counter_a[p->id].count++; //count = 3 -> double merged
				counter_a[i].count++; //count = 3 -> double merged
				counter_b[j].count++; //count = 3 -> double merged
				counter_b[k].count++; //count = 3 -> double merged
			
				printf("\t\t\tA%d merges: A%d A%d B%d B%d\n", p->id, p->id, i, j, k);
			}

			// -- version 2
			pthread_mutex_unlock(&lock_b);
		}
		
		// -- version 1
		//pthread_mutex_unlock(&lock);
		
	}

	if(p->type == 'B') {

		// -- version 1
		//pthread_mutex_lock(&lock);
		// -- version 2
		pthread_mutex_lock(&lock_b);

		/* Search for an element with same type with count = 1 */
		found = 0;
		for(i = 0; i < nThread; i++) {
			if(counter_b[i].count == 1) {
				found = 1;				
				break;
			}
		}

		if(!found) { /* If not found just increase my count */
			counter_b[p->id].count++; //count = 1 -> finished
			// -- version 2
			pthread_mutex_unlock(&lock_b); /* And it just finishes */
		} else {
			counter_b[p->id].count += 2; //count = 2 -> merged
			counter_b[i].count++; //count = 2 -> merged
			counter_b[p->id].id_merged = i;
			counter_b[i].id_merged = p->id;

			printf("%c%d cats: %c%d %c%d\n", p->type, p->id, p->type, p->id, p->type, i);

			// -- version 2
			pthread_mutex_unlock(&lock_b);
			pthread_mutex_lock(&lock_a);
			/* Search for an element of the other type with count = 2*/
			found = 0;
			for(j = 0; j < nThread; j++) {
				if(counter_a[j].count == 2) {
					found = 1;
					k = counter_a[j].id_merged;		
					break;
				}
			}

			if(found) {
				counter_b[p->id].count++; //count = 3 -> double merged
				counter_b[i].count++; //count = 3 -> double merged
				counter_a[j].count++; //count = 3 -> double merged
				counter_a[k].count++; //count = 3 -> double merged
			
				printf("\t\t\tA%d merges: A%d A%d B%d B%d\n", p->id, p->id, i, j, k);
			}
		
			// -- version 2
			pthread_mutex_unlock(&lock_a);
		}

		// -- version 1
		//pthread_mutex_unlock(&lock);

	}

	/* If last thread destroy and free all */
	pthread_mutex_lock(&collector.cs);
	collector.count++;
	if(collector.count != 2 * nThread) {

		pthread_mutex_unlock(&collector.cs);
		pthread_exit(0);
	}

	pthread_mutex_unlock(&collector.cs);
	// -- version 1
	//sem_destroy(&lock));
	// -- version 2
	sem_destroy(&lock_a);
	sem_destroy(&lock_b);
	sem_destroy(&collector.cs);

	free(counter_a);
	free(counter_b);
	free(params);

	pthread_exit(0);

}
