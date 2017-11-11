# ACM SIGMOD 2017 - Programming contest
Our submission for acm sigmod 17 programming contest, ranked in place #21 😅 (team Titans). The task of that year can be found [here](http://sigmod17contest.athenarc.gr/task.shtml), as well as some of the best submissions with available source code on the contest's [leaderboard](http://sigmod17contest.athenarc.gr/leaders.shtml) page.

## Task Details

N-grams of words are often used in natural language processing and information extraction tasks. An N-gram is a contiguous sequence of N words. So for example, if we have the phrase "the book is on the table" and we want to extract all N-grams with N=3 then the N-grams would be:

* the book is
* book is on
* is on the
* on the table

In this contest, the task is to search documents and return strings from a given set, as quickly as possible. Each string represents an N-gram. We will provide an initial set of strings which you may process and index. Once this is done, we will begin issuing a workload consisting of a series of queries (documents) and N-gram updates (insertions or deletions), arbitrarily interleaved. For each N-gram insertion or deletion, the list of N-grams of interest is updated accordingly. For each new query (document) arriving, the task is to return as fast as possible the N-grams of the currently up-to-date list that are found in the document. These should be presented in order of their first appearance in the document. If one N-gram is a prefix of another and the larger one is in the document, then the shorter one is presented first. Input to your program will be provided on the standard input, and the output must appear on the standard output.

## Testing Protocol

Our test harness will first feed the initial set of N-grams to your program's standard input. Your program will receive multiple lines where each one contains a string which represents an N-gram. The initial set ends with a line containing the character 'S'.

After sending the initial set of strings, our test harness will monitor your program's standard output for a line containing the character 'R' (case insensitive, followed by the new line character '\n'). Your program uses this line to signal that it is done ingesting the initial set of N-grams, has performed any processing and/or indexing on it and is now ready to receive the workload.

The test harness delivers the workload in batches. Each batch consists of a sequence of operations provided one per line followed by a line containing the single character 'F' that signals the end of the batch. Each operation is represented by one character ('Q', 'A' or 'D') that defines the operation type, followed by a space and either an N-gram or a document.

The three operation types are as follows:

* 'Q'/query: This operation needs to be answered with the N-grams that have been found in the document. Your program will provide a line for each document. The line contains all the extracted N-grams separated by '|'. If there are no extracted N-grams, your program should answer with -1.
* 'A'/add: This operation requires you to modify your set of N-grams by adding a new one. If the specified N-gram already exists, the set should remain unchanged. This operation should not produce any output.
* 'D'/delete: This operation requires you to modify your set of N-grams by removing an N-gram. If the specified N-gram does not exist, the set should remain unchanged. This operation should not produce any output.

After the end of every batch, our test harness will wait for output from your program before providing the next batch. You need to provide as many lines of output as the number of the query ('Q') operations in the batch - each line containing the ordered N-grams that have matched separated by '|'. Your program is free to process operations in a batch concurrently. However, the query results in the output must reflect the order of the queries within the batch. After the last batch, the standard input stream of your program will be closed by our test harness.

Your solution will be evaluated for correctness and execution time. Execution time measurement does not start until your program signals (with 'R') that it is finished ingesting the initial set of strings. Thus, you are free to pre-process or index the N-grams as you see fit without penalty, as long as your program runs within the overall testing time limit. Concurrent request execution within each batch is allowed and encouraged, as long as the results mimic a sequential execution of the operations within the batch. In particular, the result for each query must reflect all additions and deletions that precede it in the workload sequence, and must not reflect any additions and deletions that follow it.

## Algorithm design
Since the task involves locating sequences of characters, a Trie structure was used to store all the n-grams. A query string would be split to chunks amongst multiple threads and the Trie would be traversed locating each word on it's path. An insertion sort algorithm would be used later, to sort the output, since the results was relatively sorted. The hard part was the Trie structure itself, since it should support wide characters (pretty rare though), lock-free additions/deletions and shouldn't consume memory exponentially.

Having an array of characters in each node, wastes space from unused character-paths as well as the large amount of memory pointers needed to point on the children nodes. To overcome this problem, a hybrid structure was used, using as many as possible node arrays (dense nodes - fast lookups Θ(1)) and the remaining, as std::maps (red-black trees) allowing lower memory consumption for a Θ(logk) lookup cost, where k represents the unique characters. 

To reduce the memory needed by memory pointers, two array structures where used, storing the a priori created nodes. By doing so, the children pointers where replaced by int indices, adding one more level of indirection which requires less memory for children pointers, but makes the node traversal slower. Another advantage of this implementation is the almost zero cost of node initialization, speeding up  execution since dynamic memory initializations aren't required.

Finally, this repository consists of multiple branches, containing many variations of the solution, using different structures and parallization constructs.

## Compile 
To compile the project, simply run the `make` command. The available targets are:
* make all
* make dbg (force use openmp, don't use -O3)
* make omp (force use openmp, use -O3)
* make noflags (don't use any flags)

## Debugging
Valgrind support has been added, to check if all pipes work properly through it.
```bash
# Compile harness, source and prepare test data
./test.sh -prepare
# Run harness in debug mode (on small input)
./test.sh -v
```

## Contributors
| <img alt="HarryCoderK" src="https://avatars3.githubusercontent.com/u/1407769?v=4" width="48"> | <img alt="memaskal" src="https://avatars3.githubusercontent.com/u/782005?v=4" width="48"> |
| :--: | :--: |
| [HarryCoderK](https://github.com/HarryCoderK) | [memaskal](https://github.com/memaskal) |

