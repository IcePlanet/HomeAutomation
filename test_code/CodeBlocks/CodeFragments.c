/*
 * Test file to see if compilerand everything works
 */

#include "CommonAutomation.h"
#include <time.h>
#include "/usr/local/include/RF24/RF24.h"
#include <stdarg.h>

union Frame {
    unsigned long frame;
    struct {
        unsigned char order;
        unsigned char payload2;
        unsigned char payload1;
        unsigned char target;
    } d;
};

// LOG SETTINGS
const int log_level = 1000; //Log level, the lower number the more priority log, so lower level means less messages
const char* timeformat = "%Y%m%d%H%M%S";

struct queue_node { union Frame f; struct queue_node* queue_next; };
struct queue_node* queue_front = NULL;
struct queue_node* queue_rear = NULL;

struct QueueFrames { queue_node* start; queue_node* end; bool ok; union Frame t; } send_queue ;

union Frame empty_frame;

void log_message (int level, int detail, const char* message, ...) {
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

bool uq_init (struct QueueFrames *x) {
	(*x).start = (*x).end = NULL;
  (*x).ok = true ;
  return (*x).ok ;
}

bool uq_enqueue (struct QueueFrames *x) {
	struct queue_node* temp =  (struct queue_node*)malloc(sizeof(struct queue_node));
	(*temp).f = (*x).t; 
	temp->queue_next = NULL;
  (*x).ok = true;
	if((*x).start == NULL /*&& queue_rear == NULL*/){ (*x).start = (*x).end = temp; return (*x).ok;}
	(*(*x).end).queue_next = temp;
	(*x).end = temp;
  return (*x).ok ;
}

bool uq_dequeue (struct QueueFrames *x) {
  struct queue_node* temp;
  (*x).t = empty_frame;
	if((*x).start == NULL) { log_message (10,1,"Error in queue processing, out of bounds\n"); (*x).ok = false; return (*x).ok; }
  (*x).t = (*(*x).start).f ;
  temp = (*x).start;
	if((*x).start == (*x).end) { (*x).start = (*x).end = NULL;}
	else { (*x).start = (*(*x).start).queue_next;	}
	free(temp);
  (*x).ok = true;
	return (*x).ok;
}

bool uq_item (struct QueueFrames *x) {
	if((*x).start == NULL) { log_message (10,1,"Error in queue processing, out of bounds\n"); (*x).ok = false; return (*x).ok; }
  (*x).t = (*(*x).start).f ;
  (*x).ok = true;
	return (*x).ok;
}

bool uq_info (struct QueueFrames *x) {
	if((*x).start == NULL) { return false; }
	return true;
}

void uq_log (struct QueueFrames *x) {
  struct queue_node* temp = (*x).start;
	int i = 0;
	log_message (800,1,"Queue elements: ");
	while(temp != NULL) { log_message (800,0,"%lu -> ",temp->f.frame);	temp = temp->queue_next; i++; }
	log_message (800,0,"NULL (%d)\n",i);
}

void uq_clean (struct QueueFrames *x) {
	uq_log (x);
	log_message (800,1,"Queue cleaning ");
	while ((*x).start != NULL) { uq_dequeue(x); log_message (800,1,"."); }
	log_message (800,1," cleaned\n");
	uq_log (x);
}

int main () {
  union Frame tmp;
  empty_frame.frame = 0;
  tmp.frame = 134480385LU; 
  log_message (200,0,"Frame %lu (134480385), expected target %u (8) payload 1 %u (4) payload 2 %u (2) orders %u (1)\n", tmp.frame, tmp.d.target, tmp.d.payload1, tmp.d.payload2, tmp.d.order);
  tmp.d.target = tmp.d.target | 2;
  log_message (200,0,"Frame %lu (168034817), expected target %u (10) payload 1 %u (4) payload 2 %u (2) orders %u (1)\n", tmp.frame, tmp.d.target, tmp.d.payload1, tmp.d.payload2, tmp.d.order);
  tmp.frame = tmp.frame | 32 ;
  log_message (200,0,"Frame %lu (168034849), expected target %u (10) payload 1 %u (4) payload 2 %u (2) orders %u (33)\n", tmp.frame, tmp.d.target, tmp.d.payload1, tmp.d.payload2, tmp.d.order);
  tmp.frame = tmp.frame | 8 ;
  log_message (200,0,"Frame %lu (168034857), expected target %u (10) payload 1 %u (4) payload 2 %u (2) orders %u (41)\n", tmp.frame, tmp.d.target, tmp.d.payload1, tmp.d.payload2, tmp.d.order);
  printf ("Sizeof long %i  \n",sizeof(unsigned long));
  printf ("Sizeof struct %i and union %i queue %i\n",sizeof(struct queue_node), sizeof(union Frame), sizeof(struct QueueFrames));
  uq_log (&send_queue);
  send_queue.t = tmp;
  printf ("Enqueue %lu  \n",send_queue.t.frame);
  uq_enqueue (&send_queue);
  printf ("Log queue  \n");
  uq_log (&send_queue);
  printf ("deQueue  \n");
  uq_dequeue (&send_queue);
  printf ("de_queed: %lu \n",send_queue.t.frame);
  printf ("deQueue2  \n");
  uq_dequeue (&send_queue);
  printf ("de_queed2: %lu \n",send_queue.t.frame);
  send_queue.t.frame = 1;
  printf ("Enqueue %lu  \n",send_queue.t.frame);
  uq_enqueue (&send_queue);
  printf ("Log queue  \n");
  uq_log (&send_queue);
  send_queue.t.frame = 2;
  printf ("Enqueue %lu  \n",send_queue.t.frame);
  uq_enqueue (&send_queue);
  printf ("Log queue  \n");
  uq_log (&send_queue);
  send_queue.t.frame = 3;
  printf ("Enqueue %lu  \n",send_queue.t.frame);
  uq_enqueue (&send_queue);
  printf ("Log queue  \n");
  uq_log (&send_queue);
  printf ("deQueue  \n");
  uq_dequeue (&send_queue);
  printf ("de_queed: %lu \n",send_queue.t.frame);
  printf ("Clean queue \n");
  uq_clean (&send_queue);
  return 0;
}