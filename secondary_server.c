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

#define MAX_VERTICES 1000
#define MAX_GRAPH_SIZE 256
int msgid;
int *shared_memory_lock;

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
typedef struct Queue_t
{

    char *vertices;
    char *ans;
    bool *visited;
    char **adjacencyMatrix;
    int ans_ptr;
    char *cur_vertex;
    char start_vertex;
    int num_vertices;
    int back;
    int front;
    // char **arr;
    int arr_ptr;
    int prev_arr_ptr;
    pthread_t *thread;

} Queue;

pthread_mutex_t queueMutex;
pthread_mutex_t enqueueMutex;
pthread_mutex_t dequeueMutex;

void q_initializer(Queue *q, char start_vertex, int no_of_vertices, char **adj_matrix);
void enqueue(char vertex, Queue *s);
char dequeue(Queue *s);
void *bfs(void *arg);
void *add_bfs(void *arg);
typedef struct Stack_t
{

    char *vertices;
    char *ans;
    bool *visited;
    char **adjacencyMatrix;
    int ans_ptr;
    char *cur_vertex;
    char start_vertex;
    int num_vertices;
    int top;
    pthread_t *thread;
    int arr_ptr;

} Stack;
pthread_mutex_t lock;
pthread_mutex_t stackLock;
pthread_mutex_t threadLock;
pthread_mutex_t ansLock;

void s_initializer(Stack *s, char start_vertex, int no_of_vertices, char **adj_matrix);
void push(char vertex, Stack *s);
char pop(Stack *s);
void *dfs(void *arg);
void *add(void *arg);

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

int getStartingIndex(LBRequest);
char **getAdjMatrix(LBRequest);
int getNum(LBRequest);
void *operation4(void *argument)
{

    LBRequest lbRequest = *((LBRequest *)argument);

    char start_vertex = '0' + getStartingIndex(lbRequest);
    char **adj_matrix = getAdjMatrix(lbRequest);
    int no_of_vertices = getNum(lbRequest);

    Queue *queue;
    queue = (Queue *)malloc(sizeof(Queue));
    q_initializer(queue, start_vertex, no_of_vertices, adj_matrix);

    pthread_t bfs_main;
    pthread_create(&bfs_main, NULL, bfs, (void *)queue);
    pthread_join(bfs_main, NULL);
    queue->ans[queue->ans_ptr] = '\0';
    // printf("ans  is : %s", queue->ans);
    // fflush(stdout);

    Result result;
    result.mtype = lbRequest.sequence_number + 300;
    strcpy(result.mtext, queue->ans);

    // Send the message to the message queue.
    if (msgsnd(msgid, &result, sizeof(result), 0) == -1)
    {
        fprintf(stderr, "\nError in msgsnd");
        exit(1);
    }
}

void *operation3(void *argument)
{

    LBRequest lbRequest = *((LBRequest *)argument);

    char start_vertex = (char)('0' + getStartingIndex(lbRequest));
    char **adj_matrix = getAdjMatrix(lbRequest);

    int no_of_vertices = getNum(lbRequest);

    Stack *stack;
    stack = (Stack *)malloc(sizeof(Stack));

    s_initializer(stack, start_vertex, no_of_vertices, adj_matrix);

    push(stack->start_vertex, stack);

    stack->visited[(int)(*(stack->cur_vertex) - '0') - 1] = true;
    pthread_mutex_lock(&threadLock);

    pthread_create(&stack->thread[stack->arr_ptr], NULL, dfs, (void *)stack);

    while (true)
    {
        if (stack->arr_ptr == no_of_vertices)
            break;
    }

    for (int i = 0; i <= stack->arr_ptr - 1; i++)
    {
        pthread_join(stack->thread[i], NULL);
    }

    stack->ans[stack->ans_ptr] = '\0';

    Result result;
    result.mtype = lbRequest.sequence_number + 300;
    strcpy(result.mtext, stack->ans);

    // printf("mytpe : %ld",result.mtype);
    // fflush(stdout);
    //  Send the message to the message queue.
    if (msgsnd(msgid, &result, sizeof(result), 0) == -1)
    {
        fprintf(stderr, "\nError in msgsnd");
        exit(1);
    }
}

int main()
{

    key_t keyq, keysm2;
    pthread_t threads[100];
    int i = 0;
    int lockid = 0;

    if ((keyq = ftok("LoadBalancer.c", 1000)) == -1)
    {
        perror("ftok");
        exit(1);
    }

    if ((keysm2 = ftok("LoadBalancer.c", 101)) == -1)
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

    lockid = shmget(keysm2, 20 * sizeof(int), 0666);
    if (lockid == -1)
    {
        perror("shmget");
        exit(1);
    }

    // Attach shared memory to the process
    int *shared_memory_lock = shmat(lockid, NULL, 0);
    if (shared_memory_lock == (int *)-1)
    {
        perror("shmat");
        exit(1);
    }

    int flag;
    printf("Enter Secondary Server number:\n1-> Start Secondary Server 1\n2-> Start Secondary Server 2\n");
    printf("Enter choice: ");
    scanf("%d", &flag);
    flag--;

    while (true)
    {
        if (flag)
        {
            printf("Secondary Server 2 is Listening!!\n");
        }
        else
        {
            printf("Secondary Server 1 is Listening!!\n");
        }
        fflush(stdout);

        LBRequest lbRequest;

        if (msgrcv(msgid, &lbRequest, sizeof(lbRequest), 202 + flag, 0) == -1)
        {
            fprintf(stderr, "msgrcv");
            exit(1);
        }

        if (lbRequest.sequence_number == 101)
        {
            if (flag)
            {
                printf("Secondary Server 2 Terminating !!\n");
            }
            else
            {
                printf("Secondary Server 1 Terminating !!\n");
            }
            break;
        }

        while (shared_memory_lock[getFileNumber(lbRequest.graph_file_name) - 1] == 0)
            ;

        if (lbRequest.operation_number == 3)
        {
            if (pthread_create(&threads[i], NULL, operation3, (void *)&lbRequest) != 0)
            {
                fprintf(stderr, "Error creating thread \n");
                exit(EXIT_FAILURE);
            }
            i++;
        }
        else if (lbRequest.operation_number == 4)
        {
            if (pthread_create(&threads[i], NULL, operation4, (void *)&lbRequest) != 0)
            {
                fprintf(stderr, "Error creating thread \n");
                exit(EXIT_FAILURE);
            }
            i++;
        }
    }

    // joining threads
    // printf("i is %d",i);
    fflush(stdout);
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

int getStartingIndex(LBRequest lbRequest)
{
    key_t keysm;
    int shmid;
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
    return shared_memory[0];
}
char **getAdjMatrix(LBRequest lbRequest)
{
    FILE *fpointer;
    char buf[100];
    fpointer = fopen(lbRequest.graph_file_name, "r");
    if (fpointer == NULL)
    {
        printf("Failed to open file.\n");
        return NULL;
    }

    fgets(buf, sizeof(buf), fpointer);
    int no_of_vertices = (buf[0] - '0') + 0;
    // char adj_matrix[no_of_vertices][no_of_vertices];

    char **adj_matrix;
    adj_matrix = malloc(no_of_vertices * sizeof(char *));
    for (int i = 0; i < no_of_vertices; i++)
        adj_matrix[i] = malloc(no_of_vertices * sizeof(char));
    for (int i = 0; i < no_of_vertices; i++)
    {
        fgets(buf, sizeof(buf), fpointer);
        for (int j = 0; j < no_of_vertices; j++)
        {
            adj_matrix[i][j] = (buf[j]);
        }
    }
    fclose(fpointer);
    return adj_matrix;
}
int getNum(LBRequest lbRequest)
{
    FILE *fpointer;
    char buf[100];
    fpointer = fopen(lbRequest.graph_file_name, "r");
    if (fpointer == NULL)
    {
        printf("Failed to open file.\n");
        return 1;
    }

    fgets(buf, sizeof(buf), fpointer);
    int no_of_vertices = (buf[0] - '0') + 0;
    // char adj_matrix[no_of_vertices][no_of_vertices];

    return no_of_vertices;
}
void s_initializer(Stack *s, char start_vertex, int no_of_vertices, char **adj_matrix)
{

    s->num_vertices = no_of_vertices;

    s->start_vertex = start_vertex;

    s->vertices = malloc(no_of_vertices * sizeof(char));

    s->adjacencyMatrix = adj_matrix;

    s->visited = malloc(no_of_vertices * sizeof(bool));

    memset(s->visited, false, no_of_vertices * sizeof(bool));

    s->ans = malloc(no_of_vertices * sizeof(char));
    s->top = -1;
    s->ans_ptr = 0;

    s->cur_vertex = malloc(sizeof(char));
    *(s->cur_vertex) = start_vertex;

    s->thread = (pthread_t *)malloc(no_of_vertices * sizeof(pthread_t));
    s->arr_ptr = 0;
}
void push(char vertex, Stack *s)
{
    pthread_mutex_lock(&stackLock);
    // printf("I am here");
    // fflush(stdout);
    s->vertices[++(s->top)] = vertex;

    pthread_mutex_unlock(&stackLock);
}

char pop(Stack *s)
{
    pthread_mutex_lock(&stackLock);
    char vertex = '$';

    if (s->top >= 0)
    {
        vertex = s->vertices[(s->top)--];
    }

    pthread_mutex_unlock(&stackLock);
    return vertex;
}

void *add(void *arg)
{

    Stack *s = (Stack *)arg;

    int current_vertex = (int)(*(s->cur_vertex) - '0');
    pthread_mutex_unlock(&lock);

    int flag = 1;

    for (int i = 0; i < s->num_vertices; i++)
    {

        if (s->adjacencyMatrix[current_vertex - 1][i] == '1' && !s->visited[i])
        {
            flag = 0;
            s->visited[i] = true;
            pthread_mutex_lock(&threadLock);
            push((char)(i + '0' + 1), s);

            pthread_create(&s->thread[s->arr_ptr], NULL, dfs, (void *)s);
        }
    }

    if (flag)
    {

        pthread_mutex_lock(&ansLock);
        s->ans[s->ans_ptr] = '0' + current_vertex;
        (s->ans_ptr)++;
        pthread_mutex_unlock(&ansLock);
    }

    //}
}

void *dfs(void *arg)
{

    Stack *s = (Stack *)arg;
    s->arr_ptr++;
    pthread_mutex_unlock(&threadLock);

    if (s->top >= 0)
    {

        pthread_t dfs_add;

        char currentVertex = pop(s);

        pthread_mutex_lock(&lock);
        *(s->cur_vertex) = currentVertex;

        add((void *)s);
    }
}
void q_initializer(Queue *q, char start_vertex, int no_of_vertices, char **adj_matrix)
{
    q->num_vertices = no_of_vertices;
    q->start_vertex = start_vertex;

    // dynamic allocation - can be done for ans, visited

    q->adjacencyMatrix = adj_matrix;
    q->visited = malloc(no_of_vertices * sizeof(bool));
    memset(q->visited, 0, no_of_vertices * sizeof(bool));
    q->vertices = malloc(no_of_vertices * sizeof(char));
    q->ans = malloc(no_of_vertices * sizeof(char));
    q->back = 0;
    q->front = 0;
    q->ans_ptr = 0;
    q->cur_vertex = malloc(sizeof(char));
    *(q->cur_vertex) = start_vertex;

    q->thread = malloc(no_of_vertices * sizeof(pthread_t *));

    q->arr_ptr = 0;
    q->prev_arr_ptr = 0;
}

void enqueue(char vertex, Queue *q)
{
    pthread_mutex_lock(&enqueueMutex);
    // printf("%c\n",vertex);
    // fflush(stdout);
    //  pthread_mutex_lock(&queueMutex);
    if (q->back == MAX_VERTICES - 1)
    {
        fprintf(stderr, "Queue is full\n");
        exit(EXIT_FAILURE);
    }

    q->vertices[q->back] = vertex;
    q->back++;
    pthread_mutex_unlock(&enqueueMutex);
    // pthread_mutex_unlock(&queueMutex);
}

char dequeue(Queue *q)
{
    pthread_mutex_lock(&dequeueMutex);
    char vertex;
    if (q->front != q->back)
    {
        vertex = q->vertices[q->front];
        q->front++;
        if (q->front >= q->back)
        {
            q->front = q->back = 0;
        }
    }
    pthread_mutex_unlock(&dequeueMutex);
    return vertex;
}

void *add_bfs(void *arg)
{
    Queue *q = (Queue *)arg;

    int current_vertex = (int)(*(q->cur_vertex) - '0');
    pthread_mutex_unlock(&queueMutex);
    // printf("add cur is: %d   ", current_vertex);

    for (int i = 0; i < q->num_vertices; i++)
    {
        if (q->adjacencyMatrix[current_vertex - 1][i] == '1' && !q->visited[i])
        {
            q->visited[i] = true;
            // printf("   %c  ",(char)(i + '0' + 1) );
            enqueue((char)(i + '0' + 1), q);
        }
    }

    // printf("\n\n");
}

void *bfs(void *arg)
{
    Queue *q = (Queue *)arg;

    enqueue(q->start_vertex, q);
    q->visited[(int)(*(q->cur_vertex) - '0') - 1] = true;

    while (q->front != q->back)
    {
        // printf("I am out\n");
        // fflush(stdout);

        q->prev_arr_ptr = q->arr_ptr;
        int k = (q->back - q->front);

        // printf("size is :%d\n",k);

        while (k--)
        {
            // printf("I am in  ");
            // fflush(stdout);

            char currentVertex = dequeue(q);
            // printf("current vertex is %c\n",currentVertex);
            // fflush(stdout);
            pthread_mutex_lock(&queueMutex);
            *(q->cur_vertex) = currentVertex;
            q->ans[q->ans_ptr] = *(q->cur_vertex);

            (q->ans_ptr)++;
            pthread_create(&q->thread[q->arr_ptr], NULL, add_bfs, (void *)q);
            (q->arr_ptr)++;
        }

        // printf("I am here");
        // fflush(stdout);

        for (int i = q->prev_arr_ptr; i < q->arr_ptr; i++)
        {
            pthread_join(q->thread[i], NULL);
        }
    }
}
