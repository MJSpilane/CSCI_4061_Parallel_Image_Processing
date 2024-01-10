#include "image_rotation.h"
 
char *inputFolder;
char *outputFolder;
int exitCond = 0;
struct request_queue rq;
bool isQueueEmpty = true;
//Global integer to indicate the length of the queue??
int queueLength;
//Global integer to indicate the number of worker threads
int num_Worker;
//Global file pointer for writing to log file in worker??
FILE *logFile;
//Might be helpful to track the ID's of your threads in a global array
int *threadIDs;
//What kind of locks will you need to make everything thread safe? [Hint you need multiple]
pthread_mutex_t filePaths_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t exit_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t isQueueEmpty_lock = PTHREAD_MUTEX_INITIALIZER;
//What kind of CVs will you need  (i.e. queue full, queue empty) [Hint you need multiple]
pthread_cond_t worker_cond_exit = PTHREAD_COND_INITIALIZER;
pthread_cond_t processor_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t worker_cond = PTHREAD_COND_INITIALIZER;
//How will you track the requests globally between threads? How will you ensure this is thread safe?
int *reqNums;
//How will you track which index in the request queue to remove next?
//How will you update and utilize the current number of requests in the request queue?
//How will you track the p_thread's that you create for workers?
//How will you know where to insert the next request received into the request queue?

int numProcessed = 0;
int numFinished = 0;

pthread_mutex_t numProcessedLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t numFinishedLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int request_count = 0;
pthread_mutex_t request_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;


/*
    The Function takes:
    to_write: A file pointer of where to write the logs. 
    requestNumber: the request number that the thread just finished.
    file_name: the name of the file that just got processed. 
    The function output: 
    it should output the threadId, requestNumber, file_name into the logfile and stdout.
*/
void log_pretty_print(FILE* to_write, int threadId, int requestNumber, char * file_name){
    // Create and populate output log string
    char outputStr[200];
    outputStr[0] = '\0';
    sprintf(outputStr, "[%d][%d][%s]", threadId, requestNumber, file_name);

    // Print outputStr to terminal and logFile
    fprintf(to_write, "%s\n", outputStr);
    printf("%s\n", outputStr);
}


/*
    1: The processing function takes a void* argument called args. It is expected to be a pointer to a structure processing_args_t 
    that contains information necessary for processing.
    2: The processing thread need to traverse a given dictionary and add its files into the shared queue while maintaining synchronization using lock and unlock. 
    3: The processing thread should pthread_cond_signal/broadcast once it finish the traversing to wake the worker up from their wait.
    4: The processing thread will block(pthread_cond_wait) for a condition variable until the workers are done with the processing of the requests and the queue is empty.
    5: The processing thread will cross check if the condition from step 4 is met and it will signal to the worker to exit and it will exit.
*/

void *processing(void *args)
{
    // Traverse given directory and add file paths to the queue
    struct processing_args *pa = (struct processing_args *) args;
    struct dirent *de;
    DIR *dr;
    if ((dr = opendir(pa->directoryPath)) == NULL){
        printf("Error, invalid directory input\n");
        exit(1);
    }
    

    while((de = readdir(dr)) != NULL){
        pthread_mutex_lock(&request_lock);
        bool queueFull = (rq.nextOpen >= 100); //global
        pthread_mutex_unlock(&request_lock);
        while(queueFull) {
            pthread_mutex_lock(&request_lock);
            pthread_cond_wait(&processor_cond, &request_lock); // global
            queueFull = (rq.nextOpen >= 100); // global
            pthread_mutex_unlock(&request_lock);
        }
        int length = strlen(de->d_name);
        if((de->d_name[length-1] == 'g') && (de->d_name[length-2] == 'n') && (de->d_name[length-3] == 'p') && (de->d_name[length-4] == '.')){
            pthread_mutex_lock(&request_lock);
            rq.filePaths[rq.nextOpen][0] = '\0'; //global
            char fileNameWPath[100];
            fileNameWPath[0] = '\0';
            strcat(fileNameWPath, pa->directoryPath);
            strcat(fileNameWPath, "/");
            strcat(fileNameWPath, de->d_name);
            strcpy(rq.filePaths[rq.nextOpen], fileNameWPath); // global
            rq.nextOpen +=1; // global

            pthread_mutex_lock(&numProcessedLock);
            numProcessed++;
            pthread_mutex_unlock(&numProcessedLock);

            pthread_cond_signal(&worker_cond); // signal the worker threads that there are items in the array. global
            pthread_mutex_unlock(&request_lock);


        }
    }

    if (closedir(dr) == -1){
                printf("Error closing input directory\n");
                exit(1);
    }

    // check if rq.filePaths is empty
    while(1) {
        pthread_mutex_lock(&request_lock);
        pthread_cond_wait(&processor_cond, &request_lock);// global
        isQueueEmpty = (rq.nextOpen <= 0); // global
        pthread_mutex_unlock(&request_lock);
        if (isQueueEmpty) { // if is empty   
            pthread_mutex_lock(&exit_lock);
            exitCond = 1; // global
            pthread_cond_broadcast(&worker_cond); //global
            pthread_mutex_unlock(&exit_lock);
            pthread_exit(NULL);
            return NULL;
        }
        else {
            pthread_mutex_lock(&request_lock);
            pthread_cond_signal(&worker_cond); //signal the queue is not empty. global
            pthread_mutex_unlock(&request_lock);
        }
    }
}

/*
    Worker Threads
    1: The worker threads takes an int ID as a parameter
    2: The Worker thread will block(pthread_cond_wait) for a condition variable that there is a requests in the queue. 
    3: The Worker threads will also block(pthread_cond_wait) once the queue is empty and wait for a signal to either exit or do work.
    4: The Worker thread will processes request from the queue while maintaining synchronization using lock and unlock. 
    5: The worker thread will write the data back to the given output dir as passed in main. 
    6: The Worker thread will log the request from the queue while maintaining synchronization using lock and unlock.  
    8: Hint the worker thread should be in a While(1) loop since a worker thread can process multiple requests and It will have two while loops in total
        that is just a recommendation feel free to implement it your way :) 
    9: You may need different lock depending on the job.  
*/
void * worker(void *args)
{
    int *tNum = (int*) args;
    while(1) {
        pthread_mutex_lock(&request_lock);
        while(rq.nextOpen <= 0){
            pthread_mutex_lock(&exit_lock);
            if (exitCond == 1){
                pthread_mutex_unlock(&exit_lock);
                pthread_mutex_unlock(&request_lock);
                pthread_exit(NULL);
            }
            pthread_mutex_unlock(&exit_lock);
            // check to see if there are requests in the queue
            pthread_cond_wait(&worker_cond, &request_lock); // global
        }

        // if so:
        // decremenet request cout
        rq.nextOpen = rq.nextOpen - 1; // global

        // get next file name and signal processing thread
        char fPathTemp[100]; // temp to store memcpy result
        memcpy(fPathTemp, rq.filePaths[rq.nextOpen], strlen(rq.filePaths[rq.nextOpen])+1); // global
        char *fpath = (char*)malloc(sizeof(char)*100);
        strcpy(fpath, fPathTemp); // copy to char* to avoid any errors later
        int rotationAngle = rq.rotationAngle; // global
        pthread_cond_signal(&processor_cond); // global
        pthread_mutex_unlock(&request_lock);

        // Read the image in linear form
        int width, height, bpp;
        uint8_t *linImage = stbi_load(fpath, &width, &height, &bpp, CHANNEL_NUM);

        // Set up result and image matrices
        uint8_t **result_matrix = (uint8_t **)malloc(sizeof(uint8_t*) * width);
        uint8_t** img_matrix = (uint8_t **)malloc(sizeof(uint8_t*) * width);
        for(int i = 0; i < width; i++){
            result_matrix[i] = (uint8_t *)malloc(sizeof(uint8_t) * height);
            img_matrix[i] = (uint8_t *)malloc(sizeof(uint8_t) * height);
        }

        // Convert linear to 2D for image rotation
        linear_to_image(linImage, img_matrix, width, height);

        if(rotationAngle == 180){ // Rotate 180 degrees, flip left-to-right
            flip_left_to_right(img_matrix, result_matrix, width, height);
        }
        else if(rotationAngle == 270){ // Rotat 270 degrees, fip upside down
            flip_upside_down(img_matrix, result_matrix, width, height);
        }

        // Set up path and save to output dir
        const char *fname = get_filename_from_path(fpath);
        char *newPath = malloc(sizeof(char) * 1024);
        char *oldPath = malloc(sizeof(char) * 1024);

        pthread_mutex_lock(&log_lock);
        snprintf(newPath, sizeof(char) * 1024, "%s/%s", outputFolder, fname);
        snprintf(oldPath, sizeof(char) * 1024, "%s/%s", inputFolder, fname);
        pthread_mutex_unlock(&log_lock);

        uint8_t *img_array = (uint8_t*)malloc(sizeof(uint8_t) * width * height);

        // Flatten matrix
        flatten_mat(result_matrix, img_array, width, height);

        // Write to PNG
        stbi_write_png(newPath, width, height, CHANNEL_NUM, img_array, width * CHANNEL_NUM);

        // Log to LogPrettyPrint
        pthread_mutex_lock(&log_lock);
        reqNums[*tNum]++;
        log_pretty_print(logFile, *tNum, reqNums[*tNum], oldPath); // global
        pthread_mutex_unlock(&log_lock);

        pthread_mutex_lock(&numFinishedLock);
        numFinished++;
        pthread_mutex_unlock(&numFinishedLock);


        // Free dynamically-allocated memory
        for(int i = 0; i < width; i++){
            free(img_matrix[i]);
            free(result_matrix[i]);
        }

        free(img_matrix);
        free(result_matrix);
        img_matrix = NULL;
        result_matrix = NULL;

        free(img_array);
        free(linImage);
        free(newPath);
        free(oldPath);
        free(fpath);

        pthread_mutex_lock(&mutex);

        if(numFinished == numProcessed ) {
            pthread_mutex_unlock(&mutex);
            pthread_cond_signal(&processor_cond);
            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&mutex);
    }
}

/*
    Main:
        Get the data you need from the command line argument 
        Open the logfile
        Create the threads needed
        Join on the created threads
        Clean any data if needed. 
*/

int main(int argc, char* argv[])
{
    if(argc != 5)
    {
        fprintf(stderr, "Usage: File Path to image directory, File path to output dirctory, number of worker thread, and Rotation angle\n");
    }

    // Set output folder
    inputFolder = argv[1];
    outputFolder = argv[2];

    // Convert and check number of workers and rotation angle
    int N = atoi(argv[3]);
    int ra = atoi(argv[4]);

    if (N > 100 || N < 1){
        printf("Error, invalid thread count\n");
        return -1;
    }
    if (ra != 180 && ra != 270){
        printf("Error, invalid rotation angle\n");
        return -1;
    }

    rq.nextOpen = 0; // global
    rq.rotationAngle = ra; //global
    // Open and check logFile
    logFile = fopen(LOG_FILE_NAME, "w");

    if (logFile == NULL){
        printf("Error, log file open failed\n");
        exit(1);
    }

    // Create processing thread
    struct processing_args *pa = malloc((sizeof(char)*100)+(sizeof(int)*2));
    pa->directoryPath = argv[1];
    pa->num_Workers = N;
    pa->rotationAngle = ra;
    pthread_t procThread;
    pthread_create(&procThread, NULL, (void*) processing, (void*) pa);

    // Init request number array
    reqNums = malloc(sizeof(int) * N);
    for (int i = 0; i < N; i++){
        reqNums[i] = 0;
    }

    // Create workers
    threadIDs = malloc(sizeof(int) * N);
    pthread_t workerThds[N];
    for (int i = 0; i < N; i++){
        threadIDs[i] = i;
        pthread_create(&workerThds[i], NULL, (void*) worker, (void*) &threadIDs[i]);
    }

    // Join all threads back when done
    pthread_join(procThread, NULL);

    for (int i = 0; i < N; i++){
        pthread_join(workerThds[i], NULL);
    }

    if (fclose(logFile) != 0){
        printf("Failed to close log file\n");
        exit(1);
    }

    // Free malloc'd variables
    free(pa);
    free(threadIDs);
    free(reqNums);

    return 0;
}
