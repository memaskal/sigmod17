#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <map>

#ifdef _OPENMP
#include <omp.h>
#endif

// Debugging defines
#define NDEBUG
#include <assert.h>

// PARALLEL CONFIGURATION
//#define PARALLEL_SORT
#define NUM_THREADS 4
#define PARALLEL_CHUNK_SIZE 768


// ARRAY AND MEMORY CONFIGURATION
//#define ARRAY_NODES 	1500000 // The bigger the better probably
// Run atleast 1 time without NDEBUG if you make changes to this.

//#define ARRAY_MAX_DEPTH	15 // Depth bigger than this only gets maps
// Use small value if ARRAY_NODES is small


#define MAX_NODES			400000
#define MAX_USED_CHAR   	256
#define MAX_ARRAY_NODES 	100000



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


struct NODE_AR {
	char word_ending;			// NOTE: type for bool values
	unsigned int ar_children[MAX_USED_CHAR];
};
struct NODE_MAP {
	char word_ending;
	std::map<unsigned char, unsigned int> map_children;
};


struct N_GRAM {
	unsigned int start;
	unsigned int end;
};

NODE_AR* nodes_ar;

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


inline void init_node_ar(NODE_AR* node) {

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
	for (i = 0; i < 4; ++i) {
		debug_print("Allocating %zuMB in %d\n", ((size_t)MAX_ARRAY_NODES * sizeof(NODE) + (size_t)MAX_NODES * sizeof(N_GRAM))/ 1000000, 4-i);
		sleep(1);
	}
	#endif


	nodes_ar = (NODE_AR*) malloc(MAX_ARRAY_NODES*sizeof(NODE_AR));
	assert(nodes_ar);

	for (i = 0; i < MAX_ARRAY_NODES; ++i) {
		init_node_ar(&nodes_ar[i]);
	}
}

inline char get_ending(unsigned int index) {
	return nodes_ar[index].word_ending;
}

inline void set_ending(unsigned int index, char is_ending) {
	nodes_ar[index].word_ending = is_ending;
}

inline unsigned int get_child(unsigned int index, unsigned char c) {
	return nodes_ar[index].ar_children[c];
}


inline unsigned int get_create_child(unsigned int index, unsigned char c) {

	unsigned int *child = &nodes_ar[index].ar_children[c];

	if (*child == 0) {
		*child = next_node_child++;
	}
	return *child;
}


void add_word(const char* substr) {

	unsigned int node_index = 1;
	assert(substr);

	while (*substr != '\0') {
		node_index = get_create_child(node_index, *substr);
		++substr;
		debug_only(total_len_add++);
	}

	set_ending(node_index, 1);
}


// NOTE: Nodes stay inside for now. Depending on the tests this could be faster / slower
// Current implementation is faster the less searches there are.
void remove_word(const char* substr) {

	unsigned int node_index = 1;
	assert(substr);

	while (*substr != '\0') {
		node_index = get_child(node_index, *substr);
		if (node_index == 0) {
			return;
		}
		++substr;
		debug_only(total_len_delete++);
	}

	set_ending(node_index, 0);
}


void search_from(const char* search, const size_t start) {

	unsigned int node_index = 1;

	assert(search);
	debug_only(total_search++);

	const char *str_start = search;

	while (*search != '\0' && node_index != 0) {

		if (get_ending(node_index) == 1 && (*search == ' ' || *search == '\0')) {

			#pragma omp critical
			if (start < search_state[node_index].start) {

				if (search_state[node_index].start == MAX_VAL) {
					results_list[results_found++] = node_index;
				}

				search_state[node_index].start = start;
				search_state[node_index].end = search - str_start;
				// list will be used to find the results and zero the search_state table
			}
		}

		node_index = get_child(node_index, *search);
		++search;
	}

	if (node_index != 0 && *search == '\0' && get_ending(node_index) == 1) {

		#pragma omp critical
		if (start < search_state[node_index].start) {
			if (search_state[node_index].start == MAX_VAL) {
				results_list[results_found++] = node_index;
			}
			search_state[node_index].start = start;
			search_state[node_index].end = search - str_start;
		}
	}
}


inline int comparator(const N_GRAM* left, const N_GRAM* right) {
	if (left->start != right->start) {
		return (left->start > right->start) ? 1 : -1;
	}
	return (left->end > right->end) ? 1 : -1;
}


int search_implementation(const char* search, const size_t search_len) {

	// This should only be run by master thread
	size_t i;

	// set to zero for the new search
	results_found = 0;

	search_from(search, 0);
	debug_only(total_len_query+=search_len);

	#pragma omp parallel num_threads(NUM_THREADS)
	{
		// NOTE: n_start > search, no need for pntr_diff types
		#pragma omp for schedule(dynamic, PARALLEL_CHUNK_SIZE)
		for (i = 1; i < search_len; ++i) {
			if (search[i] == ' ') {
				search_from(search + i + 1, i + 1);
			}
		}
	} // end of pragma omp parallel

	if (results_found == 0) {
		printf("-1\n");
		//	debug_print("\n");
		return 0;
	}

	// Serial insertion sort
	int j;
	int temp;

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
        fwrite(&search[found->start], 1, found->end, stdout);

        // zero it for the next search
        found->start = MAX_VAL;

        printf("|");
    }

    found = &search_state[results_list[i]];
    fwrite(&search[found->start], 1, found->end, stdout);

    // zero it for the next search
    found->start = MAX_VAL;

    printf("\n");
	debug_only(total_results+=results_found);
	return 0;
}



int main() {

	init_arrays();

	//maybe larger for fiewer reallocations
	size_t len = 30000;
	char *line = (char*)malloc(len * sizeof(char));
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
	sleep(3);
	debug_print("Finished sleeping.\n");

	size_t i;
	for (i = 0; i < MAX_NODES; ++i) {
		search_state[i].start = MAX_VAL;
	}

	char action;
	int action_count = 0;
	int queries_count = 0;

	// totaly ready to start
	printf("R\n");
	// TODO: find faster way to force fflush
	fflush(stdout);

	while (1) {

		if ((action = getchar()) == EOF) {
			break;
		}
		else if (action == 'F') {
			fflush(stdout);
			getchar();
			continue;
		}

		// read junk space
		getchar();

		// TDDO: implement fast input
		// Read the rest of input
		input_len = getline(&line, &len, stdin);
		line[input_len - 1] = '\0';
		//action = line[0];
		//if (action_count % 10 == 0) debug_print("Actions: %d\n", action_count);
		action_count++;

		switch (action) {
		case 'Q':
			debug_only(total_query++);
			queries_count++;
			search_implementation(line, input_len - 1);
			break;
		case 'A':
			debug_only(total_add++);
			add_word(line);
			break;
		case 'D':
			debug_only(total_delete++);
			remove_word(line);
			break;
		}
	}

	debug_print("Arrays used: %d\nArrays missed: %d\n", next_node_child, missed_arrays);
	debug_print("Total adds: %d : %d\nTotal deletes: %d: %d\nTotal queries: %d : %d\nTotal searches run: %d & Total Results: %d\n",
			total_add, total_len_add, total_delete, total_len_delete, total_query, total_len_query, total_search, total_results); 

	free(nodes_ar);
	free(line);
	return 0;
}


