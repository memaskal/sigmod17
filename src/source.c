#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
// NOTE: do we need type control?
#include <stdint.h>

#ifdef _OPENMP
#include <omp.h>
#endif

// Debugging defines
#define NDEBUG
#include <assert.h>

// PARALLEL CONFIGURATION
//#define PARALLEL_SORT
#define NUM_THREADS 3
#define PARALLEL_CHUNK_SIZE 750


#ifndef NDEBUG
#define debug_print(...)  do{ fprintf(stderr, __VA_ARGS__); }while(0)
#else
#define debug_print(...)  do{ }while(0)
	#ifdef _MSC_VER /* VS PORTABILITY. */
	#define getline vs_getline

	int vs_getline(char** line, size_t* len, FILE* f) {
		fgets(*line, *len, f);
		return strlen(*line);
	}


	#endif // _MSC_VER
#endif // NDEBUG


#pragma GCC diagnostic ignored "-Wchar-subscripts" 	// !! DANGEROUS !!
// TODO: fix all chars to unsigned chars





#define MAX_VAL SIZE_MAX
#define MAX_NODES 100000
#define NODE_ARRAY_SIZE 256
#define NODE_ARRAY_OFFSET ' '	 //character 0 for in node array

typedef struct NODE {
	int word_ending;			// NOTE: type for bool values
	int depth;
	unsigned int id;			// offset in the node arrays
	struct NODE** children;
} NODE;


typedef struct N_GRAM {
	size_t start;
	size_t end;
} N_GRAM;


// Static for now. NOTE: need to test big sized arrays for static vs dynamic
NODE* node_arrays[MAX_NODES][NODE_ARRAY_SIZE] = { };
N_GRAM search_state[MAX_NODES];

unsigned int results_list[MAX_NODES]; // max nodes
size_t results_found = 0; // free spot 

size_t next_empty_array = 0; //used by get_new_node

// only for debuging
int nodes_freed = 0;
int iterator_counter = 0;


NODE* get_new_node() {
	assert(next_empty_array < MAX_NODES);

	NODE* node = (NODE *)malloc(sizeof(NODE));

	node->word_ending = 0;
	node->id = next_empty_array;

	// TODO: next empty array Increment should be an atomic
	node->children = node_arrays[next_empty_array++];
	// TODO : benchmark with calloc
	//node->children = (NODE **)calloc(NODE_ARRAY_SIZE, sizeof(NODE*));
	return node;
}


void free_node(NODE* root) {
	assert(root);
	int i;
	for (i = 0; i < NODE_ARRAY_SIZE; ++i) {
		if (root->children[i]) {
			free_node(root->children[i]);
		}
	}
	free(root);
	++nodes_freed;
}


void add_word(NODE* root, char* word_input) {
	
	NODE* current_node = root;
	char *word = word_input;

	assert(root);
	assert(word);

	while (*word != '\0') {
		assert(*word >= NODE_ARRAY_OFFSET);

		if (!current_node->children[*word]) {
			current_node->children[*word] = get_new_node();
			current_node->children[*word]->depth = current_node->depth + 1;
		}

		current_node = current_node->children[*word];
		++word;
	}
	assert(current_node);
	current_node->word_ending = 1;
}


// NOTE: Nodes stay inside for now. Depending on the tests this could be faster / slower
// Current implementation is faster the less searches there are.
int remove_word(NODE* root, char* word) {

	NODE* current_node = root;

	assert(root);
	assert(word);

	while (*word != '\0' && current_node != NULL) {
		if (current_node->children[*word] == NULL) {
			return 0;
		}
		current_node = current_node->children[*word];
		++word;
	}

	current_node->word_ending = 0;
	return 1;
}

void search_from(NODE* root, const char* search, const size_t start) {

	NODE* current_node = root;

	assert(root);
	assert(search);

	const char *str_start = search;

	while (*search != '\0' && current_node != NULL) {

		if (current_node->word_ending == 1 && (*search == ' ' || *search == '\0')) {
			#pragma omp critical
			if (start < search_state[current_node->id].start) {

				if (search_state[current_node->id].start == MAX_VAL) {
					results_list[results_found++] = current_node->id;
				}

				search_state[current_node->id].start = start;
				search_state[current_node->id].end = search - str_start;
				// list will be used to find the results and zero the search_state table
			}
		}
		current_node = current_node->children[*(unsigned char *)search];
		++search;
	}

	if (current_node != NULL && *search == '\0' && current_node->word_ending == 1) {

		#pragma omp critical
		if (start < search_state[current_node->id].start) {
			if (search_state[current_node->id].start == MAX_VAL) {
				results_list[results_found++] = current_node->id;
			}
			search_state[current_node->id].start = start;
			search_state[current_node->id].end = search - str_start;
		}
	}
}

int comparator(const N_GRAM* left, const N_GRAM* right) {
	if (left->start != right->start) {
		return (left->start > right->start) ? 1 : -1;
	}
	return (left->end > right->end) ? 1 : -1;
}

void swap(N_GRAM* left, N_GRAM* right) {
	static N_GRAM temp;
	temp.start = left->start;
	temp.end = left->end;
	
	left->start = right->start;
	left->end = right->end;

	right->start = temp.start;
	right->end = temp.end;
}

int search_implementation(NODE* root, const char* search) {

	// This should only be run by master thread
	size_t i;
	N_GRAM found;
	const char* n_start;
	const char* n_end;

	// set to zero for the new search
	results_found = 0;
	
	search_from(root, search, 0);
	n_start = search;
	size_t search_len = strlen(search);

	#pragma omp parallel shared(search) private(i) num_threads(NUM_THREADS)
	{
		// NOTE: n_start > search, no need for pntr_diff types
		#pragma omp for schedule(dynamic, PARALLEL_CHUNK_SIZE) nowait
		for (i=1; i<search_len; ++i) {
			if (search[i] == ' ') {
				search_from(root, search + i + 1, i + 1);
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
	

	// Serial insertion sort
#ifndef PARALLEL_SORT
	assert(results_found > 0);
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

	NODE *trie = get_new_node();
	trie->depth = 0;

	//maybe larger for fiewer reallocations
	size_t len = 1024;
	char *line = (char*)malloc(len * sizeof(char));
	assert(line);

	int input_len;
	size_t words_added = 0;

	// while there is input read it
	while ((input_len = getline(&line, &len, stdin)) > 2 ||
		(line[0] != 'S')) {

		// add the \0 char replacing \n 
		line[input_len - 1] = '\0';
		add_word(trie, line);
		words_added++;
	}
	debug_print("Words added: %zu. Total nodes in trie: %zu\n", words_added, next_empty_array);

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

	//#pragma omp parallel
	{
		//#pragma omp master 
		{

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
					search_implementation(trie, line);
					break;
				case 'A':
					add_word(trie, line);
					break;
				case 'D':
					remove_word(trie, line);
					break;
				case 'F':
					terminate = 1;
					break;
				}
			}
		}
	}
	
	free_node(trie);
	debug_print("Nodes freed: %d\n", nodes_freed);
	free(line);

	return 0;
}

