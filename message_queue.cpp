#include "message_queue.h"
#include <ctime>
#include "message.h"
#include "guard.h"

MessageQueue::MessageQueue() {
  pthread_mutex_init(&m_lock, nullptr); // initialize the mutex
  sem_init(&m_avail, 0, 0); // initialize the semaphore
}

MessageQueue::~MessageQueue() {
  pthread_mutex_destroy(&m_lock); // destroy the mutex
  sem_destroy(&m_avail); // destroy the semaphore
}

void MessageQueue::enqueue(Message *msg) {
  Guard guard(m_lock); // ensure only one thread accesses the queue at a time
  m_messages.push_back(new Message((*msg).tag, (*msg).data)); // add the message to the back of the queue
  sem_post(&m_avail); // notifies any waiting thread that a message is available
}

Message *MessageQueue::dequeue() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts); // get the current time
  // compute a time one second in the future
  ts.tv_sec += 1;
  Message *msg = nullptr;
  if (sem_timedwait(&m_avail, &ts) == -1) // wait up to 1 second for a message
    return msg; // if nothing comes, return nullptr
  //otherwise a message is available
  Guard guard(m_lock); // ensure only one thread accesses the queue at a time
  if (!m_messages.empty()) { // if the queue isn't empty, then store and remove the first message
    msg = m_messages.front();
    m_messages.pop_front();
  } 
  return msg; // return the message, which will be nullptr if there was no message on the queue
}