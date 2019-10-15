#ifndef MULTITHREAD_H
#define MULTITHREAD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "util.h"
#include "queue.h"

#define MAX_INPUT_FILES 10
#define MAX_CONVERT_THREADS 10
#define MAX_NAME_LENGTH 1025
#define MAX_IP_LENGTH INET6_ADDRSTRLEN
#define QUEUE_SIZE 60

//logging for the parser
void* parser_logging(pid_t tid);

//read input files
void* readFiles();

//create parser thread pool
void* parser_pool(char* input_files);

//resolve the hostname
void* nameResolve();

//create converter thread pool
void* converter_pool();

int main(int argc, char *argv[]);

#endif




