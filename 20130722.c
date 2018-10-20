#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#define CLOCKWISE 0
#define COUNTERCLOCKWISE 1

int nT, nS;

typedef struct {
	pthread_mutex_t clockwise;
	pthread_mutex_t counterclockwise;
} Station;

typedef struct {
	int id;
	int direction;
	int id_station;
} Train;

typedef struct {
	int station1;
	int station2;
	pthread_mutex_t cs;
} ConnectionTrack;

typedef struct {
	FILE *fp;
	pthread_mutex_t cs;
} File;
	

Station *stations;
Train *trains;
ConnectionTrack *connection_tracks; //between station i and station i+1
File file;

static void * train (void * params);
void select_station_and_track(int *,int *, int);

int main (int argc, char *argv[]) {

	pthread_t *th;	
	int ret_code, i;

	if(argc != 3) {
		fprintf(stderr, "Missing parameters. Usage: program n_station n_trains\n");
		exit(1);	
	}
	
	nS = atoi(argv[1]);
	nT = atoi(argv[2]);

	/* Init */
	stations = (Station *)malloc(nS * sizeof(Station));
	trains = (Train *)malloc(nT * sizeof(Train));
	connection_tracks = (ConnectionTrack *)malloc(nS * sizeof(ConnectionTrack));
	th = (pthread_t *)malloc(nT * sizeof(pthread_t));

	file.fp = fopen("log.txt", "w");

	pthread_mutex_init(&file.cs, NULL);

	for(i = 0; i < nS; i++) {
		pthread_mutex_init(&stations[i].clockwise, NULL);
		pthread_mutex_init(&stations[i].counterclockwise, NULL);

		connection_tracks[i].station1 = i;
		connection_tracks[i].station2 = (i+1)%nS;
		pthread_mutex_init(&connection_tracks[i].cs, NULL);
			
	}

	for(i = 0; i < nT; i++) {
	
		trains[i].id = i;
		select_station_and_track(&trains[i].id_station, &trains[i].direction, i);
		ret_code = pthread_create(&th[i], NULL, train, (void *) &trains[i]);
		if(ret_code != 0) {
			fprintf(stderr, "Error creating thread.\n");
			exit(1);
		}

	}
	
	pthread_exit(0);
}

static void * train (void * params) {

	Train *t = (Train *) params;
	int sum;

	pthread_detach (pthread_self ());

	if(t->direction == CLOCKWISE) {
		sum = 1;
		pthread_mutex_lock(&stations[t->id_station].clockwise);
	}else {
		sum = -1;
		pthread_mutex_lock(&stations[t->id_station].counterclockwise);
	}
	
	while(1) {		

		sleep(1);
		pthread_mutex_lock(&file.cs);
		fprintf(file.fp, "Train number %d, in station %d going %d\n", t->id, t->id_station, t->direction);
		fflush(file.fp);
		pthread_mutex_unlock(&file.cs);

		if(t->direction == CLOCKWISE) {
			pthread_mutex_unlock(&stations[t->id_station].clockwise);
		}else {
			pthread_mutex_unlock(&stations[t->id_station].counterclockwise);
		}

		pthread_mutex_lock(&connection_tracks[(t->id_station+sum)%nS].cs);
		pthread_mutex_lock(&file.cs);
		fprintf(file.fp,"Train number %d, travelling toward station %d\n", t->id, (t->id_station+sum)%nS);
		fflush(file.fp);
		pthread_mutex_unlock(&file.cs);		
		sleep(1);

		
		pthread_mutex_unlock(&connection_tracks[(t->id_station+sum)%nS].cs);

		if(t->direction == CLOCKWISE) {
			pthread_mutex_lock(&stations[(t->id_station+sum)%nS].clockwise);
		}else {
			pthread_mutex_lock(&stations[(t->id_station+sum)%nS].counterclockwise);
		}	
		pthread_mutex_lock(&file.cs);
		fprintf(file.fp,"Train number %d, arrived in station %d\n", t->id, (t->id_station+sum)%nS);
		fflush(file.fp);
		pthread_mutex_unlock(&file.cs);

		t->id_station = (t ->id_station + sum)%nS;

	}

}

void select_station_and_track(int * station,int * direction, int id) {

	if(id == 0) {
		*station = 0;
		*direction = CLOCKWISE;
	}

	if(id == 1) {
		*station = 2;
		*direction = COUNTERCLOCKWISE;
	}

	if(id == 2) {
		*station = 3;
		*direction = CLOCKWISE;
	}


	if(id == 3) {
		*station = 1;
		*direction = COUNTERCLOCKWISE;
	}

	if(id == 4) {
		*station = 4;
		*direction = CLOCKWISE;
	}

	return;
}


