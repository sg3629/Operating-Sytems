#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <pthread.h>
#include <string.h>

/*
 Currently MAX_NUM_THREADS is
 set to 8. So if there are more
 than 8 files the each thread may
 process multiple files
 */
#define MAX_NUM_THREADS 8

/*
 Holds the arguments for all threads
 tid -> threadIndex
 */
typedef struct threadArguments {
	int tid;
}threadArguments;

/*
 Queue to hold the list of files
 */
typedef struct fileNameQueue{
    char *curFileName;
    struct fileNameQueue* nextFile;
}fileNameQueue;

/*
 Holds global list of files to be processed
 Each thread uses fileQueueUpdateLock
 synchronization lock to get its chunk
 of work
 */
struct fileNameQueue *globalFileQueue;
pthread_mutex_t fileQueueUpdateLock;


void destroyThreadArguments (struct threadArguments* threadArgPtr){

	if(NULL != threadArgPtr){
	
		free(threadArgPtr);
		threadArgPtr = NULL;
	}
}

/*
 Creating first node of global queue
 */
struct fileNameQueue* createQueue(char *filename){
    
    struct fileNameQueue* queueHead = (struct fileNameQueue*)malloc(sizeof(struct fileNameQueue));
    
    queueHead->curFileName = strdup(filename);
    
    queueHead->nextFile = NULL;
    
    return queueHead;
    
}

/*
 Inserting into a global queue
 */
void insertInQueue(struct fileNameQueue *queueHead, char *filename){
    
    struct fileNameQueue* curfileNode = queueHead;
    
    
    while(NULL != curfileNode->nextFile){
        curfileNode = curfileNode->nextFile;
    }
    
    
    struct fileNameQueue* newNode = (struct fileNameQueue*)malloc(sizeof(struct fileNameQueue));
    newNode->curFileName = strdup(filename);
    
    newNode->nextFile = NULL;
    
    curfileNode->nextFile = newNode;
    
    return;
}

/*
 Remove a chunk of work (one file to process) for queue
 */
char* removeFromQueue(struct fileNameQueue **queueHead){
    
    struct fileNameQueue* curfileNode = *queueHead;
    char *fileToPass = NULL;
    
    if(NULL != curfileNode){
        if(NULL != curfileNode->curFileName){
            fileToPass = strdup( curfileNode->curFileName);
            
            free(curfileNode->curFileName);
            curfileNode->curFileName = NULL;
        }
        
        *queueHead = curfileNode->nextFile;
        
        free(curfileNode);
    }
    
    return fileToPass;
}

/*
 This is the main Thread function.
 Every thread gets its job (file) from
 global file list and starts processing.
 */
void *PrintData(void *threadargs)
{
    struct threadArguments *threadArgPtr = (struct threadArguments*)threadargs;
    size_t len = 1000;      //the length of bytes getline will allocate
    size_t read;
    char *line = NULL;
    char *fileToProcess;
   
    FILE *file;
    
    pthread_mutex_lock (&fileQueueUpdateLock);
    fileToProcess = removeFromQueue(&globalFileQueue);
    pthread_mutex_unlock (&fileQueueUpdateLock);
    
    while (fileToProcess != NULL)
    {
        file = fopen(fileToProcess, "r");
        
        if(file){
            // Print out each line in the file
            while ((read = getline(&line, &len, file)) != -1)
            {
                printf("Retrieved line of length %d- threadId = %d  (%s):\n", read, threadArgPtr->tid, fileToProcess);
                printf("%s", line);
            }
            fclose(file);
        }
        
        free(fileToProcess);
        
        pthread_mutex_lock (&fileQueueUpdateLock);
        fileToProcess = removeFromQueue(&globalFileQueue);
        pthread_mutex_unlock (&fileQueueUpdateLock);
        
        //fprintf(stderr, "\n Final unique Count = %u", threadArgPtr->uniqueCount);
    }
    
    destroyThreadArguments(threadArgPtr);
    
    pthread_exit(NULL);
}



int main(int argc, char *argv[])
{
	DIR *dir;	//directory stream
	FILE *file;	//file stream
	struct dirent *ent;	// directory entry structure
	char *line = NULL;	// pointer to 
	size_t len = 1000;	//the length of bytes getline will allocate
	size_t read;

    unsigned int iThread = 0;
    
	unsigned int threadCount = 0;

	char full_filename[256];	//will hold the entire file name to read		
	
   	int rc;

   	pthread_t threads[MAX_NUM_THREADS];

    pthread_attr_t attr;
    
	// check the arguments
	if(argc < 2)
	{
		printf("Not enough arguments supplied\n");
		return -1;
	}

	if(argc > 2)
	{
		printf("Too many arguments supplied\n");
		return -1;
	}

    if(0 != pthread_mutex_init(&fileQueueUpdateLock, NULL)){
        fprintf(stderr, "\n Mutex init failed !!");
    }
    
    /*
     Threads will be created with
     joinable attribute. So that each
     thread can finish before the we exit
     */
    
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
	// try to open the directory given by the argument
	if ((dir = opendir (argv[1])) != NULL)
	{
	  	/* print all the files and directories within directory */
	  	while ((ent = readdir (dir)) != NULL)
		{
			// Check if the list is a regular file
			if(ent->d_type == DT_REG)
			{
                //printf ("%s\n", ent->d_name);
				// Create the absolute path of the filename
				//snprintf(full_filename, sizeof full_filename, "./%s%s\0", argv[1], ent->d_name);
                
                
				//snprintf(arguments->filename, sizeof(full_filename), "./%s%s\0", argv[1], ent->d_name);
                snprintf(full_filename, sizeof(full_filename), "./%s%s\0", argv[1], ent->d_name);
                //fprintf(stderr, "\n (%s) ", full_filename);
                //Insert the file names in the global file list
                //Threads will get their work from this list
                if(NULL != globalFileQueue)
                {
                    insertInQueue(globalFileQueue, full_filename);
                    
                }
                else{
                    globalFileQueue = createQueue(full_filename);
                    
                }
                //sprintf(arguments->filename, "./%s%s\0", argv[1], ent->d_name);
                
                
				// open the file
				/*
                 file = fopen(full_filename, "r");
                 // file was not able to be open
                 if (file != NULL)
                 {
                 // Print out each line in the file
                 while ((read = getline(&line, &len, file)) != -1) 				{
                 printf("Retrieved line of length %d:\n", read);
                 printf("%s", line);
                 }
                 fclose(file);
                 }
                 */
			}
	  	}
        /*
        Only MAX_NUM_THREADS of threads are created.
        Each thread may or may not process multiple
        files from the global list created
        */
        for( iThread = 0; iThread < MAX_NUM_THREADS; iThread++)
        {
            struct threadArguments *arguments = (struct threadArguments *)malloc(sizeof(struct threadArguments));
            
            arguments->tid = iThread;
            
            rc = pthread_create(&threads[iThread], &attr, PrintData, (void *)arguments);
            if (rc){
                printf("ERROR; return code from pthread_create() is %d\n", rc);
                exit(-1);
            }
            
            
        }
        
		// Close the directory structure
	  	closedir (dir);
	}
	else
	{
	  	/* could not open directory */
	  	perror ("");
  		return -1;
	}
    
    pthread_attr_destroy(&attr);
    /*
     Wait for all threads to finish processing its work
     */
    for( iThread = 0; iThread < MAX_NUM_THREADS; iThread++)
    {
        pthread_join(threads[iThread], NULL);
    }
    
    pthread_mutex_destroy(&fileQueueUpdateLock);
    
   pthread_exit(NULL);

return 0;
}
