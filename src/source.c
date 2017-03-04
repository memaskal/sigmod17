#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
// NOTE: do we need type control?
// #include <stdint.h>
#include <assert.h>

// Debugging defines
//#define NDEBUG

#ifndef NDEBUG
#define debug_print(...)  do{ fprintf(stderr, __VA_ARGS__); }while(0)
#else
#define debug_print(...)  do{ }while(0);
#endif

#pragma GCC diagnostic ignored "-Wchar-subscripts" 	// !! DANGEROUS !!
							// TODO: fix all chars to unsigned chars

#define MAX_NODES 100000
#define MAX_WORD_LEN 400
//#define MAX_SEARCH_LEN 10000

#define NODE_ARRAY_SIZE 200
#define NODE_ARRAY_OFFSET ' ' //character 0 for in node array

#define JOBS_SIZE 10000

typedef struct NODE {
	struct NODE** children;
	int word_ending; // NOTE: type for bool values
	int depth;
	int id; // offset in the node arrays
} NODE;

// Static for now. NOTE: need to test big sized arrays for static vs dynamic
NODE* node_arrays[MAX_NODES][NODE_ARRAY_SIZE] = {};
int next_empty_array = 0;
int search_state[MAX_NODES];

int nodes_freed = 0;
int results_found = 0;

int jobs_array[JOBS_SIZE];
int iterator_counter = 0;
int jobs_array_ptr = 0;

NODE* get_new_node() {
	assert(next_empty_array < MAX_NODES);

	NODE* node = (NODE *)malloc(sizeof(NODE));
	
	node->word_ending = 0;
	//node->id = next_empty_array++;
	node->id = next_empty_array;
	
	node->children = node_arrays[next_empty_array++];
	// NOTE: used calloc for test to run commented assert
	// calloc is fast :P calloc is good
	//node->children = (NODE **)calloc(NODE_ARRAY_SIZE, sizeof(NODE*));
	return node;
}

void free_node(NODE* root) {
	int i;
	for (i=0; i<NODE_ARRAY_SIZE; ++i) {
		if (root->children[i]) {
			free_node(root->children[i]);
		}
	}
	free(root);
	nodes_freed++;
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
	int i;
	NODE* current_node = root;

	assert(current_node);
	assert(word);

	for (i=0; word[i]!='\0'; ++i) {
		assert(current_node);
		if (current_node->children[word[i]] == NULL) {
			return 0;
		}
		current_node = current_node->children[word[i]];
	}
	assert(current_node);
	current_node->word_ending = 0;
	return 1;
}

int search_from(NODE* root, char* search, int start) {
	unsigned int i;
	NODE* current_node = root;
	assert(current_node);
	// TODO:
	// BUG: of by 1 mistake. Node lags 1 letter behind
	for (i=start; i<strlen(search)+1; ++i) {
		iterator_counter++;
		assert(current_node);
		if (current_node->word_ending == 1 && search_state[current_node->id] == 0 && (search[i] == ' ' || search[i] == '\0')) {
			#pragma omp critical
			{
				int j;
				for (j=start; j < current_node->depth+start; ++j) {
					printf("%c", search[j]);
					debug_print("%c", search[j]);
					results_found++;
				}
				printf("|");
				debug_print("|");
			
				search_state[current_node->id]++;
			}
		}

		if (current_node->children[(unsigned char)search[i]] == NULL) {
			// no path matches, stop searching.
			break;
		}
		current_node = current_node->children[search[i]];
	}
	return 0;
}

int search_implementation(NODE* root, char* search) {
	int i;

	// init search
	iterator_counter = 0;
	jobs_array_ptr = 0;

	// NOTE: find optimised way to do this
	for (i=0; i< next_empty_array; ++i) {
		search_state[i] = 0;
	}
	// end of init search

	// NOTE: this needs to be done on input if we use getchar()
	// make a job for each space character
	for (i=0; search[i]!='\0'; ++i) {
		if (search[i] == ' ') {
			jobs_array[jobs_array_ptr++] = i+1;
		}
	}
	debug_print("Jobs added: %d\n", i);

	// execute all jobs
	for (i=0; i<jobs_array_ptr; ++i) {
		#pragma omp task 
		{	
			search_from(root, search, jobs_array[i]);
		}
	}
	#pragma omp taskwait
	debug_print(" < in %d iterations!\n", iterator_counter);
	return 0;
}


int main(){

	NODE *trie = get_new_node();
	trie->depth = 0;
	
	//maybe larger for fiewer reallocations
	size_t len = 1024;
	char *line = (char*)malloc(len * sizeof(char));
	assert(line);
	
	size_t input_len;
	size_t words_added = 0;

	// while there is input read it
	while ((input_len = getline(&line, &len, stdin)) > 2 || 
		  (line[0] != 'S')) {

		// add the \0 char replacing \n 
		line[input_len - 1] = '\0';
		add_word(trie, line);
		words_added++;
	}
	debug_print("Words added: %zu. Total nodes in trie: %d\n", words_added, next_empty_array);

	// lets start our parallel code number of threads are set to maximum
	// by default -> i think :P
	#pragma omp parallel
	{
		#pragma omp master
		{
			// this one is run only by master thread (id = 0)
			// input finished we are ready to read the queries
			char action;
					
			// totaly ready to start
			printf("R\n");
			// TODO: find faster way to force fflush
			fflush(stdout);
	
			action = getchar_unlocked();
			// read junk space
			getchar_unlocked();
	
			// TDDO: implement fast input
			// Read the rest of input
			input_len = getline(&line, &len, stdin);
			line[input_len-1] = '\0';
			debug_print("Selected action was: %c and input size was: %zu\n", action, input_len);
			switch (action) {
				case 'Q':
					search_implementation(trie, line);
					break;	
				case 'A':
					debug_print("A\n");
			}		
			#pragma omp taskwait
		}	
	}
	debug_print("\nTotal results found during runtime: %d\n", results_found);
	free_node(trie);
	debug_print("Nodes freed: %d\n", nodes_freed);
	free(line);

	return 0;
}

