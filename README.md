# CSCI 4061 Fall 2023 Programming Assignment 3

Canvas PA Group 60

### Group Members:
- Matthew Breach (breac001) 
- Matthew Johnson (joh18723)
- Harrison Wallander (walla875)

### CSELabs Computer Used for Testing:
- csel-kh1260-14.cselabs.umn.edu

### Makefile Changes:
- None

### Additional Assumptions:
- None

### AI Code:
- None

### Individual Contributions:
- breac001
    - Worker threads, code styling, README, debugging
- joh18723
    - processing threads, directory traversal, debugging
- walla875
    - main thread, thread creation/merging, README, debugging

### Pseudocode for Our Project:
1. In image_rotation.c:
```
log_pretty_print(file, threadId, reqNum, filePath){
    create outputStr
    concat [<threadId>][<reqNum>][<filePath>] onto outputStr
    
    fprintf(file, outputStr)
    printf(outputStr)
}

processing(args){
    Set processing_args pa to be args
    intialize dirent de
    open directory path of pa

    loop through directories{
        wait for processor_cond
        
        set length to directory's name
        if the directory's name ends with ".png"{
            lock filePaths_lock
            set request_queue filePaths[nextOpen] to be path to current dir (.png)
            lock request_lock
            increment nextOpen
            unlock request_lock
            unlock filePaths_lock
        }
    }

    enlesslyLoop{
        wait for processor_cond
        if job queue is empty{
            close dir
            assert exitCond var to kill workers
            broadcast worker_cond
            exit thread NULL
            return NULL
        }
        otherwise, signal worker_cond
    }
}

worker(args){
    convert args to tNum (the thread number)
    endlesslyLoop{
        ifNoWorkToBeDone{
            check for exit conditions
            exit thread if cond. is met
        }

        lock request_lock
        decrement number of jobs
        set fpath to path of image
        set rotationAngle to the specified rotation angle
        signal processor_cond
        unlock request_lock

        init width, height, bpp
        linImage = stbi_load(fpath, &width, &height, &bpp, CHANNEL_NUM)

        intiialize result and image matrices, result_matrix and img_matrix

        linear_to_image(linImage, img_matrix, width, height)

        if rotate 180 degrees{
            flip_left_to_right(img_matrix, result_matrix, width, height)
        }
        else if rotate 270 degrees{
            flip_upside_down(img_matrix, result_matrix, width, height)
        }

        set fname to output of get_filename_from_path(fpath)

        create and malloc newPath and oldPath

        set newPath = {outputFolder}/{fname}
        set oldPath = {inputFolder}/{fname}

        create img_array

        flatten_mat(result_matrix, img_array, width, height)

        stbi_write_png(newPath, width, height, CHANNEL_NUM, img_array, width * CHANNEL_NUM)

        lock log_lock
        incrememnt requestNums array element at tNum
        log_pretty_print(logFile, *tnum, reqNums[*tnum], oldPath)
        unlock log_lock

        for(i = 0; i < width; i++){
            free(ith element of img_matrix)
            free(ith element of result_matrix)
        }
        free(img_matrix)
        free(result_matrix)
        set img_matrix and result_matrix to NULL

        free(img_array)
        free(linImage)
        free(newPath)
        free(oldPath)
    }
    exit thread NULL
    return NULL
}

main(argc, argv){
    check argc
    convert argv[3] and argv[4] to ints N and ra
    check N and ra bounds
    open and check logFile
    
    init global queue with rotation angle and nextopen = 0
    create, malloc, and populate pargs with directoryPath, N, and ra
    init procThread
    pthread_create(&procThread, NULL, (void*) processing, (void*) pargs);

    init workerThds array size N
    init threadId array size N
    for (i = 0, i < N, i++){
        threadId[i] = i
        pthread_create(&workerThds[i], NULL, (void*) worker, (void*) &threadIDs[i]);
    }

    join procThread

    for (i = 0, i < N, i++){
        join workerThds[i]
    }

    close and check logFile

    free(pargs)
}
```
2. In image_rotation.h: (Just struct definitions)
```
typedef struct request_queue
{
    char filePaths[100][100];
    int rotationAngle;
    int nextOpen;
}request_t; 

typedef struct processing_args
{
   int num_Workers;
   int rotationAngle;
   char *directoryPath;
} processing_args_t;
```
