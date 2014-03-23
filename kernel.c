/*
Kelvin Betances
Operating Systems
Project #1
3/17/14
*/
#include <stdio.h>
#include <stdlib.h>

#include "hardware.h"
#include "drivers.h"
#include "kernel.h"

/* You may use the definitions below, if you are so inclined, to
 define the process table entries. Feel free to use your own
 definitions, though. */

typedef enum { RUNNING, READY, BLOCKED , UNINITIALIZED } PROCESS_STATE;

typedef struct process_table_entry {
    PROCESS_STATE state;
    int CPU_time_used;
} PROCESS_TABLE_ENTRY;

PROCESS_TABLE_ENTRY process_table[MAX_NUMBER_OF_PROCESSES];

//process counter
int num_of_processes = 1;

/* Since you will have a ready queue as well as a queue for each semaphore,
 where each queue contains PIDs, here are two structure definitions you can
 use, if you like, to implement a single queue.
 In this case, a queue is a linked list with a head pointer
 and a tail pointer. */

typedef struct PID_queue_elt {
    struct PID_queue_elt *next;
    PID_type pid;
} PID_QUEUE_ELT;


typedef struct {
    PID_QUEUE_ELT *head;
    PID_QUEUE_ELT *tail;
} PID_QUEUE;

//queue for ready processes
PID_QUEUE * ready_queue;

/* This constant defines the number of semaphores that your code should
 support */
#define NUMBER_OF_SEMAPHORES 16

/* This is the initial integer value for each semaphore (remember that
 each semaphore should have a queue associated with it, too). */
#define INITIAL_SEMAPHORE_VALUE 1

//a semaphore consists of a integer value and a queue of processes
typedef struct
{
    PID_QUEUE * queue;
    int count;
} Semaphore;

//16 semphores
Semaphore semaphores [NUMBER_OF_SEMAPHORES];

//Counter to keep track of IO requests
int IO_counter = 0;

/* A quantum is 40 ms */
#define QUANTUM 40

//quantum start time
int current_quantum_start_time;

//places process at end of a ready queue
void placeIntoQueue(PID_QUEUE * queue,PID_type pid)
{
    //allocate memory for a new process
    PID_QUEUE_ELT *element = (PID_QUEUE_ELT *) malloc(sizeof(PID_QUEUE_ELT));
    element->pid = pid;
    element->next = NULL;
    
    //check if the head or tail queue is empty
    if ((queue->tail == NULL) && (queue->head == NULL))
    {
        queue->tail = element;
        queue->head = queue->tail;
        element->next = NULL;
    }
    //otherwise set the element to the tail of the current queue element
    else
    {
        queue->tail->next = element;
        queue->tail = element;
    }
}

//removes the current process id from the front of a ready queue
PID_type removeCurrentQueue(PID_QUEUE * queue)
{
    //check if current queue/tail queue is emty
    if ((queue->head == NULL) && (queue->tail == NULL))
        return IDLE_PROCESS;
    
    else
    {
        PID_QUEUE_ELT *front_queue = queue->head;
        queue->head = queue->head->next;
        //check if head queue is empty
        if (queue->head== NULL)
            queue->tail = NULL;
        
        return front_queue->pid;
    }
}

//check if queue is empty
int checkQueueEmpty(PID_QUEUE * queue)
{
    if ((queue->head == NULL) && (queue->tail == NULL))
        return TRUE;
    
    else
        return FALSE;
}

//Round Robin Scheduler implementation
PID_type roundRobinScheduler()
{
    current_pid = removeCurrentQueue(ready_queue);
    //check if a queue is idle
    if (current_pid == IDLE_PROCESS)
    {
        printf("Time %d: Processor is idle.\n", clock);
        process_table[current_pid].CPU_time_used += (clock - current_quantum_start_time);
        current_quantum_start_time = clock;
    }
    
    //check otherwise if a queue is not idle
    else
    {
        //Dequeue any empty queues
        while (process_table[current_pid].state == UNINITIALIZED)
            current_pid = removeCurrentQueue(ready_queue);
        
        printf("Time %d: Process %d runs.\n", clock, current_pid);
        
        //reset quantum
        current_quantum_start_time = clock;
        
    }
    return current_pid;
}

//handler for any trap processes
void trapHandler(){
    
    switch(R1){
        
        //check for a disk read process
        case(DISK_READ):
            printf("Time %d: ", clock);
            printf("Process %d issues disk read request.\n",current_pid);
            
            //make a disk read request
            disk_read_req(current_pid, R2);
            
            //block current process and increment the IO counter
            process_table[current_pid].state = BLOCKED;
            IO_counter++;
            
            //once current process quantum runs out, update the processes' CPU time
            process_table[current_pid].CPU_time_used += (clock - current_quantum_start_time);
            
            //choose another process to run with the scheduler
            roundRobinScheduler();
            
            break;
        
        //check for a disk write process
        case(DISK_WRITE):
            printf("Time %d: ", clock);
            printf("Process %d issues disk write request. \n",current_pid);
            
            //make a disk write request
            disk_write_req(current_pid);
            
            break;
        
        //check for a keyboard read process
        case(KEYBOARD_READ):
            printf("Time %d: ", clock);
            printf("Process %d issues keyboard write request. \n",current_pid);
            
            //make keyboard read
            keyboard_read_req(current_pid);
            
            //block current process and increment the IO counter
            process_table[current_pid].state = BLOCKED;
            IO_counter++;
            
            //once current process quantum runs out, update the processes' CPU time
            process_table[current_pid].CPU_time_used += (clock - current_quantum_start_time);
            
            //choose another process to run with the scheduler
            roundRobinScheduler();
            
            break;
        
        //check for a forked process
        case(FORK_PROGRAM):
            
            //check that the number of processes doesnt exceed the maximum or minimum
            if ( (R2 > MAX_NUMBER_OF_PROCESSES - 1) && (R2 < 0) )
            {
                printf("Time %d: ", clock);
                printf("Error! Number of processes to fork is greater than the maximum/minimum number of processes allowed.\n");
                exit(1);
            }
            
            //otherwise check if processes already exist
            PROCESS_TABLE_ENTRY * new_process = &process_table[R2];
            if(new_process->state != UNINITIALIZED)
            {
                printf("Error! PID of process to fork already exists.\n");
                exit(1);
            }
            else
            {
                printf("Time %d: ", clock);
                printf("Process %d forks process %d. Creating process entry for process: %d.\n", current_pid, R2, R2);
                
                //reset CPU and quantum time for the new process
                new_process->CPU_time_used = 0;
                current_quantum_start_time = 0;
                
                //set new process to ready status
                new_process->state = READY;
                
                //add process to ready queue
                placeIntoQueue(ready_queue,R2);
                
                //increase the number of processes due to successful fork procedure
                num_of_processes++;
            }
            break;
    
        //check for a end program procedure
        case(END_PROGRAM):
            
            //check if process is uninitialized
            if(process_table[R2].state == UNINITIALIZED)
            {
                printf("Error! PID of process does not exist. %d\n", R2);
                exit(1);
            }
            
            else
            {
                // otherwise update CPU time
                 process_table[R2].CPU_time_used += (clock - current_quantum_start_time);
                
                //make a new process to uninitialize it
                PROCESS_TABLE_ENTRY * new_process = &process_table[R2];
                new_process->state = UNINITIALIZED;
                
                printf("Time %d: ", clock);
                printf("Process %d exits. Total CPU time = %dms.\n", R2,  new_process->CPU_time_used);
                
                //uninitialize the processes' values
                new_process->CPU_time_used = 0;
                current_quantum_start_time = 0;
                num_of_processes--;
                
                //check if there are any processes to run
                if(num_of_processes <= 0)
                {
                    printf("--- No more processes to execute ---\n");
                    exit(1);
                }
                
                // otherwise choose another process to run with the scheduler
                else roundRobinScheduler();
            }
            
            break;
            
            
        //check for a semaphore operation
        case (SEMAPHORE_OP):
            //check for a DOWN operation
            if(R3 == 0)
            {
                printf("Time %d: Process %d issues DOWN on semaphore %d \n", clock, current_pid , R2 );
                //check if semaphore is already 0
                if(semaphores[R2].count == 0)
                {
                    //if semaphore is already 0, block the process and increase CPU time
                    process_table[R2].CPU_time_used += (clock - current_quantum_start_time);
                    process_table[current_pid].state = BLOCKED;
                    
                    //check if processes are deadlocked
                    if((ready_queue->head == NULL) && (IO_counter == 0))
                    {
                            printf("Time %d: DEADLOCK \n", clock);
                            exit(1);
                    }
                    //otherwise no deadlock has occured
                    else
                    {
                        placeIntoQueue(semaphores[R2].queue,current_pid);
                        roundRobinScheduler();
                    }
                }
                
                //otherwise semaphore is not 0 and complete the DOWN
                else
                    semaphores[R2].count--;
            }
            
            
            //check for a UP operation
            else if (R3 == 1)
            {
                printf("Time %d: Process %d issues UP on semaphore %d \n", clock, current_pid , R2);
    
                //check if the semaphore is 0 and the queue isnt empty
                if((semaphores[R2].count==0) && !(checkQueueEmpty(semaphores[R2].queue)))
                {
                    int pid = removeCurrentQueue(semaphores[R2].queue);
                    placeIntoQueue(ready_queue, pid);
                }
                
                //otherwise complete the UP
                else
                    semaphores[R2].count++;
            }
            
            break;
            
        //check for unknown processes
        default:
            printf("Error! Unknown process request.\n");
            exit(1);
            break;
    }
    
}

//handler for any clock interrupt processes
void clockHandler(){
    
    //check if anything is idle and if queue is empty
    if ((current_pid == IDLE_PROCESS) && (! checkQueueEmpty(ready_queue)) )
            roundRobinScheduler();
    
    //if there is still a process in the queue then check its quantum
    else if ((clock - current_quantum_start_time) >= QUANTUM){
        process_table[current_pid].CPU_time_used += (clock - current_quantum_start_time);
        process_table[current_pid].state = READY;
        placeIntoQueue(ready_queue,current_pid);
        roundRobinScheduler();
    }
    
}

//handler for any disk interrupt processes
void diskHandler()
{
    //check for any ready processes to add to queue
    process_table[R1].state = READY;
    placeIntoQueue(ready_queue,R1);
    IO_counter--;
    
    printf("Time %d: HANDLED DISK_INTERRUPT for process %d.\n", clock, R1);
    //otherwise choose another process to run with the scheduler
    if (current_pid == IDLE_PROCESS)
        roundRobinScheduler();
    
}

//handler for any keyboard interrupt processes
void keyboardHandler()
{
    printf("Time %d: HANDLED KEYBOARD_INTERRUPT for process %d.\n", clock, R1);
    
     //check for any ready processes to add to queue
    process_table[R1].state = READY;
    placeIntoQueue(ready_queue,R1);
    IO_counter--;
    
    //otherwise choose another process to run with the scheduler
    if (current_pid == IDLE_PROCESS)
        roundRobinScheduler();
    
}

/* This procedure is automatically called when the
 (simulated) machine boots up */
void initialize_kernel()
{
    // Put any initialization code you want here.
    // Remember, the process 0 will automatically be
    // executed after initialization (and current_pid
    // will automatically be set to 0),
    // so the your process table should initially reflect
    // that fact.
    
    // Don't forget to populate the interrupt table
    // (see hardware.h) with the interrupt handlers
    // that you are writing in this file.
    
    // Also, be sure to initialize the semaphores as well
    // as the current_quantum_start_time variable.
    

    //initialize the ready queue
    ready_queue = (PID_QUEUE *) malloc(sizeof(PID_QUEUE));
    ready_queue->head = NULL;
    ready_queue->tail = NULL;
    
    //initialize process table
    process_table[0].CPU_time_used = 0;
    process_table[0].state = RUNNING;
    //set the rest of the processes to UNINITIALIZED
    int j;
    for (j = 1; j < MAX_NUMBER_OF_PROCESSES; j++)
        process_table[j].state = UNINITIALIZED;
    
    
    //initialize semaphores
    int i;
    for(i =0; i<=NUMBER_OF_SEMAPHORES;i++)
    {
        semaphores[i].queue = (PID_QUEUE *) malloc(sizeof(PID_QUEUE));
        semaphores[i].count=INITIAL_SEMAPHORE_VALUE;
        (semaphores[i].queue)->head=NULL;
        (semaphores[i].queue)->tail=NULL;
    }
    //initialize INTERRUPT_TABLE to corresponding method pointers
    INTERRUPT_TABLE[TRAP] = trapHandler;
    INTERRUPT_TABLE[CLOCK_INTERRUPT] = clockHandler;
    INTERRUPT_TABLE[DISK_INTERRUPT] = diskHandler;
    INTERRUPT_TABLE[KEYBOARD_INTERRUPT] = keyboardHandler;
}
