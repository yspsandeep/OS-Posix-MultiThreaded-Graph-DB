#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/shm.h>
#include <stdbool.h>

// Define the message format

typedef struct Request
{
    long sequence_number;
    int operation_number;
    char graph_file_name[100];
} Request;

int msgid;

int main()
{

    Request request;
    key_t keyq;

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
    char input;
    while (true)
    {
        printf("Want to terminate the appilication ? Press Y (Yes) or N (No)\n");

        // scanf("%c", &input);
        gets(&input);

        if (input == 'Y')
        {
            request.sequence_number = 101;
            if (msgsnd(msgid, &request, sizeof(request), 0) == -1)
            {
                fprintf(stderr, "msgsnd");
                exit(1);
            }
            break;
        }
    }

    return 0;
}
