#include "room.h"
#include "guard.h"
#include "message.h"
#include "user.h"
#include "message_queue.h"

Room::Room(const std::string &room_name)
  : room_name(room_name) {
  pthread_mutex_init(&lock, nullptr); // initialize the mutex
}

Room::~Room() {
  pthread_mutex_destroy(&lock); // destroy the mutex
}

void Room::add_member(User *user) {
  Guard guard(lock); // ensures the list can't be modified simultaneously by multiple threads
  members.insert(user);
}

void Room::remove_member(User *user) {
  Guard guard(lock);  // ensures the list can't be modified simultaneously by multiple threads
  members.erase(user);
}

void Room::broadcast_message(const std::string &sender_username, const std::string &message_text) {
  Guard guard(lock); // ensures broadcasting and adding/removing members aren't simultaneous
  // iterate through all the users in the room
  for(auto each: members){
   if(sender_username != (*each).username) // only if the user isn't the original sender of the message
      each->mqueue.enqueue(new Message(TAG_DELIVERY, get_room_name() + ":" + sender_username + ":" + message_text)); // send message
  }
}