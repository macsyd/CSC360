#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

/* Enqueue customer to back of queue */
customer * enqueue(customer* head, customer* new_customer) {
    // Add to back of queue
    customer* curr = head;
    if(curr == NULL) {
        head = new_customer;
        new_customer->next = NULL;
        return head;
    }
    while(curr->next != NULL) {
        curr = curr->next;
    }
    curr->next = new_customer;
    new_customer->next = NULL;
    return head;
}

/* Dequeue customer from front of queue */
customer * dequeue(customer* head) {
    // Remove from front
    customer* temp = head;
    head = temp->next;
    return head;
}

/* Check the ID of the customer at the front of the queue */
int check_front(customer* head) {
    // Return id of first element in queue
    // If queue is empty, return -1
    if(head == NULL) return -1;
    return head->id;
}

/* Enqueue clerk to back of queue */
clerk* enqueue_clerk(clerk* head, int clerk_id) {
    clerk* new_clerk;
    new_clerk = (clerk*) malloc(sizeof(clerk));
    if(new_clerk == NULL) {
        fprintf(stderr, "Error: Malloc of size %ld failed.\n", sizeof(clerk));
    }
    new_clerk->id = clerk_id;
    new_clerk->next = NULL;

    if(head == NULL) {
        head = new_clerk;
    } else {
        clerk* curr = head;
        while(curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = new_clerk;
    }
    return head;
}

/* Dequeue next clerk from front of queue */
clerk* dequeue_clerk(clerk* head) {
    clerk* temp = head;
    head = temp->next;
    free(temp);
    return head;
}