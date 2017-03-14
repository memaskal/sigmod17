#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>
// for the semaphores
#include <sys/types.h> 
#include <sys/ipc.h>
#include <sys/sem.h> 

// Debugging defines
//#define NDEBUG
#include <assert.h>

// PARALLEL CONFIGURATION
//#define PARALLEL_SORT
#define NUM_THREADS 4
#define PARALLEL_CHUNK_SIZE 728


// ARRAY AND MEMORY CONFIGURATION
//#define ARRAY_NODES 	1500000 // The bigger the better probably
// Run atleast 1 time without NDEBUG if you make changes to this.

//#define ARRAY_MAX_DEPTH	15 // Depth bigger than this only gets maps
// Use small value if ARRAY_NODES is small


#define MAX_NODES	500000
#define MAX_USED_CHAR   256


// define max value
#define MAX_VAL UINT_MAX

#ifndef NDEBUG
#define debug_print(...)  do{ fprintf(stderr, __VA_ARGS__); }while(0)
#define debug_only(...) do{ __VA_ARGS__; }while(0)
#else
#define debug_print(...)  do{ }while(0)
#define debug_only(...) do{ }while(0)
	#ifdef _MSC_VER /* VS PORTABILITY. */
	/*#define getline vs_getline

	int vs_getline(char** line, size_t* len, FILE* f) {
		fgets(*line, *len, f);
		return strlen(*line);
	}
*/
	#endif // _MSC_VER
#endif // NDEBUG


#pragma GCC diagnostic ignored "-Wchar-subscripts" // TODO: fix all chars to unsigned chars. Need to make getline accept unsigned char


struct NODE {
	char word_ending;			// NOTE: type for bool values
	unsigned int ar_children[MAX_USED_CHAR];
};


struct N_GRAM {
	unsigned int start;
	unsigned int end;
};

NODE* nodes;

int next_node_child = 2; // always start from position 2
int missed_arrays = 0;

N_GRAM search_state[MAX_NODES];
unsigned int results_list[MAX_NODES]; // max nodes
size_t results_found = 0; // free spot 


#ifndef NDEBUG
int total_len_add = 0;
int total_len_delete = 0;
int total_len_query = 0;
int total_add = 0;
int total_delete = 0;
int total_query = 0;
int total_search = 0;
int total_results = 0;
#endif



#define UP 0
#define DOWN 1
#define SINGLE 0
#define ALL 1
#define QUEUE_SIZE 1000

size_t len;
char *line;

// set by input handler when input has finished
int terminate_all = 0;
pthread_mutex_t results_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
	char action;
	size_t batch_index;
} JOB;


typedef struct {
	JOB jobs[QUEUE_SIZE];	// the queue containing the jobs
	int head;
	int tail;
	pthread_mutex_t mutex;
	pthread_cond_t can_produce;
	pthread_cond_t can_consume;
} QUEUE;


QUEUE queue = {
	{},
	-1,
	-1,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_COND_INITIALIZER,
	PTHREAD_COND_INITIALIZER
};


int isFull(QUEUE q) {
	return ((q.tail - QUEUE_SIZE) == q.head);
}

int isEmpty(QUEUE q) {
	return (q.head == q.tail);
}

int sem_id;
struct sembuf sem_ops[][2] = {
	{	// Operations for query
		{0,  1,  0}, 
		{0, -1,  0}
	},
	{	// Operations for sorting
		{0,  NUM_THREADS,  0},
		{0, -NUM_THREADS,  0}
	}		
};


void new_job(JOB job) {

	pthread_mutex_lock(&queue.mutex);
	
	while (isFull(queue)) {
		pthread_cond_wait(&queue.can_produce, &queue.mutex);
	}
	
	//debug_print("Description %c\ntext: %s\n", job.action, &line[job.batch_index]);	
	
	queue.jobs[++queue.tail % QUEUE_SIZE] = job;
	
	pthread_cond_signal(&queue.can_consume);
	pthread_mutex_unlock(&queue.mutex);
}


JOB get_job() {
	
	JOB job;
		
	pthread_mutex_lock(&queue.mutex);
	
	while (isEmpty(queue)) {
		pthread_cond_wait(&queue.can_consume, &queue.mutex);
	}
	
	job = queue.jobs[++queue.head % QUEUE_SIZE];
	
	pthread_cond_signal(&queue.can_produce);
	pthread_mutex_unlock(&queue.mutex);
	
	return job;
}


inline void init_node(NODE* node) {

	unsigned int i;
	unsigned int *children = node->ar_children;

	for (i = 0; i < MAX_USED_CHAR; ++i) {
		children[i] = 0;
	}
	node->word_ending = 0;
}


void init_arrays() {

	unsigned int i;

	#ifndef NDEBUG
	for (i = 0; i < 2; ++i) {
		debug_print("Allocating %zuMB in %d\n", ((size_t)MAX_NODES * sizeof(NODE) + (size_t)MAX_NODES * sizeof(N_GRAM))/ 1000000, 2-i);
		sleep(1);
	}
	#endif


	nodes = (NODE*) malloc(MAX_NODES*sizeof(NODE));
	assert(nodes);

	for (i = 0; i < MAX_NODES; ++i) {
		init_node(&nodes[i]);
	}
}


inline unsigned int get_child(NODE* root, unsigned char c) {
	return root->ar_children[c];
}


inline unsigned int get_create_child(NODE* root, unsigned char c) {

	unsigned int *child = &root->ar_children[c];

	if (*child == 0) {
		*child = next_node_child++;
	}
	return *child;
}


void add_word(const char* substr) {

	unsigned int node_index = 1;
	assert(substr);

	while (*substr != '\0') {
		node_index = get_create_child(&nodes[node_index], *substr);
		++substr;
		debug_only(total_len_add++);
	}

	nodes[node_index].word_ending = 1;
}


// NOTE: Nodes stay inside for now. Depending on the tests this could be faster / slower
// Current implementation is faster the less searches there are.
void remove_word(const char* substr) {

	unsigned int node_index = 1;
	assert(substr);

	while (*substr != '\0') {
		node_index = get_child(&nodes[node_index], *substr);
		if (node_index == 0) {
			return;
		}
		++substr;
		debug_only(total_len_delete++);
	}

	nodes[node_index].word_ending = 0;
}


void search_from(const char* search, const size_t start) {

	unsigned int node_index = 1;

	assert(search);
	debug_only(total_search++);
	
	//debug_print("search for : %s\n", search);

	const char *str_start = search;

	while (*search != '\0' && node_index != 0) {
		//	debug_print("Word ending %d at index: %d\n", nodes[node_index].word_ending, node_index);
		if (nodes[node_index].word_ending == 1 && (*search == ' ' || *search == '\0')) {
			
			pthread_mutex_lock(&results_mutex);
			if (start < search_state[node_index].start) {

				if (search_state[node_index].start == MAX_VAL) {
					results_list[results_found++] = node_index;
				}

				search_state[node_index].start = start;
				search_state[node_index].end = search - str_start;
				// list will be used to find the results and zero the search_state table
			}
			pthread_mutex_unlock(&results_mutex);
		}

		node_index = get_child(&nodes[node_index], *search);
		++search;
	}

	if (node_index != 0 && *search == '\0' && nodes[node_index].word_ending == 1) {

		pthread_mutex_lock(&results_mutex);
		if (start < search_state[node_index].start) {
			if (search_state[node_index].start == MAX_VAL) {
				results_list[results_found++] = node_index;
			}
			search_state[node_index].start = start;
			search_state[node_index].end = search - str_start;
		}
		pthread_mutex_unlock(&results_mutex);
	}
}


void search(char *search, size_t start) {

	// fisrt search allways start at character
	search_from(search++, start++);
	
	while(*search) {
		if (*search == ' ') {
			search_from(search + 1, start + 1);
		}
		++search;
		++start;
	}
}


inline int comparator(const N_GRAM* left, const N_GRAM* right) {
	if (left->start != right->start) {
		return (left->start > right->start) ? 1 : -1;
	}
	return (left->end > right->end) ? 1 : -1;
}


void sort_results() {

	if (results_found == 0) {
		printf("-1\n");
		//	debug_print("\n");
		return;
	}

	// Serial insertion sort
	int j;
	int temp;
	size_t i;

	for (i = 1; i < results_found; ++i) {
		j = i - 1;
		temp = results_list[i];
		while (j>=0 && comparator(&search_state[temp], &search_state[results_list[j]])<0) {

			results_list[j+1] = results_list[j];
			--j;
		}
		results_list[j+1] = temp;
	}


	N_GRAM *found;

    for (i = 0; i < results_found - 1; ++i) {

        found = &search_state[results_list[i]];
        fwrite(&line[found->start], 1, found->end, stdout);

        // zero it for the next search
        found->start = MAX_VAL;

        printf("|");
    }

    found = &search_state[results_list[i]];
    fwrite(&line[found->start], 1, found->end, stdout);

    // zero it for the next search
    found->start = MAX_VAL;

    printf("\n");
    
    results_found = 0;    
}


void *job_handler(void *) {

	JOB job;
	//size_t thread_id = *(size_t *)id;
	
	// while they are available jobs and 
	// input haven't finished yet
	while (!terminate_all || !isEmpty(queue)) {
		
		// thread requests of a new job
		job = get_job();	
		
		switch (job.action) {
		case 'Q':
			search(&line[job.batch_index], job.batch_index);
			break;
		case 'A':
			add_word(&line[job.batch_index]);
			break;
		case 'D':
			remove_word(&line[job.batch_index]);
			break;
		case 'S':
			sort_results();
			fflush(stdout);
			break;
		}
	}	
	debug_print("thread exiting");
	return NULL;
}


void input_handler() {
	
	int i, jobs;
	ssize_t read;
	size_t length;
	char *task, *val;
	const char *delim = "\n";
	
	while (1) {
		
		// read a whole batch into memory
		if ((read = getdelim(&line, &len, 'F', stdin)) != -1) {
			
			// remove F
			line[read - 1] = '\0';
			
			// every new line is a new job
			task = strtok(line, delim);
			
			while (task != NULL) {
			
				if (task[0] == 'Q') {
				
					// split query to small jobs to be handled 
					// by single threads
					val = &task[2];
					length = strlen(task);
					
					new_job((JOB) {'Q', (size_t)(val - line)});	
					
					jobs = length / PARALLEL_CHUNK_SIZE + 1;
									
					for (i = 0; i < jobs; ++i) {
						
						val += PARALLEL_CHUNK_SIZE;
						while (*val && *val != ' ') ++val;
						if (!*val) break;
						
						// +1 to start from next space
						new_job((JOB) {'Q', (size_t)(val - line) + 1});		
					}	
					
					// sort the results task
					new_job((JOB) {'S', 0});
																	
				}
				else {
					// line - task => get the beggining of action
					// + 2 offset to remove the action, space chars
					new_job((JOB) {task[0], (size_t)(task - line) + 2});			
				}
				task = strtok(NULL, delim);
			}						
		}
		else {
			// end of file or error
			terminate_all = 1;
			return;			
		}
	}
} 



int main() {

	// init semaphore
	if ((sem_id = semget(IPC_PRIVATE, 1, 0600)) == -1) {	
		perror("semget");
		exit(1);
	}

	init_arrays();

	//maybe larger for fiewer reallocations
	len = 30000;
	line = (char*)malloc(len * sizeof(char));
	assert(line);

	int input_len;
	size_t words_added = 0;

	// while there is input read it
	while ((input_len = getline(&line, &len, stdin)) > 2 ||
		(line[0] != 'S')) {

		// add the \0 char replacing \n 
		line[input_len - 1] = '\0';
		add_word(line);
		++words_added;
	}
	debug_print("TOTAL: Words added: %zu\n", words_added); 

	// Wait 2 seconds before printing R
	debug_print("Waiting 3 seconds for swap\n");
	//sleep(3);
	debug_print("Finished sleeping.\n");

	size_t i;
	for (i = 0; i < MAX_NODES; ++i) {
		search_state[i].start = MAX_VAL;
	}

	// set semaphore to its max value
	semop(sem_id, &sem_ops[ALL][UP], 1);

	// create all worker threads here
	pthread_t workers[NUM_THREADS];
	
	for (i = 0; i < NUM_THREADS; ++i) {
		// create worker threads
		pthread_create(&workers[i], NULL, job_handler, (void*)i);
	}
		
	// totaly ready to start
	printf("R\n");
	// TODO: find faster way to force fflush
	fflush(stdout);

	// Run my main thread
	input_handler();
	
	
	for (i = 0; i < NUM_THREADS; ++i) {
		// wait for all threads to finish
		pthread_join(workers[i], NULL);
	}    

	// remove semaphore
	semctl(sem_id, 0, IPC_RMID);
	
	free(nodes);
	free(line);
	return 0;
}


