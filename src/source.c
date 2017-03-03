#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#pragma GCC diagnostic ignored "-Wchar-subscripts" // using assert for that


#define MAX_NODES 10000
#define MAX_WORD_LEN 400
#define MAX_SEARCH_LEN 1000

#define NODE_ARRAY_SIZE 100
#define NODE_ARRAY_OFFSET ' ' //character 0 for in node array

typedef struct NODE {
	struct NODE** children;
	int word_ending; // NOTE: type for bool values
	int depth;
	int id; // offset in the node arrays
} NODE;

// per search additional for nodes
// override_word_ending


// FIFO was implemented and used in previous versions.
// Although we don't use it at the moment we may need it later.


/* FIFO CODE
typedef struct FIFO_NODE {
	int array_start; //offset in the question string, where this search starts

	struct FIFO_NODE* next;
} FIFO_NODE;

*
typedef struct FIFO {
	FIFO_NODE first;
	FIFO_NODE* last;
} FIFO;

FIFO* get_new_fifo() {
	FIFO* fifo;
	fifo = (FIFO*)malloc(sizeof(FIFO));
	fifo->first.array_start = -2;
	fifo->first.next = NULL;
	fifo->last = &fifo->first;
	return fifo;
}

void fifo_insert(FIFO* queue, int start) {
	FIFO_NODE* added = (FIFO_NODE*) malloc(sizeof(FIFO_NODE));

	queue->last->next = added;
	added->next = NULL;
	added->array_start = start;
	queue->last = added;
}

int fifo_remove(FIFO* queue) {
	FIFO_NODE* removed;

	if (!queue->first.next) {
		debug_print("FIFO is empty.\n");
		return -1;
	}
	if (queue->first.next == queue->last) {
		queue->last = &queue->first;
	}
	removed = queue->first.next;
	queue->first.next = removed->next;
	return removed->array_start;
}
*/


// Static for now. NOTE: need to test big sized arrays for static vs dynamic
NODE* node_arrays[MAX_NODES][NODE_ARRAY_SIZE] = {};
int next_empty_array = 0;
int search_state[MAX_NODES];

NODE* get_new_node() {
	assert(next_empty_array < MAX_NODES);

	NODE* node = (NODE *)malloc(sizeof(NODE));

	node->word_ending = 0;
	node->id = next_empty_array;
	node->children = node_arrays[next_empty_array++];
	return node;
}

void add_word(NODE* root, char* word) {
	int i;
	NODE* current_node = root;

	assert(root);
	assert(word);

	for (i=0; word[i] != '\0'; ++i) {
		assert(word[i] >= NODE_ARRAY_OFFSET && word[i] < NODE_ARRAY_OFFSET + NODE_ARRAY_SIZE);

		if (!current_node->children[word[i]]) {
			current_node->children[word[i]] = get_new_node();
			current_node->children[word[i]]->depth = current_node->depth + 1;
		}

		current_node = current_node->children[word[i]];
	}
	assert(current_node);
	current_node->word_ending = 1;
	debug_print("Total Nodes in trie: %d after adding: %s\n", next_empty_array, word);
}

// NOTE: Unused function. Was here for testing purposes
int item_exists(NODE* root, char* search, int length, int start_point) {
	int i;
	NODE* current_node = root;

	assert(current_node);
	assert(search);
	assert(length <= MAX_WORD_LEN);

	for (i=start_point; i<length; ++i) {
		assert(current_node);
		assert(search[i] >= NODE_ARRAY_OFFSET && search[i] <= NODE_ARRAY_OFFSET + NODE_ARRAY_SIZE);
		if (current_node->children[search[i]] == NULL) {
			return 0;
		}
		current_node = current_node->children[search[i]];
	}
	assert(current_node);
	if (current_node->word_ending) {
		return 1;
	}
	return 0;
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

int jobs_array[1000];
int iterator_counter = 0;
int jobs_array_ptr = 0;


int search_from(NODE* root, char* search, int start) {
	int i;
	NODE* current_node = root;
	assert(current_node);
	// TODO:
	// BUG: of by 1 mistake. Node lags 1 letter behind
	for (i=start; i<strlen(search)+1; ++i) {
		iterator_counter++;
		assert(current_node);
		if (current_node->word_ending == 1 && search_state[current_node->id] == 0 && (search[i] == ' ' || search[i] == '\0')) {
			// result found
			int j;
			for (j=start; j < current_node->depth+start; ++j) {
				printf("%c", search[j]);
			}
			printf("|");
			search_state[current_node->id] = 1;
		}
		if (current_node->children[search[i]] == NULL) {
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
			debug_print("Adding job: %d\n", i+1);
			jobs_array[jobs_array_ptr++] = i+1;
		}
	}

	// execute all jobs
	for (i=0; i<jobs_array_ptr; ++i) {
		search_from(root, search, jobs_array[i]);
	}

	printf(" < in %d iterations!\n", iterator_counter);
	return 0;
}

int main(){

	NODE *trie = get_new_node();
	trie->depth = 0;

	add_word(trie, "AC AB");
	add_word(trie, "AC ABAB");
	add_word(trie, "ABAB");
	add_word(trie, "AB CA AB");
	add_word(trie, "AB AB ABAB");
	add_word(trie, "AB AB");
	add_word(trie, "AB");
	search_implementation(trie, "AC ABAB AB AB CA AB AB ABAB");
	search_implementation(trie, "AC ABAB AB AB CA AB AB ABAB");

/*
	char input[1000] = "";
	while (1){
		fgets(input, 100, stdin);

		input[strlen(input)-1] = '\0'; // TODO: unsafe. Use fgetc...
		if (input[0] == 'S') {
			break;
		}

		add_word(trie, input);
	}

	printf("Give Q string: \n");
	fgets(input, 1000, stdin);
	input[strlen(input)-1] = '\0';

	search_implementation(trie, input);
	search_implementation(trie, input); // double check that all variables init correctly
*/
	return 0;
}
