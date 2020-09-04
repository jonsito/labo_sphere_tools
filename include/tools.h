//
// Created by jantonio on 2/06/19.
//

#ifndef IMALIVE_TOOLS_H
#define IMALIVE_TOOLS_H

#include<stdio.h>

typedef struct qitem_st {
    time_t expire;
    int index;
    char *msg;
    struct qitem_st *next;
} qitem_t;

typedef struct queue_st {
    char *name;
    int last_index;
    qitem_t *first_out; // first element to fetch on get()
    qitem_t *last_out; // last item inserted in queue with put()
} queue_t;

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
EXTERN queue_t *queue_create(char *name);
EXTERN void queue_destroy(queue_t *queue);
EXTERN qitem_t *queue_put(queue_t*queue, char * msg);
EXTERN char *queue_get(queue_t *queue);
EXTERN char *queue_pick(queue_t *queue,int id);
EXTERN size_t queue_size(queue_t *queue);
EXTERN void queue_expire(queue_t *queue);

#undef EXTERN
#endif // IMALIVE_TOOLS_H
