//
// Created by jantonio on 2/06/19.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>

#define IMALIVE_TOOLS_C

#include "tools.h"

unsigned int strhash(const char *key) {
    unsigned int h=3323198485ul;
    for (;*key;++key) {
        h ^= *key;
        h *= 0x5bd1e995;
        h ^= h >> 15;
    }
    return h;
}

// fork from https://github.com/irl/la-cucina/blob/master/str_replace.c
/**
 * implementation of str_replace from github
 * @param string original string
 * @param substr string to search
 * @param replacement string to replace with
 * @return replaced string or null on fail
 */
char *str_replace(const char* string, const char* substr, const char* replacement) {
    char* tok = NULL;
    char* newstr = NULL;
    char* oldstr = NULL;
    size_t   oldstr_len = 0;
    size_t   substr_len = 0;
    size_t   replacement_len = 0;

    newstr = strdup(string);
    substr_len = strlen(substr);
    replacement_len = strlen(replacement);

    if (substr == NULL || replacement == NULL) { return newstr; } // no search nor replace

    // iterative replace all instances of search string with replacement
    while ((tok = strstr(newstr, substr))) {
        oldstr = newstr;
        oldstr_len = strlen(oldstr);
        newstr = (char*)malloc(sizeof(char) * (oldstr_len - substr_len + replacement_len + 1));
        if (newstr == NULL) { free(oldstr); return NULL;  } // malloc failed
        // compose new string
        memcpy(newstr, oldstr, tok - oldstr);
        memcpy(newstr + (tok - oldstr), replacement, replacement_len);
        memcpy(newstr + (tok - oldstr) + replacement_len, tok + substr_len, oldstr_len - substr_len - (tok - oldstr));
        memset(newstr + oldstr_len - substr_len + replacement_len, 0, 1);
        // and repeat loop for next search
        free(oldstr);
    }

    // original code free()s provided source. don't do that, as may be used outside these code
    // free(string);
    return newstr;
}

int stripos(char *haystack, char *needle) {
    char *ptr1, *ptr2, *ptr3;
    if( haystack == NULL || needle == NULL)  return -1;
    for (ptr1 = haystack; *ptr1; ptr1++)    {
        for (ptr2 = ptr1, ptr3 = needle;
            *ptr3 && (toupper(*ptr2) == toupper(*ptr3));
            ptr2++, ptr3++
        ); // notice ending loop
        if (!*ptr3) return (int)(ptr1 - haystack + 1);
    }
    return -1;
}

int strpos(char *haystack, char *needle) {
    if( haystack == NULL || needle == NULL)  return -1;
    char *p = strstr(haystack, needle);
    if (p) return (int)(p - haystack);
    return -1;
}

// add str2 to str1
char * str_concat(char *str1,char *str2) {
        size_t len=0;
        char *s;
        if (!str2) return str1;
        if (str1!=NULL) len = strlen(str1);
        len += strlen(str2) + 1 * sizeof(*str2); // sizeof to handle multibyte
        s = realloc(str1, len);
        strcat(s, str2);
        return s;
}

/**
 * dump data array in hexadecimal string format
 * @param data data array
 * @param len array length
 * @return resulting string or null on error
 */
char *hexdump(char *data, size_t len) {
    if (!data) return NULL;
    char *result=calloc(1+2*len,sizeof(char));
    if (!result) return NULL;
    for (int n=0;n<len;n++) sprintf(result+2*n,"%02X",*(data+n));
    return result;
}

// this implementation does not handle escape char
// also split up to 32 tokens.
// nelem returns number of tokens parsed
char **tokenize(char *line, char separator,int *nelem) {
    char *buff = calloc(1+strlen(line), sizeof(char));
    strncpy(buff,line,strlen(line)); // copy string into working space
    char **res=calloc(32,sizeof(char *));
    *nelem=0;
    char *from=buff;
    for (char *to = buff; *to; to++) { // while not end of string
        if (*to != separator) continue; // not separator; next char
        *to = '\0';
        res[*nelem] = from;
        // point to next token to explode (or null at end)
        from = to+1;
        *nelem= 1+*nelem;
        if (*nelem>32) return res;
    }
    // arriving here means end of line. store and return
    res[*nelem]=from;
    *nelem= 1+*nelem;
    return res;
}

void free_tokens( char** tokens) {
    if(!tokens) return;
    if (*tokens) free(*tokens);
    free(tokens);
}

/* get current timestamp in miliseconds */
long long current_timestamp() {
    struct timeval te;
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds
    // printf("milliseconds: %lld\n", milliseconds);
    return milliseconds;
}

int file_exists(char *fname) {
    return (access(fname,R_OK)>=0)?1:0;
}

/*************************************** fifo queue management */
queue *queue_create() {
    queue *q=calloc(1,sizeof(queue));
    if (!q) return NULL;
    q->front=NULL; // next element to get out
    q->rear=NULL; // last inserted element
    return q;
}

void queue_destroy(queue *q) {
    if (!q) return;
    for (node_t *node=q->front; node->next; node=node->next) {
        if (node->data) free(node->data);
    }
    free(q);
}

void *queue_put(queue *q,void * data,size_t *size) {
    if (!q) return NULL;
    if (!data) return NULL;
    // creamos entrada en la cola y copiamos los datos en dicha entrada
    node_t *node=calloc(1,sizeof(node_t));
    if (!node) return NULL;
    void *m=calloc(1,*size);
    if (!m) {
        free(node);
        return NULL;
    }
    memcpy(m,data,*size);
    node->data=m;
    node->data_len=*size;
    node->next=NULL;
    // insert element before last item in queue
    if (q->front==NULL) q->front=node; // empty queue
    node->next=q->rear;
    q->rear=node;
    q->size++;
    return node->data;
}

void *queue_get(queue *q,size_t *size) {
    if (!q) { *size=0; return NULL; } // no queue
    if (!q->front) { *size=0; return NULL; } // empty queue
    // retrieve front node
    node_t *node=q->front;
    char *msg=node->data;
    *size=node->data_len;
    q->front=node->next;
    if (!q->front) q->rear=NULL; // queue becomes empty
    q->size--;
    // extract stored data, fix len and free node
    void *data=node->data;
    *size=node->data_len;
    free(node);
    return data;
}

// pick requested element from queue. DO NOT remove
void *queue_pick(queue *q,size_t *size) {
    if (!q) return NULL; // null
    if (!q->front) return NULL; // empty
    // extract data from queue front without removeing
    char *data=calloc(1,q->front->data_len);
    if (!data) { *size=0; return NULL;}
    memcpy(data,q->front->data,q->front->data_len);
    *size=q->front->data_len;
    return data;
}

// indexes are allways correlative, so just substract
size_t queue_size(queue *q) {
    if (!q) return -1;
    return q->size;
}


#undef IMALIVE_TOOLS_C