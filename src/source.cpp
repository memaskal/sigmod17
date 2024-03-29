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
#define NUM_THREADS 10
#define PARALLEL_CHUNK_SIZE 512


#define MAX_NODES        900000000L
#define MAX_USED_CHAR    255
#define MAX_ARRAY_NODES  8000000

#define MAX_RESULTS      10000000

#define MAX_MAP_NODES (MAX_NODES - MAX_ARRAY_NODES)


// define max value
#define MAX_VAL UINT_MAX

#ifndef NDEBUG
#define debug_print(...)  do{ fprintf(stderr, __VA_ARGS__); }while(0)
#define debug_only(...) do{ __VA_ARGS__; }while(0)
#else
#define debug_print(...)  do{ }while(0)
#define debug_only(...) do{ }while(0)
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
NODE_MAP* nodes_map;

int next_node_child = 2; // always start from position 2
int missed_arrays = 0;

N_GRAM* search_state;
unsigned int* results_list; // max nodes
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


void print_report() {
	debug_print("NODE COUNT: %d -- MAPS USED: %d\n", next_node_child, next_node_child - MAX_ARRAY_NODES);
	debug_print("Total adds: %d : %d\nTotal deletes: %d: %d\nTotal queries: %d : %d\nTotal searches run: %d & Total Results: %d\n",
		total_add, total_len_add, total_delete, total_len_delete, total_query, total_len_query, total_search, total_results);

}


inline void init_node_ar(NODE_AR* node) {

	unsigned int i;
	unsigned int *children = node->ar_children;

	for (i = 0; i < MAX_USED_CHAR; ++i) {
		children[i] = 0;
	}
	node->word_ending = 0;
}


inline void init_node_map(NODE_MAP* node) {
	node->word_ending = 0;
	node->map_children = std::map<unsigned char, unsigned int>();
}


// Allocate all of the memory since this does not count as execution time (as per the contest's rules)
// This saves allocation time when it matters.
void init_arrays() {
	//	debug_print("Sizeof NODE_AR: %zu - Sizeof NODE_MAP: %zu\n", sizeof(NODE_AR), sizeof(NODE_MAP));
	unsigned int i;

	search_state = (N_GRAM*)malloc(MAX_NODES * sizeof(N_GRAM));
	results_list = (unsigned int*)malloc(MAX_RESULTS * sizeof(unsigned int));

	nodes_ar = (NODE_AR*)malloc(MAX_ARRAY_NODES * sizeof(NODE_AR));
	assert(nodes_ar);

	for (i = 0; i < MAX_ARRAY_NODES; ++i) {
		init_node_ar(&nodes_ar[i]);
	}

	// init NODE_MAP
	nodes_map = (NODE_MAP*)malloc(MAX_MAP_NODES * sizeof(NODE_MAP));
	for (i = 0; i < MAX_MAP_NODES; ++i) {
		init_node_map(&nodes_map[i]);
	}
}


char get_ending(unsigned int index) {
	if (index < MAX_ARRAY_NODES) {
		return nodes_ar[index].word_ending;
	}
	// TODO: fix optimisation
	index -= MAX_ARRAY_NODES;
	return nodes_map[index].word_ending;
}


void set_ending(unsigned int index, char is_ending) {
	if (index < MAX_ARRAY_NODES) {
		nodes_ar[index].word_ending = is_ending;
		return;
	}
	// TODO: fix optimisation
	index -= MAX_ARRAY_NODES;

	nodes_map[index].word_ending = is_ending;
}


// Gets the child node of the Trie
// It works with both the array part and the map part of the Trie.
unsigned int get_child(unsigned int index, unsigned char c) {
	if (index < MAX_ARRAY_NODES) {
		return nodes_ar[index].ar_children[c];
	}
	// TODO: fix optimisation
	index -= MAX_ARRAY_NODES;
	std::map<unsigned char, unsigned int>::iterator it = nodes_map[index].map_children.find(c);
	if (it != nodes_map[index].map_children.end()) {
		return it->second;
	}
	return 0;
}


// Used in add operation, if it does not find the child node it returns an empty (preallocated) node
// It works with both the array part and the map part of the Trie.
unsigned int get_create_child(unsigned int index, unsigned char c) {
	unsigned int child = get_child(index, c);
	if (child != 0) {
		return child;
	}

	// Create child
	if (index < MAX_ARRAY_NODES) {
		// Parent is array
		nodes_ar[index].ar_children[c] = next_node_child;
		return next_node_child++;
	}

	//Parent is map
	index -= MAX_ARRAY_NODES;
	#ifndef NDEBUG
	if (index >= MAX_MAP_NODES) {
		debug_print("Error. Max nodes are not enough!\n");
		print_report();
		printf("\n"); // to stop harness
		fflush(stdout);
		exit(0);
	}
	#endif
	nodes_map[index].map_children.insert(std::pair<unsigned char, unsigned int>(c, next_node_child));
	return next_node_child++;
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


// NOTE: Nodes stay in the tree for now. Depending on the tests this could be faster / slower
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


// Performs the Trie search from the search query (+ an offset)
// This runs in parallel and stores the results in the result arrays
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


// Used for sorting the results
inline int comparator(const N_GRAM* left, const N_GRAM* right) {
	if (left->start != right->start) {
		return (left->start > right->start) ? 1 : -1;
	}
	return (left->end > right->end) ? 1 : -1;
}


// Resolves a Q: query and stores the results sorted.
int search_implementation(const char* search, const size_t search_len) {

	// This should only be run by master thread
	size_t i;

	// set to zero for the new search
	results_found = 0;

	// This is because search[-1] is technically a ' '
	search_from(search, 0);
	debug_only(total_len_query += search_len);

	#pragma omp parallel num_threads(NUM_THREADS)
	{
		// NOTE: n_start > search, no need for pntr_diff types
		#pragma omp for schedule(dynamic, PARALLEL_CHUNK_SIZE)
		for (i = 1; i < search_len; ++i) {
			// Whenever we find a space we start a new Trie search
			if (search[i] == ' ') {
				search_from(search + i + 1, i + 1);
			}
		}
	} // end of pragma omp parallel

	if (results_found == 0) {
		printf("-1\n");
		return 0;
	}

	// Serial insertion sort
	int j;
	int temp;

	for (i = 1; i < results_found; ++i) {
		j = i - 1;
		temp = results_list[i];
		while (j >= 0 && comparator(&search_state[temp], &search_state[results_list[j]])<0) {
			results_list[j + 1] = results_list[j];
			--j;
		}
		results_list[j + 1] = temp;
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
	debug_only(total_results += results_found);
	return 0;
}


int main() {

	init_arrays();

	//maybe larger for fewer reallocations
	size_t len = 1000000;
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
	size_t i;

	// Wait 3 seconds before printing R (R starts the timer)
	sleep(3);
	debug_print("Finished sleeping.\n");


	for (i = 0; i < MAX_NODES; ++i) {
		search_state[i].start = MAX_VAL;
	}

	char action;
	int action_count = 0;
	int queries_count = 0;

	// totally ready to start
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

		// TODO: implement fast input (nonblocking)
		// Read the rest of input
		input_len = getline(&line, &len, stdin);
		line[input_len - 1] = '\0';
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
	print_report();
	free(nodes_ar);
	free(nodes_map);
	free(line);
	return 0;
}

