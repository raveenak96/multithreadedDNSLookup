#include "multi-lookup.h"

queue shared_queue; //for hostnames
queue file_queue;

pthread_mutex_t lock_queue; //for locking hostname queue
pthread_mutex_t lock_file_queue; //for locking file queue
pthread_mutex_t lock_parser; //for locking parse logging
pthread_mutex_t lock_converter; //for locking converter logging
pthread_cond_t queue_empty; //hostname queue
pthread_cond_t queue_full; //hostname queue
pthread_cond_t file_queue_empty; //file queue

int files_done = 0;
int num_parse_threads;
int num_convers_threads;
int num_input_files;
FILE *parse_log;
FILE *convers_log;
int err_p,err_c;

pid_t parser_ids[10];
int parser_nums[10];


void* parser_logging(pid_t tid) {

    //increase count for thread
    for(int i=0;i<num_parse_threads;i++) {
        if(parser_ids[i]==tid) {
            parser_nums[i]++;
            return NULL;
        }
    }

    //adding tid to parser id array
    for(int i=0;i<num_parse_threads;i++) {
        if(parser_ids[i]==0) {
            parser_ids[i] = tid;
            parser_nums[i] = 0;
            return NULL;
        }
    }
    return NULL;
}


void* readFiles() {

	pthread_t tid = pthread_self();
    parser_logging(tid);

    while(1) {

    	pthread_mutex_lock(&lock_file_queue);

    	//checking if there are files to be processed
    	while(empty(&file_queue)) {
            //check if all files read
    		if(files_done==num_input_files) {
    			pthread_mutex_unlock(&lock_file_queue);
    			return NULL;
    		}

            //otherwise wait for items in the queue
    		pthread_cond_signal(&queue_empty);
    		pthread_cond_wait(&file_queue_empty,&lock_file_queue);
    	}
    //dequeue from file queue and start processing
    char* filename = (char*) dequeue(&file_queue);
    parser_logging(tid);

	//read the file
	FILE *input_file = fopen(filename,"r");
	if(input_file == NULL) {
		fprintf(stderr,"Error: Could not open input file %s\n",filename);
		pthread_mutex_lock(&lock_parser);
		files_done++;
		pthread_mutex_unlock(&lock_parser);
		pthread_mutex_unlock(&lock_file_queue);
		free(filename);
		continue;
	}

    //scan input file and add to queue
	char hostname[MAX_NAME_LENGTH];
	while(fscanf(input_file,"%1025s",hostname) > 0) {
        //check hostname length
        if(strlen(hostname)==1024) {
            fprintf(stderr,"Error: Hostname length is too long. Must be less than 1025 characters.\n");
            exit(EXIT_FAILURE);
        }
        pthread_mutex_lock(&lock_queue);

        //wait if hostname queue is full
        while(full(&shared_queue)) {
			pthread_cond_wait(&queue_full,&lock_queue);
		}
        enqueue(&shared_queue,strdup(hostname));
        pthread_cond_signal(&queue_empty);
		pthread_mutex_unlock(&lock_queue);
	}
    //count number of files done
	pthread_mutex_lock(&lock_parser);
    files_done++;
    pthread_mutex_unlock(&lock_parser);


	fclose(input_file);
    free(filename);
    pthread_mutex_unlock(&lock_file_queue);
    }

	return NULL;


}

void* parser_pool(char* input_files) {
    char** input_filepaths = (char**) input_files;
    pthread_t parser_threads[num_parse_threads];

    //create parser threads
    for(int i=0;i<num_parse_threads;i++) {
    	int err;
        err = pthread_create(&parser_threads[i],NULL,(void*) readFiles,NULL);
    	if(err!=0) {
    		fprintf(stderr,"Error: Could not create parser thread number %d\n",i);
    		exit(EXIT_FAILURE);
    	}
    }

    //add input files to the file queue
    for(int i=0;i<num_input_files;i++) {
		pthread_mutex_lock(&lock_file_queue);
		enqueue(&file_queue,strdup(input_filepaths[i]));
		pthread_mutex_unlock(&lock_file_queue);
        pthread_cond_signal(&file_queue_empty);
	}

    //join threads and log to parser log
    for(int i=0;i<num_parse_threads;i++) {
    	int ret;
        pthread_join(parser_threads[i],(void**) &ret);
        if(ret!=0) {
        	fprintf(stderr,"Error: Could not join parser thread %d\n",i);
        	exit(EXIT_FAILURE);
        }
        pthread_mutex_lock(&lock_parser);
        fprintf(parse_log,"Thread %d serviced %d files.\n",parser_ids[i],parser_nums[i]);
        pthread_mutex_unlock(&lock_parser);
	}

    return NULL;
}

void* nameResolve() {

	while(1) {

        pthread_mutex_lock(&lock_queue);
        //queue is empty
		while(empty(&shared_queue)) {
            //check if all files have already been processed
            pthread_mutex_lock(&lock_parser);
            int done = 0;
            if(files_done==num_input_files) {
                done = 1;
            }
            pthread_mutex_unlock(&lock_parser);
            if(done) {
                pthread_mutex_unlock(&lock_queue);
                return NULL;
            }
            //otherwise wait for items to be put into the queue
            pthread_cond_wait(&queue_empty,&lock_queue);
		}
		//dequeue from hostname queue and perform lookup
		char* hostname = (char*) dequeue(&shared_queue);
		pthread_cond_signal(&queue_full);
		char first_ip[MAX_IP_LENGTH];

		if(dnslookup(hostname,first_ip,sizeof(first_ip))==UTIL_FAILURE) {
			fprintf(stderr,"Error: IP Address could not be resolved for %s\n",hostname);
			strncpy(first_ip,"",MAX_IP_LENGTH);
		}
		//log in conversion log
		pthread_mutex_lock(&lock_converter);
		fprintf(convers_log,"%s,%s\n",hostname,first_ip);
		pthread_mutex_unlock(&lock_converter);
		hostname[0] = '\0';
		free(hostname);
		strncpy(first_ip,"",sizeof(first_ip));
        pthread_mutex_unlock(&lock_queue);
	}

	return NULL;
}

void* converter_pool() {
    pthread_t converter_threads[num_convers_threads];

    for(int i=0;i<num_convers_threads;i++) {
        int err;
        err = pthread_create(&converter_threads[i],NULL,(void*) nameResolve,NULL);
    	if(err!=0) {
    		fprintf(stderr,"Error: Could not create converter thread number %d",i);
    		exit(EXIT_FAILURE);
    	}
    }

    for(int i=0;i<num_convers_threads;i++) {
    	int ret;
        pthread_join(converter_threads[i],(void**) &ret);
        if(ret!=0) {
        	fprintf(stderr,"Error: Could not join converter thread %d\n",i);
        	exit(EXIT_FAILURE);
        }
    }

    return NULL;
}

int main(int argc, char *argv[])

{
    clock_t start_clock = clock();

    //check user input
	if (argc < 6) {
		printf("Not enough arguments provided\n");
		return(EXIT_FAILURE);
	}

	num_parse_threads = atoi(argv[1]);
	num_convers_threads = atoi(argv[2]);
	num_input_files = argc-5;

	if (num_input_files > MAX_INPUT_FILES) {
		printf("Too many input files provided. Please provide less than 10.\n");
		exit(EXIT_FAILURE);
	}

	if (num_input_files < 1) {
		printf("Must provide at least one input file.\n");
		exit(EXIT_FAILURE);
	}



    if (num_parse_threads > MAX_CONVERT_THREADS) {
		printf("Can't create more than 10 parse threads.\n");
		exit(EXIT_FAILURE);
	}

    if (num_parse_threads < 1) {
		printf("Must create at least one parse thread.\n");
		exit(EXIT_FAILURE);
	}

	if (num_convers_threads > MAX_CONVERT_THREADS) {
		printf("Can't create more than 10 conversion threads.\n");
		exit(EXIT_FAILURE);
	}

	if (num_convers_threads < 1) {
		printf("Must create at least one conversion thread.\n");
		exit(EXIT_FAILURE);
	}

    //check if log files are valid
	parse_log = fopen(argv[3],"r+");
	if(parse_log == NULL){
		perror("Can't open parser log path. Error");
		exit(EXIT_FAILURE);
	}

	convers_log = fopen(argv[4],"r+");
	if(convers_log == NULL){
		perror("Can't open conversion log path. Error");
		fclose(parse_log);
		exit(EXIT_FAILURE);
	}

    //init queues to hold files and hostnames
    init_queue(&shared_queue,QUEUE_SIZE); //hostname queue
    init_queue(&file_queue,MAX_INPUT_FILES); //input file queue
    char* input_files[num_input_files];

	//get input files
	for(int i=0;i<num_input_files;i++) {
		input_files[i] = argv[i+5];
	}

    //initialize conditions and mutexes
	pthread_cond_init(&queue_full,NULL);
	pthread_cond_init(&queue_empty,NULL);
	pthread_cond_init(&file_queue_empty,NULL);
	pthread_mutex_init(&lock_queue,NULL);
	pthread_mutex_init(&lock_file_queue,NULL);
	pthread_mutex_init(&lock_parser,NULL);
	pthread_mutex_init(&lock_converter,NULL);

    //create parser thread pool
	pthread_t parser_id;

	err_p = pthread_create(&parser_id,NULL,(void*) parser_pool,(void*) input_files);

	if (err_p!=0) {
		fprintf(stderr,"Error in creating parser pool: [%s]",strerror(err_p));
        exit(EXIT_FAILURE);
	}

    //create converter thread pool
    pthread_t converter_id;
    err_c = pthread_create(&converter_id,NULL,(void*) converter_pool,NULL);
	if(err_c!=0) {

		fprintf(stderr,"Error in creating converter thread: [%s]",strerror(err_c));
		exit(EXIT_FAILURE);
	}

    //wait for pools to join
    pthread_join(parser_id,NULL);
    pthread_join(converter_id,NULL);

    //close log files
    fclose(parse_log);
	fclose(convers_log);

	//clear queue memory
    clear_queue(&shared_queue);
    clear_queue(&file_queue);

    //destroy mutexes and conditions
    pthread_mutex_destroy(&lock_queue);
    pthread_mutex_destroy(&lock_file_queue);
    pthread_mutex_destroy(&lock_parser);
    pthread_mutex_destroy(&lock_converter);
    pthread_cond_destroy(&queue_full);
    pthread_cond_destroy(&queue_empty);
    pthread_cond_destroy(&file_queue_empty);


    //calculate program time
    clock_t stop_clock = clock();
    double total_time = (double)(stop_clock-start_clock)/CLOCKS_PER_SEC;
    printf("Program time: %f\n",total_time);

    pthread_exit(NULL);
	return 0;

}

