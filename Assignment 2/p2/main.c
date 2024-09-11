#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <semaphore.h>
#include "queue.h"

#define NUMQUEUES 2
#define ECON 0
#define BUSN 1
#define NUMCLERKS 5
#define MAX_NUM_C 50

// Initialize global vars and mutexes + convars + semaphore

// simulation waiting times
struct timeval sim_start_time;
double total_wait_time[NUMQUEUES] = {0, 0};
pthread_mutex_t wait_time_mutex;

// to synchronize simulation start
int start_ready = 0;
pthread_mutex_t start_synch_mutex;
pthread_cond_t start_synch_convar;

// customer queues
int customer_queue_lens[NUMQUEUES] = {0, 0};
customer* customer_queues[NUMQUEUES] = {NULL, NULL};

// pthread_mutex_t queue_len_mutex;
pthread_mutex_t queues_mutex;

// Convar + mutex for clerks
pthread_mutex_t clerks_mutex;
pthread_cond_t clerks_convar;
sem_t clerks_sem;

// Queue to hold clerks
clerk* curr_clerk = NULL;
pthread_mutex_t clerk_id_mutex;

// number of customers
int num_customers[NUMQUEUES] = {0, 0};
// thread ids - for joining at end
pthread_t thread_ids[MAX_NUM_C];

/* From posted sample code - gets current simulated time */
double get_current_sim_time() {	
	struct timeval cur_time;
	double cur_secs, init_secs;
	
	//pthread_mutex_lock(&start_time_mtex); you may need a lock here
	init_secs = (sim_start_time.tv_sec + (double) sim_start_time.tv_usec / 1000000);
	//pthread_mutex_unlock(&start_time_mtex);
	
	gettimeofday(&cur_time, NULL);
	cur_secs = (cur_time.tv_sec + (double) cur_time.tv_usec / 1000000);
	
	return cur_secs - init_secs;
}

/* Checks if the customer meets all required conditions to leave the queue */
int check_if_ready(int c_type, int id) {
    int cur_free_clerks;
    sem_getvalue(&clerks_sem, &cur_free_clerks);
    if(cur_free_clerks && check_front(customer_queues[c_type]) == id) {
        // is head of queue
        if(c_type == BUSN) {
            return 1; // lock clerk & be serviced
        } else {
            if(customer_queue_lens[BUSN] == 0) {
                return 1; // lock clerk & be serviced
            } // otherwise keep waiting
            return 0;
        }
    }
    return 0;
}

/* Function called in each customer thread. Simulates arriving, waiting, being serviced */
void * customer_func(void * customer_info) {
    // In customer thread
    struct customer* c_info = (struct customer *) customer_info;
    // Wait until all customers are ready
    pthread_mutex_lock(&start_synch_mutex);
    while(!start_ready) {
        pthread_cond_wait(&start_synch_convar, &start_synch_mutex);
    }
    pthread_mutex_unlock(&start_synch_mutex);

    // Simulate arrival time
    usleep(c_info->arrival_time * 100000);
    double start_wait_time = get_current_sim_time();
    printf("A customer arrives: customer ID %2d. \n", c_info->id);

    // Enter the queue
    pthread_mutex_lock(&queues_mutex);
    customer_queues[c_info->customer_type] = enqueue(customer_queues[c_info->customer_type], c_info);
    customer_queue_lens[c_info->customer_type] = customer_queue_lens[c_info->customer_type] + 1;
    printf("Customer %d enters queue %d, length of the queue is %d.\n", c_info->id, c_info->customer_type, customer_queue_lens[c_info->customer_type]);
    pthread_mutex_unlock(&queues_mutex);

    // Wait for clerk
    //lock mutex - want to use clerk
    pthread_mutex_lock(&clerks_mutex);
    //loop - while not head of queue and no business c's in queue if econ...
    //wait on the convar
    while(!check_if_ready(c_info->customer_type, c_info->id)) {
        pthread_cond_wait(&clerks_convar, &clerks_mutex);
    }

    pthread_mutex_unlock(&clerks_mutex);

    // Dequeue self from queue
    pthread_mutex_lock(&queues_mutex);
    customer_queues[c_info->customer_type] = dequeue(customer_queues[c_info->customer_type]);
    customer_queue_lens[c_info->customer_type]--;
    // printf("Customer %d leaving queue %d. Queue length: %d\n", c_info->id, c_info->customer_type, customer_queue_lens[c_info->customer_type]);
    pthread_mutex_unlock(&queues_mutex);

    //when conditions met + clerk free
    sem_wait(&clerks_sem);
    
    int clerk_id;
    pthread_mutex_lock(&clerk_id_mutex);
    clerk_id = curr_clerk->id;
    curr_clerk = dequeue_clerk(curr_clerk);
    // curr_clerk = (curr_clerk + 1) % 5;
    pthread_mutex_unlock(&clerk_id_mutex);

    //lock clerk, sleep for service time, unlock clerk
    double end_wait_time = get_current_sim_time();
    printf("Clerk %d starts serving customer %d: start time %.2f.\n", clerk_id, c_info->id, end_wait_time);
    usleep(c_info->service_time * 100000);
    printf("Clerk %d finishes serving customer %d: end time %.2f.\n", clerk_id, c_info->id, end_wait_time + c_info->service_time/10);
    
    pthread_mutex_lock(&clerk_id_mutex);
    // curr_clerk = clerk_id - 1;
    curr_clerk = enqueue_clerk(curr_clerk, clerk_id);
    pthread_mutex_unlock(&clerk_id_mutex);
    sem_post(&clerks_sem);
    pthread_cond_broadcast(&clerks_convar);

    // Add wait time to total
    pthread_mutex_lock(&wait_time_mutex);
    total_wait_time[c_info->customer_type] += (end_wait_time - start_wait_time);
    pthread_mutex_unlock(&wait_time_mutex);

    // Done
    free(c_info);
    pthread_exit(0);
}

/* Reads input file and creates customer threads */
void read_file(char* input_file) {
    FILE *f = fopen(input_file, "r");
    if(f == NULL) {
        fprintf(stderr, "Error: Input file does not exist.\n");
        exit(-1);
    }

    int total_customers;

    fscanf(f, "%d", &total_customers);
    if(total_customers < 0){
        fprintf(stderr, "Error: Can't have negative number of customers.\n");
        exit(-1);
    }
    pthread_t c_id; 
    int c_type, arr_time, serv_time;

    for(int i = 0; i < total_customers; i++) {
        fscanf(f, "%ld:%d,%d,%d", &c_id, &c_type, &arr_time, &serv_time);
        if(arr_time < 0 || serv_time < 0) {
            fprintf(stderr, "Error: Customers can't have negative arrival or service times.\n");
            exit(-1);
        }
        // Make thread for customer
        customer* curr;
        curr = (customer*) malloc(sizeof(customer));
        if(curr == NULL) {
            fprintf(stderr, "Error: malloc of size %ld failed. \n", sizeof(customer));
            exit(-1);
        }
        curr->id = c_id;
        curr->customer_type = c_type;
        curr->arrival_time = arr_time;
        curr->service_time = serv_time;
        if(pthread_create(&c_id, NULL, &customer_func, (void *)curr) != 0) {
            fprintf(stderr, "Error: Thread %ld could not be created.\n", c_id);
            exit(-1);
        }
        num_customers[c_type]++;
        thread_ids[i] = c_id;
    }

    fclose(f);
}

int main(int argc, char* argv[]) {
    // Initialization
    if(argc != 2) {
        fprintf(stderr, "Error: Please run with one input file.\n");
        exit(-1);
    }
    if(pthread_mutex_init(&queues_mutex, NULL) != 0) {
        fprintf(stderr, "Error: Queues mutex could not be created.\n");
        exit(-1);
    }
    if(pthread_mutex_init(&clerks_mutex, NULL) != 0) {
        fprintf(stderr, "Error: Clerks mutex could not be created.\n");
        exit(-1);
    }
    if(pthread_cond_init(&clerks_convar, NULL) != 0) {
        fprintf(stderr, "Error: Clerks convar could not be created.\n");
        exit(-1);
    }
    if(sem_init(&clerks_sem, 0, NUMCLERKS) != 0) {
        fprintf(stderr, "Error: Semaphore could not be created.\n");
        exit(-1);
    }
    if(pthread_mutex_init(&clerk_id_mutex, NULL) != 0) {
        fprintf(stderr, "Error: Clerk id mutex could not be created.\n");
        exit(-1);
    }
    if(pthread_mutex_init(&start_synch_mutex, NULL) != 0) {
        fprintf(stderr, "Error: Start mutex could not be created.\n");
        exit(-1);
    }
    if(pthread_cond_init(&start_synch_convar, NULL) != 0) {
        fprintf(stderr, "Error: Start convar could not be created.\n");
        exit(-1);
    }
    if(pthread_mutex_init(&wait_time_mutex, NULL) != 0) {
        fprintf(stderr, "Error: Waiting time mutex could not be created.\n");
        exit(-1);
    }

    read_file(argv[1]);
    int c_total = num_customers[BUSN] + num_customers[ECON];

    // Initialize clerk queue
    pthread_mutex_lock(&clerk_id_mutex);
    for(int j = 1; j <= NUMCLERKS; j++) {
        curr_clerk = enqueue_clerk(curr_clerk, j);
    }
    pthread_mutex_unlock(&clerk_id_mutex);

    // Now all threads have been created
    // so they can start at the same time
    start_ready = 1;
    gettimeofday(&sim_start_time, NULL);
    pthread_cond_broadcast(&start_synch_convar);

    // Join all threads
    for(int i = 1; i <= c_total; i++) {
        pthread_join(thread_ids[i], NULL);
    }

    // Free all clerks
    for(int k = 0; k < NUMCLERKS; k++) {
        curr_clerk = dequeue_clerk(curr_clerk);
    }

    // Calculate avg waiting times
    double all_customer_wait_time = total_wait_time[BUSN] + total_wait_time[ECON];
    printf("\n-----\n");
    printf("\nWe served %d customers in total!\n", c_total);
    printf("Customers spent a total of %.2f seconds waiting.\n", all_customer_wait_time);
    printf("The average waiting time for all customers in the system is: %.2f seconds. \n", all_customer_wait_time/c_total);

    printf("\nWe served %d business-class customers!\n", num_customers[BUSN]);
    printf("Business customers spent a total of %.2f seconds waiting.\n", total_wait_time[BUSN]);
    printf("The average waiting time for all business-class customers in the system is: %.2f seconds. \n", total_wait_time[BUSN]/num_customers[BUSN]);

    printf("\nWe served %d economy-class customers!\n", num_customers[ECON]);
    printf("Economy customers spent a total of %.2f seconds waiting.\n", total_wait_time[ECON]);
    printf("The average waiting time for all economy-class customers in the system is: %.2f seconds. \n", total_wait_time[ECON]/num_customers[ECON]);

    // Destroy all mutexes and convars
    if(pthread_mutex_destroy(&queues_mutex) != 0) {
        fprintf(stderr, "Error: Queues mutex could not be destroyed.\n");
    }
    if(pthread_mutex_destroy(&clerks_mutex) != 0) {
        fprintf(stderr, "Error: Clerks mutex could not be destroyed.\n");
    }
    if(pthread_mutex_destroy(&clerk_id_mutex) != 0) {
        fprintf(stderr, "Error: Clerk ID mutex could not be destroyed.\n");
    }
    if(pthread_mutex_destroy(&wait_time_mutex) != 0) {
        fprintf(stderr, "Error: Wait time mutex could not be destroyed.\n");
    }
    if(pthread_mutex_destroy(&start_synch_mutex) != 0) {
        fprintf(stderr, "Error: Start mutex could not be destroyed.\n");
    }
    if(pthread_cond_destroy(&clerks_convar) != 0) {
        fprintf(stderr, "Error: Clers convar could not be destroyed.\n");
    }
    if(pthread_cond_destroy(&start_synch_convar) != 0) {
        fprintf(stderr, "Error: Start convar could not be destroyed.\n");
    }

    return 0;
}