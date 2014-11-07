#include <stdio.h>
	// for printf()
#include <pthread.h>
	// for pthread~
#include <stdlib.h>
	// for srand(), rand(), malloc(), free()
#include <time.h>
	// for clock_gettime(), sleep(), struct timespec


#ifdef __MACH__
#include <sys/time.h>

#define CLOCK_REALTIME 0

//clock_gettime is not implemented on OSX
int clock_gettime(int clk_id, struct timespec* t) {
	struct timeval now;
	int rv = gettimeofday(&now, NULL);
	if (rv) return rv;
	t->tv_sec  = now.tv_sec;
	t->tv_nsec = now.tv_usec * 1000;
	return 0;
}
#endif

#define NUM_OF_WRITER	3	
#define NUM_OF_READER	10	

#define TYPE_WRITER 0
#define TYPE_READER 1

pthread_mutex_t mutex;
pthread_mutex_t wrt;

int readcount = 0;

int shared_val;

void *pthread_reader(void *thread_num);
void *pthread_writer(void *thread_num);

void init();
void sleeping();

void msg_waiting			(int thread_type, int thread_num);
void msg_num_of_readers	(int thread_num, int num_of_readers);
void msg_complete			(int thread_type, int thread_num, int written_data);

int main(void) {
	pthread_t writer[NUM_OF_WRITER], reader[NUM_OF_READER];
	void *writer_ret[NUM_OF_WRITER], *reader_ret[NUM_OF_READER];

	int i;

	init();

	// Writter thread creation	
	for(i=0;i<NUM_OF_WRITER;i++) {
		int *num = (int *)malloc(1 * sizeof(int));
		*num = i;

		pthread_create(&writer[i], NULL, pthread_writer, num);
	}
	
	// Reader thread creation
	for(i=0;i<NUM_OF_READER;i++) {
		int *num = (int *)malloc(1 * sizeof(int));
		*num = i;

		pthread_create(&reader[i], NULL, pthread_reader, num);
	}

	// Waiting for reader and writer threads to quit.
	for(i=0;i<NUM_OF_WRITER;i++)	pthread_join(writer[i], &writer_ret[i]);
	for(i=0;i<NUM_OF_READER;i++)	pthread_join(reader[i], &reader_ret[i]);

	// Mutex destruction
	pthread_mutex_destroy(&mutex);
	pthread_mutex_destroy(&wrt);

}

void *pthread_reader(void *thread_num) {
	int reader_num = *(int *)thread_num;
	free((int *)thread_num);

	sleeping();

	msg_waiting(TYPE_READER, reader_num);	

	// ---
	// reader 쓰레드들이 공유하는 readcount 변수에 접근하기 위해 mutex 뮤텍스에 lock을 잡는다.
	// 다른 reader 쓰레드가 lock을 잡고 있으면 readcount 변수를 사용하는 중 이므로 unlock 될 때까지
	// 대기한다. 
	// lock 을 잡으면 readcout 를 +1 하고 readcount 가 1일 경우 처음 진입하는 reader 이므로 
	// shared_val 을 안전하게 읽을 수 있도록 wrt 뮤텍스에 lock을 잡는다.
	// ---
	pthread_mutex_lock(&mutex);
	readcount++;
	if(readcount == 1)
	{
		pthread_mutex_lock(&wrt);
	}
	
	msg_num_of_readers(reader_num, readcount);	

	// ---
	// readcount 변수를 다른 reader 쓰레드들이 사용할 수 있도록 mutex 뮤텍스를 unlock 한다.
	// ---
	pthread_mutex_unlock(&mutex);

	msg_complete(TYPE_READER, reader_num, shared_val);

	// --- 
	// shared_val 변수 읽기를 끝냈으므로 다시 mutex 뮤텍스에 lock을 잡고
	// readcount 를 1 감소시킨다.
	// readcount 가 0 이면 읽고 있는 reader 가 없으므로 writer 가 쓸 수 있도록 
	// wrt 뮤텍스를 unlock 한다.
	// ---
	pthread_mutex_lock(&mutex);
	readcount--;
	if(readcount == 0)
	{
		pthread_mutex_unlock(&wrt);
	}
	
	pthread_mutex_unlock(&mutex);

	return NULL;
}

void *pthread_writer(void *thread_num) {
	int writer_num = *(int *)thread_num;
	free((int *)thread_num);

	sleeping();

	msg_waiting(TYPE_WRITER, writer_num);
	
	// ---
	// 공유변수 shared_val 에 랜덤값을 쓰기위해 wrt 뮤텍스에 lock을 잡는다.
	// 다른 writer 또는 reader 가 잡고 있을경우 unlock 될때 까지 쓰레드 동작이 뭠춘다.
	// lock 을 획득 하면 shared_val 에 랜덤값을 쓴다.
	// lock 을 잡았기 때문에 다른 쓰레드에 의해 동시에 읽거나 쓰여질수 없어 안전하다.
	// ---
	pthread_mutex_lock(&wrt);

	shared_val = rand();

	msg_complete(TYPE_WRITER, writer_num, shared_val);

	// ---
	// 공유변수 사용이 끝났으므로 wrt 뮤텍스를 unlock 하여 다른 쓰레드가 사용할 수 있게 한다.
	// ---
	pthread_mutex_unlock(&wrt);

	return NULL;
}

void init() {
	struct timespec current_time;
	
	// Mutex initializaion
	if(pthread_mutex_init(&mutex, NULL)) {
		printf("mutex init error!\n");
		exit(1);
	}
	if(pthread_mutex_init(&wrt, NULL)) {
		printf("mutex init error!\n");
		exit(1);
	}

	// Random seed initializaion
	clock_gettime(CLOCK_REALTIME, &current_time);
	srand(current_time.tv_nsec);

	// Shared value initializaion
	shared_val = 0;
}

// Being slept during random for randomly selected time (< 1s)
void sleeping() {
	sleep(1 + rand() % 2);
}

void msg_waiting(int thread_type, int thread_num) {
	if(thread_type == TYPE_READER) 
		printf("[Reader %02d] --------------------- Waiting\n", thread_num);

	else
		printf("[Writer %02d] --------------------- Waiting\n", thread_num);
}

void msg_num_of_readers(int thread_num, int num_of_readers) {
	printf("[Reader %02d] --------------------- # of readers : %d\n",
			thread_num, num_of_readers);
}

void msg_complete(int thread_type, int thread_num, int written_data) {
	if(thread_type == TYPE_READER)
		printf("[Reader %02d] Read    : %d\n", thread_num, written_data);

	else
		printf("[Writer %02d] Written : %d\n", thread_num, written_data);
}

