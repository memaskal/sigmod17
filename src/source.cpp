#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
// NOTE: do we need type control?
#include <limits.h>
#include <stdint.h>
#include <unistd.h>
#include <list>
#include <map>


#ifdef _OPENMP
#include <omp.h>
#endif

// Debugging defines
#define NDEBUG
#include <assert.h>

// PARALLEL CONFIGURATION
//#define PARALLEL_SORT
#define NUM_THREADS 4		// Good value: CPU threads - 1 seems to be the fastest atm
#define PARALLEL_CHUNK_SIZE 750 // Good value: 750 for large test case


// ARRAY AND MEMORY CONFIGURATION
//#define ARRAY_NODES 	1500000 // The bigger the better probably
				// Run atleast 1 time without NDEBUG if you make changes to this.

//#define ARRAY_MAX_DEPTH	15 // Depth bigger than this only gets maps
			  // Use small value if ARRAY_NODES is small



#define MAX_NODES 	7400000
#define MAX_USED_CHAR   240



// define max value
#define MAX_VAL UINT_MAX

#ifndef NDEBUG
#define debug_print(...)  do{ fprintf(stderr, __VA_ARGS__); }while(0)
#else
#define debug_print(...)  do{ }while(0)
	#ifdef _MSC_VER /* VS PORTABILITY. */
	/*#define getline vs_getline

	int vs_getline(char** line, size_t* len, FILE* f) {
		fgets(*line, *len, f);
		return strlen(*line);
	}
*/
	#endif // _MSC_VER
#endif // NDEBUG


#pragma GCC diagnostic ignored "-Wchar-subscripts" 	// !! DANGEROUS !!
// TODO: fix all chars to unsigned chars


// typedef unsigned char KEY_TYPE;

struct NODE {

	char word_ending;			// NOTE: type for bool values
	//unsigned int id;			// offset in the node arrays
	//int ch_type;	// 0 null,
			// 1 NODE* Array[256]
			// 2 map<KEY_TYPE, NODE*>
	unsigned int ar_children[MAX_USED_CHAR];
};


struct N_GRAM {
	unsigned int start;
	unsigned int end;
};

NODE* nodes;

//NODE*** node_child;
int next_node_child = 2;
int missed_arrays = 0;

void init_node(NODE* node) {
	int i;	
	node->word_ending = 0;
	for (i=0; i<MAX_USED_CHAR; ++i) {
		node->ar_children[i] = 0;
	}
}

void init_arrays() {
	int i;

	#ifndef NDEBUG
	for (i=0; i<4; ++i) {
		debug_print("Allocating %zuMB in %d\n", ((size_t)MAX_NODES * sizeof(NODE) + (size_t)MAX_NODES * sizeof(N_GRAM))/ 1000000, 4-i);
		sleep(1);
	}
	#endif


	nodes = (NODE*) malloc(MAX_NODES*sizeof(NODE));

	for (i=0; i<MAX_NODES; ++i) {
		init_node(&nodes[i]);
	}
	#ifndef NDEBUG
	debug_print("Waiting 1 second for good measure\n");
	sleep(1);
	#endif
}

void free_arrays() {
/*	size_t i;

	for (i=0; i<MAX_NODES; ++i) {
		free(node_child[i]);
	}
	free(node_child);
*/
}


unsigned int get_child(NODE* root, unsigned char c) {
	return root->ar_children[c];
}

unsigned int get_create_child(NODE* root, unsigned char c) {
	if (root->ar_children[c] == 0) {
		root->ar_children[c] = next_node_child++;
	}
	return root->ar_children[c];
}




N_GRAM search_state[MAX_NODES];
unsigned int results_list[MAX_NODES]; // max nodes
size_t results_found = 0; // free spot 

void free_node(NODE* current_node) {
	current_node++;
//	assert(current_node);
/*	if (current_node) {
		return;
	}

	size_t i;
	NODE* temp;	

	for(i=0; i<MAX_USED_CHAR; ++i) {
		temp = get_create_child(current_node, i);
		free_node(temp);
	}
	delete current_node;
*/
}

void add_word(char* substr) {
	unsigned int node_index = 1;
	assert(substr);

	while (*substr != '\0') {
		node_index = get_create_child(&nodes[node_index], *substr);
		++substr;
	}

	nodes[node_index].word_ending = 1;
}


// NOTE: Nodes stay inside for now. Depending on the tests this could be faster / slower
// Current implementation is faster the less searches there are.
void remove_word(char* substr) {
	unsigned int node_index = 1;
	assert(substr);

	while (*substr != '\0') {
		node_index = get_child(&nodes[node_index], *substr);
		if (node_index == 0) {
			return;
		}
		++substr;
	}

	nodes[node_index].word_ending = 0;
}

void search_from(const char* search, const size_t start) {

	unsigned int node_index = 1;

	assert(search);

	const char *str_start = search;

	while (*search != '\0' && node_index != 0) {
	//	debug_print("Word ending %d at index: %d\n", nodes[node_index].word_ending, node_index);
		if (nodes[node_index].word_ending == 1 && (*search == ' ' || *search == '\0')) {
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

		node_index = get_child(&nodes[node_index], *search);
		if (node_index == 0) {
			return;
		}
		++search;
	}

	if (node_index != 0 && *search == '\0' && nodes[node_index].word_ending == 1) {

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

int comparator(const N_GRAM* left, const N_GRAM* right) {
	if (left->start != right->start) {
		return (left->start > right->start) ? 1 : -1;
	}
	return (left->end > right->end) ? 1 : -1;
}

int search_implementation(const char* search) {

	// This should only be run by master thread
	size_t i;
	N_GRAM found;
	const char* n_start;
	const char* n_end;

	// set to zero for the new search
	results_found = 0;
	
	search_from(search, 0);
	n_start = search;
	size_t search_len = strlen(search);

	#pragma omp parallel shared(search) private(i) num_threads(NUM_THREADS)
	{
		// NOTE: n_start > search, no need for pntr_diff types
		#pragma omp for schedule(dynamic, PARALLEL_CHUNK_SIZE) nowait
		for (i=1; i<search_len; ++i) {
			if (search[i] == ' ') {
				search_from(search + i + 1, i + 1);
			}
		}

		// wait for all the tasks to finish
		#pragma omp barrier
		#ifdef PARALLEL_SORT
		int threads = omp_get_num_threads();
		int id = omp_get_thread_num();
		size_t start_pos; 
  

   
		for (i = 0; i < results_found; ++i) {
			start_pos = id * 2 + (i % 2);
   
			while (start_pos < results_found - 1) {
				if (comparator(&search_state[results_list[start_pos]], &search_state[results_list[start_pos + 1]]) > 0) {
					//swap results
					unsigned int temp_id = results_list[start_pos];
					results_list[start_pos] = results_list[start_pos + 1];
					results_list[start_pos + 1] = temp_id;   
				} 
				start_pos += threads * 2;
			}
			#pragma omp barrier  
		}
		#endif // PARALLEL_SORT

	} // end of pragma omp parallel
	
	if (results_found == 0) {
		printf("-1\n");
		//	debug_print("\n");
		fflush(stdout);
		return 0;
	}

	// Serial insertion sort
#ifndef PARALLEL_SORT
	int j;
	int temp;

	for (i=1; i<results_found; ++i) {
		temp = results_list[i];
		j = i - 1;
		while (j>=0 && comparator(&search_state[temp], &search_state[results_list[j]])<0) {

			results_list[j+1] = results_list[j];
			--j;
		}
		results_list[j+1] = temp;
	}
#endif // PARALLEL_SORT




	for (i=0; i<results_found-1; ++i) {

		found = search_state[results_list[i]];
		// zero it for the next search
		search_state[results_list[i]].start = MAX_VAL;

		n_start = (search + found.start);
		n_end = (n_start + found.end);

		for (; n_start < n_end; ++n_start) {
			printf("%c", *n_start);
//			debug_print("%c", *n_start);
		}
		printf("|");
//		debug_print("|");
	}


	found = search_state[results_list[i]];
	search_state[results_list[i]].start = MAX_VAL;

	n_start = (search + found.start);
	n_end = (n_start + found.end);

	for (; n_start < n_end; ++n_start) {
		printf("%c", *n_start);
//		debug_print("%c", *n_start);
	}

	printf("\n");
//	debug_print("\n");
	fflush(stdout);
	
	return 0;
}



int main() {

	init_arrays();

	//maybe larger for fiewer reallocations
	size_t len = 300000;
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
	debug_print("TOTAL: Words added: %zu\n", words_added); //. Total nodes in trie: %zu\n", words_added, next_empty_array);

	size_t i;
	for (i = 0; i < MAX_NODES; ++i) {
		search_state[i].start = MAX_VAL;
	}

	char action;
	int terminate = 0;
	int action_count = 0;
	int queries_count = 0;


	// totaly ready to start
	printf("R\n");
	// TODO: find faster way to force fflush
	fflush(stdout);

	while (!terminate) {
		action = getchar();
		// read junk space
		getchar();

		// TDDO: implement fast input
		// Read the rest of input
		input_len = getline(&line, &len, stdin);
		line[input_len - 1] = '\0';
		action_count++;
		switch (action) {
		case 'Q':
			queries_count++;
			search_implementation(line);
			break;
		case 'A':
			add_word(line);
			break;
		case 'D':
			remove_word(line);
			break;
		case 'F':
			terminate = 1;
			break;
		}
	}

	debug_print("Arrays used: %d\nArrays missed: %d\n", next_node_child, missed_arrays);
//	free_node(trie);
	free(line);
	free_arrays();

	return 0;
}


