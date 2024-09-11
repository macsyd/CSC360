#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "linked_list.h"


/*
* Create a new node and allocate memory, 
* and add node to linked list.
*/
Node * add_newNode(Node* head, pid_t new_pid, char * new_path){
	Node* temp;
	temp = (Node*) malloc(sizeof(Node));

	if(temp == NULL) {
		fprintf(stderr, "malloc of size %ld failed.\n", sizeof(Node));
	}

	char* path;
	size_t n = sizeof(char)*20;
	path = (char*) malloc(n);
	if(path == NULL) {
		fprintf(stderr, "malloc of size %ld failed.\n", n);
	}
	strncpy(path, new_path, strlen(new_path));

	temp->pid = new_pid;
	temp->path = path;
	temp->next = NULL;

	if(head == NULL) {
		head = temp;
		return head;
	}
	Node* cur = head;
	while(cur->next != NULL) {
		cur = cur->next;
	}
	cur->next = temp;

	return head;
}


/*
* Delete a node from the linked list
* and free the memory.
*/
Node * deleteNode(Node* head, pid_t pid){
	Node* cur = head;
	if(cur == NULL) {
		return NULL;
	}
	if(cur->pid == pid) {
		// Node* tmp = cur;
		if(cur->next == NULL) {
			head = NULL;
		} else {
			head = cur->next;
		}
		freeNode(cur);
		return head;
	}
	while(cur->next != NULL) {
		if(cur->next->pid == pid) {
			Node* tmp = cur->next;
			cur->next = cur->next->next;
			freeNode(tmp);
			return head;
		}
		cur = cur->next;
	}
	return NULL;
}


/*
* Print out all nodes in the linked list.
*/
void printList(Node *node){
	int count = 0;
	Node* cur = node;
	while(cur != NULL) {
		printf("%d: %s \n", cur->pid, cur->path);
		cur = cur->next;
		count++;
	}
	printf("Total background jobs: %d \n", count);
}


/*
* If a node with this PID exists, return 1.
* Else, return 0.
*/
int PifExist(Node *node, pid_t pid){
	Node* cur = node;
	while(cur != NULL) {
		if(cur->pid == pid) {
			return 1;
		}
		cur = cur->next;
	}
  	return 0;
}


/*
* Helper function to free all
* memory in a node.
*/
void freeNode(Node* cur) {
	free(cur->path);
	free(cur);
}


/*
* Free all the nodes in the linked list at once.
*/
void free_allNodes(Node* head) {
	Node* cur = head;
	while(cur != NULL) {
		Node* tmp = cur;
		cur = cur->next;
		freeNode(tmp);
	}
}
