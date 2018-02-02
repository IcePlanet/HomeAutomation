/*
 * Test file to see if compilerand everything works
 */

#include "CommonAutomation.h"
#include <time.h>
#include "/usr/local/include/RF24/RF24.h"
#include <stdarg.h>

union frame {
    unsigned long frame;
    struct {
        char target;
        char payload1;
        char payload2;
        char order;
    } d;
};

// LOG SETTINGS
const int log_level = 500; //Log level, the lower number the more priority log, so lower level means less messages
const char* timeformat = "%Y%m%d%H%M%S";

struct queue_node { union frame; struct queue_node* queue_next; };
struct queue_node* queue_front = NULL;
struct queue_node* queue_rear = NULL;

int log_message (int level, int detail, const char* message, ...) {
  struct timeval tv;
  time_t t = time(NULL);
  char tstr [21];
  if (level <= log_level)
  {
    gettimeofday (&tv,NULL);
    t = tv.tv_sec;
    strftime (tstr,20,timeformat,localtime (&t));
    if (detail == 1) {printf ("%s:%d ",tstr,level);} // standard print
    if (detail == 2) {printf ("%s:%d:%lu ",tstr,level,tv.tv_usec);} // include us 
    if (detail == 3) {printf ("%s:%d:%lu ",tstr,level,tv.tv_usec/1000);} // include ms
    if (detail > 10) {printf ("%s:%d:%lu ",tstr,level,tv.tv_usec/detail);} // divide by detail 
    va_list args;
    va_start (args, message);
    vprintf (message, args);
    va_end (args);
  }
}

void uq_enqueue(union frame *x, struct *start, struct *end) {
	struct queue_node* temp =  (struct queue_node*)malloc(sizeof(struct queue_node));
	temp->queue_data = x; 
	temp->queue_next = NULL;
	if(*start == NULL /*&& queue_rear == NULL*/){ *start = *end = temp; return;}
	*end->queue_next = temp;
	*end = temp;
}

unsigned long uq_dequeue(struct *start, struct *end) {
	struct queue_node* temp = *start;
	unsigned long return_value;
	if(*start == NULL) { log_message (10,1,"Error in queue processing, out of bounds\n"); return 0; }
	if(*start == *end) { *start = *end = NULL;}
	else { *start = *start->queue_next;	}
	return_value = temp->queue_data;
	free(temp);
	return return_value;
}

unsigned long uq_item (struct *start, struct *end) {
	if(*start == NULL) { log_message (10,1,"Error in queue processing, out of bounds\n"); return 0; }
	return *start->queue_data;
}

bool uq_info(struct *start, struct *end) {
	if(*start == NULL) { return false; }
	return true;
}

void uq_log(struct *start, struct *end) {
	struct queue_node* temp = *start;
	int i = 0;
	log_message (800,1,"Queue elements: ");
	while(temp != NULL) { log_message (800,0,"%lu -> ",temp->queue_data);	temp = temp->queue_next; i++; }
	log_message (800,0,"NULL (%d)\n",i);
}

void uq_clean(struct *start, struct *end) {
	queue_log ();
	log_message (800,1,"Queue cleaning ");
	while (*start != NULL) { dequeue(); log_message (800,1,"."); }
	log_message (800,1," cleaned\n");
	queue_log ();
}

void main () {
  union frame tmp;
  tmp.frame = 134480385LU
  log_message (200,0,"Frame %lu (134480385), expected target %c (8) payload 1 %c (4) payload 2 (2) orders %c (1)\n", tmp.frame, tmp.d.target, tmp.d.payload1, tmp.d.payload2, tmp.orders);
  tmp.d.target = tmp.d.target | 2;
  log_message (200,0,"Frame %lu (168034817), expected target %c (10) payload 1 %c (4) payload 2 (2) orders %c (1)\n", tmp.frame, tmp.d.target, tmp.d.payload1, tmp.d.payload2, tmp.orders);
  tmp.frame = tmp.frame | 32 ;
  log_message (200,0,"Frame %lu (168034849), expected target %c (10) payload 1 %c (4) payload 2 (2) orders %c (33)\n", tmp.frame, tmp.d.target, tmp.d.payload1, tmp.d.payload2, tmp.orders);

  printf ("Sizeif struct %i and union %i \n",sizeof(struct queue_node), sizeof(struct union frame));
  uq_log (&queue_front, &queue_rear);
  uq_enqueue (
}