//
// Created by jantonio on 2/06/19.
//

#ifndef IMALIVE_TOOLS_H
#define IMALIVE_TOOLS_H

#include<stdio.h>

#define signature(a) ((a)<0)?-1:(((a)>0)?1:0)

typedef struct node_st {
    void *data;
    size_t data_len;
    struct node_st *next;
} node_t;

typedef struct queue_st {
    size_t size;
    node_t *front; // first element to fetch on get()
    node_t *rear; // last item inserted in queue with put()
} queue;

#ifndef IMALIVE_TOOLS_C
#define EXTERN extern
#else
#define EXTERN
#endif
/* string functions not declared in ANSI C */
EXTERN unsigned int strhash(const char *key); // simple hash for strings
EXTERN int stripos(char* haystack, char* needle );
EXTERN int strpos(char* haystack, char* needle );
EXTERN char *hexdump(char *data, size_t len);
EXTERN char *str_replace(const char* string, const char* substr, const char* replacement);
EXTERN char *str_concat(char *str1,char *str2); // add str2 to str1

EXTERN char **tokenize(char* line, char separator,int *nelem );
EXTERN void free_tokens(char **tokens); // free previous tokenized data

/* extra comodity functions */
EXTERN long long current_timestamp(); // timestamp in miliseconds
EXTERN int file_exists(char *fname);

/* fifo queue management */
EXTERN queue *queue_create();
EXTERN void queue_destroy(queue *queue);
EXTERN void *queue_put(queue *queue, void *data,size_t *len);
EXTERN void *queue_get(queue *queue,size_t *len);
EXTERN void *queue_pick(queue *queue,size_t *len);
EXTERN size_t queue_size(queue *queue);

#undef EXTERN
#endif // IMALIVE_TOOLS_H
