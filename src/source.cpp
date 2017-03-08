#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
// NOTE: do we need type control?
#include <stdint.h>
#include <unistd.h>
#include <list>
#include <map>


#ifdef _OPENMP
#include <omp.h>
#endif

// Debugging defines
//#define NDEBUG
#include <assert.h>

// PARALLEL CONFIGURATION
//#define PARALLEL_SORT
#define NUM_THREADS 2		// Good value: CPU threads - 1 seems to be the fastest atm
#define PARALLEL_CHUNK_SIZE 750 // Good value: 750 for large test case


// ARRAY AND MEMORY CONFIGURATION
#define ARRAY_NODES 	1000000 // The bigger the better probably
				// Run atleast 1 time without NDEBUG if you make changes to this.

#define ARRAY_MAX_DEPTH	10 // Depth bigger than this only gets maps
			  // Use small value if ARRAY_NODES is small



#define MAX_NODES 	10000000
#define MAX_USED_CHAR   256



// define max value
#define MAX_VAL SIZE_MAX

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


typedef unsigned char KEY_TYPE;

struct NODE {

	static size_t total;
	
	NODE() {
		word_ending = 0;
		depth = 0;
		id = total++;
		ch_type = 0;
	}

	int word_ending;			// NOTE: type for bool values
	int depth;
	unsigned int id;			// offset in the node arrays
	int ch_type;	// 0 null,
			// 1 NODE* Array[256]
			// 2 map<KEY_TYPE, NODE*>
	std::map<KEY_TYPE, NODE*> map_children;
	NODE** ar_children;
};


struct N_GRAM {
	size_t start;
	size_t end;
};

NODE*** node_child;
size_t next_node_child = 0;
size_t missed_arrays = 0;

void init_arrays() {
	size_t i;
	size_t j;

	#ifndef NDEBUG
	for (i=0; i<4; ++i) {
		debug_print("Allocating %zuGB in %zu\n", (size_t)ARRAY_NODES * 8 * 256 / 1000000000, 4-i);
		sleep(1);
	}
	#endif

	node_child = (NODE***) malloc(ARRAY_NODES * sizeof(NODE **));
	for (i=0; i<ARRAY_NODES; ++i) {
		node_child[i] = (NODE**) malloc(MAX_USED_CHAR*sizeof(NODE*));
		for (j=0; j<MAX_USED_CHAR; ++j) {
			node_child[i][j] = NULL;
		}
	}
}

void free_arrays() {
	size_t i;

	for (i=0; i<ARRAY_NODES; ++i) {
		free(node_child[i]);
	}
	free(node_child);
}


NODE* get_child(NODE* root, unsigned char c) {
	if (!root->ch_type) {
		return NULL;
	}

	if (root->ch_type == 1){
		return root->ar_children[c];
	}
	std::map<KEY_TYPE, NODE*>::iterator it;
	it = root->map_children.find(c);
	return (it != root->map_children.end() ? it->second : NULL);

}

NODE* get_create_child(NODE* root, unsigned char c) {

	if (root->ch_type == 0) {
		// decide what type
		if (root->depth > ARRAY_MAX_DEPTH) {
			root->ch_type = 2;
		}
		else {
			if (next_node_child < ARRAY_NODES) {
				root->ch_type = 1;
				root->ar_children = node_child[next_node_child++];
			}
			else {
				root->ch_type = 2;
				++missed_arrays;
			}
		}
	}
	
	if (root->ch_type == 1) {
		// its an array
		NODE** temp = &(root->ar_children[c]);
		if (*temp) {
			return *temp;
		}
		else {
			// create
			*temp = new NODE();
			(*temp)->depth = root->depth + 1;
			return *temp;
		}
	}
	else {
		// its a map
		NODE** temp = &(root->map_children[c]);
		if (*temp) {
			assert(*temp);
			return *temp;
		}
		else {
			// create
			*temp = new NODE();
			(*temp)->depth = root->depth + 1;
			return *temp;
		}
	}
}




N_GRAM search_state[MAX_NODES];
unsigned int results_list[MAX_NODES]; // max nodes
size_t results_found = 0; // free spot 
size_t NODE::total = 0; //used for node id

void free_node(NODE* current_node) {
	if (current_node) {
		return;
	}

	size_t i;
	NODE* temp;	

	for(i=0; i<MAX_USED_CHAR; ++i) {
		temp = get_create_child(current_node, i);
		free_node(temp);
	}
	delete current_node;
}

void add_word(NODE* root, char* word_input) {
	NODE* current_node = root;

	char *word = word_input;

	assert(root);
	assert(word);

	while (*word != '\0') {
		current_node = get_create_child(current_node, *word);
		assert(current_node);
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

	std::map<KEY_TYPE, NODE*>::iterator found;

	while (*word != '\0') {

		current_node = get_child(current_node, *word);
		if (!current_node) {
			return 0;
		}
		++word;
	}
	assert(current_node);
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

		current_node = get_child(current_node, *search);
		if (!current_node) {
			return;
		}
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

	NODE *trie = new NODE();
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
	debug_print("Words added: %zu\n", words_added); //. Total nodes in trie: %zu\n", words_added, next_empty_array);

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
				if (action_count % 10000 == 0) {debug_print("Action: %d\n", action_count);}
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
	debug_print("Arrays used: %zu\nArrays missed: %zu\n", next_node_child, missed_arrays);
	free_node(trie);
	free(line);
	free_arrays();

	return 0;
}


