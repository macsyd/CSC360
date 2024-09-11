#include <string.h>
#include <stdio.h>

typedef struct customer customer;

struct customer{
    int id;
    int customer_type;
    int arrival_time;
    int service_time;
    customer* next;
};

typedef struct clerk clerk;

struct clerk {
    int id;
    clerk* next;
};

customer * enqueue(customer* head, customer* new_customer);
customer * dequeue(customer* head);
int check_front(customer* head);
clerk* enqueue_clerk(clerk* head, int clerk_id);
clerk* dequeue_clerk(clerk* head);