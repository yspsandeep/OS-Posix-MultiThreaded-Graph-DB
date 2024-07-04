#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/shm.h>
#include <pthread.h>
#include <stdbool.h>

#define MAX_GRAPH_SIZE 256

// Define the message format
typedef struct LBRequest_t
{
    long mtype;
    long sequence_number;
    int operation_number;
    char graph_file_name[100];
} LBRequest;
typedef struct Result_t
{
    long mtype;
    char mtext[100];
} Result;

int msgid;

int *shared_memory_lock;
int getFileNumber(char *c)
{
    int n = strlen(c);
    if (n == 6)
    {
        return (int)c[1] - '0';
    }
    else if (n == 7)
    {
        if (c[1] == '2')
        {
            return 20;
        }
        else
        {
            return 10 + (int)c[2] - '0';
        }
    }
}
void Semaphore_lock(int n)
{

    while (shared_memory_lock[n - 1] == 0)
        ;

    shared_memory_lock[n - 1]--;
}
void Semaphore_unlock(int n)
{

    shared_memory_lock[n - 1]++;
}
// function for handing task of primary server
void *primaryServer(void *argument)
{
    Result result;
    key_t keysm;
    int shmid;
    LBRequest lbRequest = *((LBRequest *)argument);
    if ((keysm = ftok("LoadBalancer.c", lbRequest.sequence_number)) == -1)
    {
        perror("ftok");
        exit(1);
    }

    // Create or get the shared memory
    shmid = shmget(keysm, MAX_GRAPH_SIZE * MAX_GRAPH_SIZE * sizeof(int), 0666 | IPC_CREAT);
    if (shmid == -1)
    {
        perror("shmget");
        exit(1);
    }

    // Attach shared memory to the process
    int *shared_memory = shmat(shmid, NULL, 0);
    if (shared_memory == (int *)-1)
    {
        perror("shmat");
        exit(1);
    }

    FILE *fptr;

    // Open a file in writing mode

    fptr = fopen(lbRequest.graph_file_name, "w");

    // Write some text to the file

    char temp[100];
    int n = shared_memory[0];

    sprintf(temp, "%d", n);
    fprintf(fptr, "%s", temp);
    fprintf(fptr, "%s", "\n");
    shared_memory++;
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {

            // printf("%d ",shared_memory[i*n + j]);
            fprintf(fptr, "%d", shared_memory[i * n + j]);
        }
        // printf("\n");
        fprintf(fptr, "\n");
    }
    // Close the file

    fclose(fptr);

    result.mtype = lbRequest.sequence_number + 300;
    strcpy(result.mtext, "File Created/Edited !");

    // Send the message to the message queue.
    if (msgsnd(msgid, &result, sizeof(result), 0) == -1)
    {
        fprintf(stderr, "\nError in msgsnd");
        exit(1);
    }
    Semaphore_unlock(getFileNumber(lbRequest.graph_file_name));
}

int main()
{

    int lock;
    key_t keyq, keysm1;
    pthread_t threads[100];
    int i = 0;

    // Creating message queue
    if ((keyq = ftok("LoadBalancer.c", 1000)) == -1)
    {
        perror("ftok");
        exit(1);
    }

    // Create or get the message queue
    msgid = msgget(keyq, 0666);
    if (msgid == -1)
    {
        perror("msgget");
        exit(1);
    }

    // semophore shm
    if ((keysm1 = ftok("LoadBalancer.c", 101)) == -1)
    {
        perror("ftok");
        exit(1);
    }
    // Create or get the shared memory
    lock = shmget(keysm1, 20 * sizeof(int), 0666);
    if (lock == -1)
    {
        perror("shmget");
        exit(1);
    }

    // Attach shared memory to the process
    shared_memory_lock = shmat(lock, NULL, 0);
    if (shared_memory_lock == (int *)-1)
    {
        perror("shmat");
        exit(1);
    }

    while (true)
    {

        printf("Primary server is Listening: \n");
        fflush(stdout);
        LBRequest lbRequest;
        if (msgrcv(msgid, &lbRequest, sizeof(lbRequest), 201, 0) == -1)
        {
            fprintf(stderr, "msgrcv");
            exit(1);
        }

        if (lbRequest.sequence_number == 101)
        {
            printf("Terminating !!\n");
            break;
        }

        Semaphore_lock(getFileNumber(lbRequest.graph_file_name));

        // fflush(stdout);
        if (pthread_create(&threads[i], NULL, primaryServer, (void *)&lbRequest) != 0)
        {
            fprintf(stderr, "Error creating thread \n");
            exit(EXIT_FAILURE);
        }
        i++;
    }
    for (int j = 0; j < i; j++)
    {
        if (pthread_join(threads[j], NULL) != 0)
        {
            fprintf(stderr, "Error joining thread %d\n", j + 1);
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}
