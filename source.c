#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// NOTE: do we need type control?
// #include <stdint.h>
#include <assert.h>

// Debugging defines
//#define NDEBUG

#ifndef NDEBUG
#define debug_print(...)  do{ fprintf(stdout, __VA_ARGS__); }while(0)
#else
#define debug_print(...)  do{ }while(0);
#endif


#define MAX_NODES 10000
#define MAX_WORD_LEN 400
#define MAX_SEARCH_LEN 1000

#define NODE_ARRAY_SIZE 100
#define NODE_ARRAY_OFFSET ' ' //character 0 for in node array

typedef struct NODE {
	struct NODE** children;
	int word_ending; // NOTE: type for bool values
} NODE;

// Static for now. NOTE: need to test big sized arrays for static vs dynamic
NODE* node_arrays[MAX_NODES][NODE_ARRAY_SIZE] = {};
int next_empty_array = 0;

NODE* get_new_node(){
	assert(next_empty_array < MAX_NODES);

	NODE* node = (NODE *)malloc(sizeof(NODE));

	node->word_ending = 0;
	node->children = node_arrays[next_empty_array++];

	return node;
}

void add_word(NODE* root, const char* word, int length) {
	int i;
	NODE* current_node = root;

	assert(root);
	assert(word);
	assert(length < MAX_WORD_LEN);

	for (i=0; i<length; ++i) {
		assert(word[i] >= NODE_ARRAY_OFFSET && word[i] < NODE_ARRAY_OFFSET + NODE_ARRAY_SIZE);

		if (current_node->children[word[i]]) {
			// node exists, continue
			current_node = current_node->children[word[i]];
		}
		else {
			current_node->children[word[i]] = get_new_node();
			current_node = current_node->children[word[i]];
		}
	}
	assert(current_node);
	current_node->word_ending = 1;
	debug_print("Total Nodes in trie: %d after adding: %s\n", next_empty_array, word);
}

int item_exists(NODE* root, const char* search, int length, int start_point) {
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
int remove_word(NODE* root, const char* word, int length) {
	int i;
	NODE* current_node = root;


	assert(current_node);
	assert(word);
	assert(length <= MAX_WORD_LEN);

	for (i=0; i<length; ++i){
		assert(current_node);
		if (current_node->children[word[i]] == NULL) {
			return 0;
		}
		current_node = current_node->children[word[i]];
	}
	assert(current_node);
	current_node->word_ending = 0;
}

int main(){
	NODE *trie = get_new_node();

	char* test = "test";
	char* test2 = "test 123";


	char* search1 = "test";
	char* search2 = "test 123";

	debug_print("=== SIGMOD 2017 SUBMITION ===\n");

	add_word(trie, test, strlen(test));
	add_word(trie, test2, strlen(test2));


	remove_word(trie, "test", strlen("test"));
	debug_print("Removed word: test\n");

	debug_print("Searching for %s in trie. -> ", search1);
	if (item_exists(trie, search1, strlen(search1), 0)) {
		debug_print("Found it.\n");
	}
	else {
		debug_print("Not found.\n");
	}

	debug_print("Searching for %s in trie. -> ", search2);
	if (item_exists(trie, search2, strlen(search2), 0)) {
		debug_print("Found it.\n");
	}
	else {
		debug_print("Not found.\n");
	}
	return 0;
}
