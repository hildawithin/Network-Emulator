#include "prog2.h"
#include<string.h>
#include<stdbool.h>

void starttimer(int AorB, float increment);
/******************* Define private structure *****************/
/* buffer at A and B side */
/* implemented as a singly linked list   */
struct QNode
{
  float timestamp;
  struct pkt packet;
  struct QNode* next;
};

struct Queue { 
    struct QNode *front, *rear; 
};

struct QNode* newNode(float k, struct pkt packet);
struct Queue* createQueue();
void enQueue(struct Queue* q, float k, struct pkt packet);
void deQueue(struct Queue* q);

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/
int curseqnuma = 0;
int curacknuma = 1;
int curseqnumb = 0;
int curacknumb = 0;
int nextseqnum = 0; // the next packet A expects to send
int lastseqnum = -1; // the last packet B received
// int curbufferida = 0;
float timerinc = 10.0;
int windowsize = 8;
int winstrt = 0; // sequence number at the start of the window, also at the start of buffer
int wined = 7;
int timerbit = 0; // timer not yet started
// buffersize = 100;
struct Queue* buffera;

int char_checksum(char payload[20])
{
    int sum = 0;
    for (int i = 0; i < 20; i++)
    {
        sum = sum + payload[i];
    }
    return sum;
}

// create a new linked list node
struct QNode* newNode(float k, struct pkt packet) 
{ 
    struct QNode* temp = (struct QNode*)malloc(sizeof(struct QNode)); 
    temp->timestamp = k; 
    temp->packet = packet;
    temp->next = NULL; 
    return temp; 
}

// create an empty queue
struct Queue* createQueue() 
{ 
    struct Queue* q = (struct Queue*)malloc(sizeof(struct Queue)); 
    q->front = q->rear = NULL; 
    return q; 
}

void enQueue(struct Queue* q, float k, struct pkt packet) 
{ 
    // Create a new LL node 
    struct QNode* temp = newNode(k, packet); 
  
    // If queue is empty, then new node is front and rear both 
    if (q->rear == NULL) { 
        q->front = q->rear = temp; 
        return; 
    } 
  
    // Add the new node at the end of queue and change rear 
    q->rear->next = temp; 
    q->rear = temp; 
}

void deQueue(struct Queue* q) 
{ 
    // If queue is empty, return NULL. 
    if (q->front == NULL) 
        return; 
  
    // Store previous front and move front one node ahead 
    struct QNode* temp = q->front; 
  
    q->front = q->front->next; 
    
    // If front becomes NULL, then change rear also as NULL 
    if (q->front == NULL) 
        q->rear = NULL; 
    
    free(temp); 
}

/* called from layer 5, passed the data to be sent to other side */
int A_output(struct msg message)
{
  //(void)message;
  // add msg to buffer
  struct pkt packet;
  packet.seqnum = curseqnuma;
  packet.acknum = curacknuma;
  strcpy(packet.payload, message.data);
  packet.checksum = curseqnuma + curacknuma + char_checksum(message.data);
  enQueue(buffera, time, packet);
  // curbufferida += 1; // increment the buffer count
  curseqnuma += 1; // increment current sequence number
  // check if the packet is in the window // buffera->front->seqnum > wined
  if (packet.seqnum > wined)
  {
    return 0;
  }
  // if it's inside the window, send to B
  tolayer3(0, packet);
  // if timer is not yet started, start the timer
  if(timerbit == 0) 
  {
    starttimer(0, timerinc);
    timerbit = 1;
  }
  return 0;
}

/* called from layer 3, when a packet arrives for layer 4 */
int A_input(struct pkt packet)
{
  // count the number of packets to send to B
  int cnt = 0;
  int strt = 0;
  float t;
  // ignore acks already received
  // printf("Current sequence number: %d\n", curseqnuma);
  // printf("Current ack number: %d\n", curacknuma);
  // printf("Current packet sequence number: %d\n", packet.seqnum);
  // printf("Current packet ack number: %d\n", packet.acknum);
  // printf("buffer front: %f\n", buffera->front->timestamp);
  // printf("buffer front: %d\n", buffera->front->packet.seqnum);
  // printf("buffer front: %d\n", buffera->front->packet.acknum);
  if(buffera->front == NULL) {
    return 0;
  }
  if (packet.acknum == buffera->front->packet.seqnum)
  {
    return 0;
  }
  
  // check checksum
  if (packet.checksum == packet.seqnum + packet.acknum) // + char_checksum(packet.payload)
  {
      nextseqnum = packet.acknum;
      curacknuma = packet.seqnum + 1;
      stoptimer(0);
      
      // tolayer5(0, packet.payload);
      // TODO receive data from B
      // delete acked packets, send new available packets
      while (buffera->front->packet.seqnum < nextseqnum)
      {
        deQueue(buffera);
        cnt++;
        if(buffera->front == NULL){
          break;// prevent segmentation fault caused by dereferencing null pointer
        }
      }
      
      // if the buffer is empty or has no unsent packets, then return
      if (buffera->rear == NULL || buffera->rear->packet.seqnum < nextseqnum)
      {
        timerbit = 0;
        return 0;
      }
      
      t = time - buffera->front->timestamp;
      starttimer(0,timerinc-t);
      
      // send unsent packets in the window, do not resend packets already in transimission
      struct QNode* curnode;
      winstrt = buffera->front->packet.seqnum;
      wined = winstrt + windowsize - 1;
      curnode = buffera->front;
      strt = winstrt + (windowsize - cnt);
      while (curnode->packet.seqnum <= wined)
      {
        if (curnode->packet.seqnum >= strt)
        {
          tolayer3(0, curnode->packet);
        }
        curnode = curnode->next;
        if(curnode == NULL) {
          break; // prevent segmentation fault caused by dereferencing null pointer
        }
      }
  } else
  {
      printf("Wrong checksum for ackowledgement from B.\n");
  }
  
  return 0;
}

/* called when A's timer goes off */
int A_timerinterrupt() {
  float t;
  // resend unacked msg: literally everything in current window
  struct QNode* curnode;
  curnode = buffera->front;
  while (curnode->packet.seqnum <= wined)
  {
    tolayer3(0, curnode->packet);
    curnode->timestamp = time;
    curnode = curnode->next;
    if(curnode == NULL) {break;}
  }
  t = time - buffera->front->timestamp;
  starttimer(0, timerinc-t);
  return 0;
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
int A_init() {
  // initialize buffer struct Queue* 
  buffera = createQueue();
  //buffera = (struct node*)malloc(sizeof(struct node)); 
  return 0;
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
int B_input(struct pkt packet)
{
  // (void)packet;
  // check checksum and send to layer 5
  // printf("Current sequence number: %d\n", curseqnumb);
  // printf("Current ack number: %d\n", curacknumb);
  // printf("Current packet sequence number: %d\n", packet.seqnum);
  // printf("Current packet ack number: %d\n", packet.acknum);
  if (packet.checksum == packet.seqnum + packet.acknum + char_checksum(packet.payload))
  {
    // right checksum
    // if already received, then send ack, not do anything with the data
    struct pkt ack;
    if (packet.seqnum <= lastseqnum || packet.seqnum > lastseqnum + 1)
    {
      ack.acknum = curacknumb;
      ack.seqnum = curseqnumb;
      ack.checksum = curacknumb + curseqnumb;
      tolayer3(1, ack);
      printf("Ack %d\n", curacknumb);
    } else if (packet.seqnum == lastseqnum + 1)
    {
      curseqnumb = packet.acknum;
      curacknumb = packet.seqnum + 1;
      tolayer5(1, packet.payload);
      ack.acknum = curacknumb;
      ack.seqnum = curseqnumb;
      ack.checksum = curacknumb + curseqnumb;
      tolayer3(1, ack);
      printf("Ack %d\n", curacknumb);
      lastseqnum = packet.seqnum;
    }
    // printf("Receiving %d\n", packet.seqnum);
  } else
  {
    // wrong checksum, do nothing
    printf("Wrong checksum for data transmitted from A.\n");
  }
  return 0;
}

/* called when B's timer goes off */
int B_timerinterrupt() {return 0;}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
int B_init() {
  lastseqnum = curseqnuma-1;
  return 0;
}

int TRACE = 2;   /* for my debugging */
int nsim = 0;    /* number of messages from 5 to 4 so far */
int nsimmax = 50; /* number of msgs to generate, then stop */
float time = 0.000;
float lossprob = 0.2;    /* probability that a packet is dropped  */
float corruptprob = 0.2; /* probability that one bit is packet is flipped */
float lambda = 10;      /* arrival rate of messages from layer 5 */
int ntolayer3;     /* number sent into layer 3 */
int nlost;         /* number lost in media */
int ncorrupt;      /* number corrupted by media*/

int main()
{
  struct event * eventptr;
  struct msg msg2give;
  struct pkt pkt2give;

  int i, j;

  init();
  A_init();
  B_init();

  for (;; ) {
    eventptr = evlist; /* get next event to simulate */
    if (NULL == eventptr) {
      goto terminate;
    }
    evlist = evlist->next; /* remove this event from event list */
    if (evlist != NULL) {
      evlist->prev = NULL;
    }
    if (TRACE >= 2) {
      printf("\nEVENT time: %f,", eventptr->evtime);
      printf("  type: %d", eventptr->evtype);
      if (eventptr->evtype == 0) {
        printf(", timerinterrupt  ");
      } else if (eventptr->evtype == 1) {
        printf(", fromlayer5 ");
      } else {
        printf(", fromlayer3 ");
      }
      printf(" entity: %d\n", eventptr->eventity);
    }
    time = eventptr->evtime; /* update time to next event time */
    if (nsim == nsimmax) {
      break; /* all done with simulation */
    }
    if (eventptr->evtype == FROM_LAYER5) {
      generate_next_arrival(); /* set up future arrival */
      /* fill in msg to give with string of same letter */
      j = nsim % 26;
      for (i = 0; i < 20; i++) {
        msg2give.data[i] = 97 + j;
      }
      if (TRACE > 2) {
        printf("          MAINLOOP: data given to student: ");
        for (i = 0; i < 20; i++) {
          printf("%c", msg2give.data[i]);
        }
        printf("\n");
      }
      nsim++;
      if (eventptr->eventity == A) {
        A_output(msg2give);
      }
    } else if (eventptr->evtype == FROM_LAYER3) {
      pkt2give.seqnum = eventptr->pktptr->seqnum;
      pkt2give.acknum = eventptr->pktptr->acknum;
      pkt2give.checksum = eventptr->pktptr->checksum;
      for (i = 0; i < 20; i++) {
        pkt2give.payload[i] = eventptr->pktptr->payload[i];
      }
      if (eventptr->eventity == A) { /* deliver packet by calling */
        A_input(pkt2give);           /* appropriate entity */
      } else {
        B_input(pkt2give);
      }
      free(eventptr->pktptr); /* free the memory for packet */
    } else if (eventptr->evtype == TIMER_INTERRUPT) {
      if (eventptr->eventity == A) {
        A_timerinterrupt();
      } else {
        B_timerinterrupt();
      }
    } else {
      printf("INTERNAL PANIC: unknown event type \n");
    }
    free(eventptr);
  }
  return 0;

terminate:
  printf(
    " Simulator terminated at time %f\n after sending %d msgs from layer5\n",
    time, nsim);
  return 0;
}

void init() /* initialize the simulator */
{
  int i;
  float sum, avg;

  printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");
  printf("Enter the number of messages to simulate: ");
  scanf("%d", &nsimmax);
  printf("Enter  packet loss probability [enter 0.0 for no loss]:");
  scanf("%f", &lossprob);
  printf("Enter packet corruption probability [0.0 for no corruption]:");
  scanf("%f", &corruptprob);
  printf("Enter average time between messages from sender's layer5 [ > 0.0]:");
  scanf("%f", &lambda);
  printf("Enter TRACE:");
  scanf("%d", &TRACE);

  srand(rand_seed); /* init random number generator */
  sum = 0.0;   /* test random number generator for students */
  for (i = 0; i < 1000; i++) {
    sum = sum + jimsrand(); /* jimsrand() should be uniform in [0,1] */
  }
  avg = sum / 1000.0;
  if (avg < 0.25 || avg > 0.75) {
    printf("It is likely that random number generation on your machine\n");
    printf("is different from what this emulator expects.  Please take\n");
    printf("a look at the routine jimsrand() in the emulator code. Sorry. \n");
    exit(0);
  }

  ntolayer3 = 0;
  nlost = 0;
  ncorrupt = 0;

  time = 0.0;              /* initialize time to 0.0 */
  generate_next_arrival(); /* initialize event list */
}

/****************************************************************************/
/* jimsrand(): return a float in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
float jimsrand()
{
  double mmm = INT_MAX;         /* largest int  - MACHINE DEPENDENT!!!!!!!!   */
  float x;                      /* individual students may need to change mmm */
  x = rand_r(&rand_seed) / mmm; /* x should be uniform in [0,1] */
  return x;
}

/************ EVENT HANDLINE ROUTINES ****************/
/*  The next set of routines handle the event list   */
/*****************************************************/
void generate_next_arrival()
{
  double x;
  struct event * evptr;

  if (TRACE > 2) {
    printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");
  }

  x = lambda * jimsrand() * 2; /* x is uniform on [0,2*lambda] */
  /* having mean of lambda        */
  evptr = (struct event *)malloc(sizeof(struct event));
  evptr->evtime = time + x;
  evptr->evtype = FROM_LAYER5;
  if (BIDIRECTIONAL && (jimsrand() > 0.5)) {
    evptr->eventity = B;
  } else {
    evptr->eventity = A;
  }
  insertevent(evptr);
}

void insertevent(struct event * p)
{
  struct event * q, * qold;

  if (TRACE > 2) {
    printf("            INSERTEVENT: time is %lf\n", time);
    printf("            INSERTEVENT: future time will be %lf\n", p->evtime);
  }
  q = evlist;      /* q points to header of list in which p struct inserted */
  if (NULL == q) { /* list is empty */
    evlist = p;
    p->next = NULL;
    p->prev = NULL;
  } else {
    for (qold = q; q != NULL && p->evtime > q->evtime; q = q->next) {
      qold = q;
    }
    if (NULL == q) { /* end of list */
      qold->next = p;
      p->prev = qold;
      p->next = NULL;
    } else if (q == evlist) { /* front of list */
      p->next = evlist;
      p->prev = NULL;
      p->next->prev = p;
      evlist = p;
    } else { /* middle of list */
      p->next = q;
      p->prev = q->prev;
      q->prev->next = p;
      q->prev = p;
    }
  }
}

void printevlist()
{
  struct event * q;
  printf("--------------\nEvent List Follows:\n");
  for (q = evlist; q != NULL; q = q->next) {
    printf("Event time: %f, type: %d entity: %d\n", q->evtime, q->evtype,
      q->eventity);
  }
  printf("--------------\n");
}

/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
void stoptimer(int AorB)
{
  struct event * q;

  if (TRACE > 2) {
    printf("          STOP TIMER: stopping timer at %f\n", time);
  }

  for (q = evlist; q != NULL; q = q->next) {
    if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB)) {
      /* remove this event */
      if (NULL == q->next && NULL == q->prev) {
        evlist = NULL;              /* remove first and only event on list */
      } else if (NULL == q->next) { /* end of list - there is one in front */
        q->prev->next = NULL;
      } else if (q == evlist) { /* front of list - there must be event after */
        q->next->prev = NULL;
        evlist = q->next;
      } else { /* middle of list */
        q->next->prev = q->prev;
        q->prev->next = q->next;
      }
      free(q);
      return;
    }
  }
  printf("Warning: unable to cancel your timer. It wasn't running.\n");
}

void starttimer(int AorB, float increment)
{
  struct event * q;
  struct event * evptr;

  if (TRACE > 2) {
    printf("          START TIMER: starting timer at %f\n", time);
  }

  /* be nice: check to see if timer is already started, if so, then  warn */
  for (q = evlist; q != NULL; q = q->next) {
    if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB)) {
      printf("Warning: attempt to start a timer that is already started\n");
      return;
    }
  }

  /* create future event for when timer goes off */
  evptr = (struct event *)malloc(sizeof(struct event));
  evptr->evtime = time + increment;
  evptr->evtype = TIMER_INTERRUPT;
  evptr->eventity = AorB;
  insertevent(evptr);
}

/************************** TOLAYER3 ***************/
void tolayer3(int AorB, struct pkt packet)
{
  struct pkt * mypktptr;
  struct event * evptr, * q;
  float lastime, x;
  int i;

  ntolayer3++;

  /* simulate losses: */
  if (jimsrand() < lossprob) {
    nlost++;
    if (TRACE > 0) {
      printf("          TOLAYER3: packet being lost\n");
    }
    return;
  }

  /*
   * make a copy of the packet student just gave me since he/she may decide
   * to do something with the packet after we return back to him/her
   */

  mypktptr = (struct pkt *)malloc(sizeof(struct pkt));
  mypktptr->seqnum = packet.seqnum;
  mypktptr->acknum = packet.acknum;
  mypktptr->checksum = packet.checksum;
  for (i = 0; i < 20; ++i) {
    mypktptr->payload[i] = packet.payload[i];
  }
  if (TRACE > 2) {
    printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
      mypktptr->acknum, mypktptr->checksum);
    for (i = 0; i < 20; ++i) {
      printf("%c", mypktptr->payload[i]);
    }
    printf("\n");
  }

  /* create future event for arrival of packet at the other side */
  evptr = (struct event *)malloc(sizeof(struct event));
  evptr->evtype = FROM_LAYER3;      /* packet will pop out from layer3 */
  evptr->eventity = (AorB + 1) & 1; /* event occurs at other entity */
  evptr->pktptr = mypktptr;         /* save ptr to my copy of packet */

  /*
   * finally, compute the arrival time of packet at the other end.
   * medium can not reorder, so make sure packet arrives between 1 and 10
   * time units after the latest arrival time of packets
   * currently in the medium on their way to the destination
   */

  lastime = time;
  for (q = evlist; q != NULL; q = q->next) {
    if ((q->evtype == FROM_LAYER3 && q->eventity == evptr->eventity)) {
      lastime = q->evtime;
    }
  }
  evptr->evtime = lastime + 1 + 9 * jimsrand();

  /* simulate corruption: */
  if (jimsrand() < corruptprob) {
    ncorrupt++;
    if ((x = jimsrand()) < .75) {
      mypktptr->payload[0] = 'Z'; /* corrupt payload */
    } else if (x < .875) {
      mypktptr->seqnum = 999999;
    } else {
      mypktptr->acknum = 999999;
    }
    if (TRACE > 0) {
      printf("          TOLAYER3: packet being corrupted\n");
    }
  }

  if (TRACE > 2) {
    printf("          TOLAYER3: scheduling arrival on other side\n");
  }
  insertevent(evptr);
}

void tolayer5(int AorB, const char * datasent)
{
  (void)AorB;
  int i;
  if (TRACE > 2) {
    printf("          TOLAYER5: data received: ");
    for (i = 0; i < 20; i++) {
      printf("%c", datasent[i]);
    }
    printf("\n");
  }
}
