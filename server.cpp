#include <pthread.h>
#include <iostream>
#include <memory>
#include "message.h"
#include "connection.h"
#include "user.h"
#include "room.h"
#include "guard.h"
#include "server.h"

using std::cerr;
using std::string;

////////////////////////////////////////////////////////////////////////
// Server implementation data types
////////////////////////////////////////////////////////////////////////

// encapsulates a connection with a server that it belongs to
struct Info
{
  Server *server;
  Connection *connection;
  ~Info()
  {
    delete connection;
  }
};

////////////////////////////////////////////////////////////////////////
// Client thread functions
////////////////////////////////////////////////////////////////////////
// helper function for receiver to communicate with the server
void r_chat(User *u, Server *s, Connection *c)
{
  Message msg = Message();
  if (!(*c).receive(msg)) { // if message reception failed, will send appropriate error message and return
    if ((*c).get_last_result() == Connection::INVALID_MSG) // case when a message was received but it was invalid
      (*c).send(Message(TAG_ERR, "Invalid message received"));
    else // case when no message was received
      (*c).send(Message(TAG_ERR, "No message received"));
    return;
  }

  Room *rm = nullptr;
  // if we get to here, the message was successfully received, so must handle the possible tags for a receiver
  // the only tag that is acceptable (at the beginning) is the join tag
  if (msg.tag == TAG_JOIN) {
    rm = (*s).find_or_create_room(msg.data);  // find the room if it exists or create otherwise
    (*rm).add_member(u); // add this receiver into the room
    if (!(*c).send(Message(TAG_OK, "Successfully joined room")))
      return; // return if confirmation of room join could not be sent
  }
  else // if the tag is not a join, then receiver is not able to join a room
    (*c).send(Message(TAG_ERR, "Invalid message as receiver has not joined a room"));

  // loop indefinitely relaying messages to the receiver now that it is in a room
  while (true) {
    Message *message = (*u).mqueue.dequeue(); // grab the first message from the message queue
    
    // if a message was successfully dequeued, then handle it
    // need this case because dequeue might not return a message every time
    if (message != nullptr) { 
      // attempt to send the message to the receiver
      if (!(*c).send(*message)) { // if it fails, then break out and can return
        delete message;
        break;
      }
      delete message; // delete the message regardless
    }
  }
  (*rm).remove_member(u); // before returning, make sure to remove the receiver from the room it was in
}

// helper function for sender to communicate with the server
void s_chat(User *u, Server *s, Connection *c)
{
  Room *rm = nullptr; // local variable that will store which room this sender is a part of
  // loop infinitely until the sender quits or leaves the room
  while (true) {
    Message msg;
    if (!(*c).receive(msg)) { // if message reception failed
      // case when a message was received but it was invalid, so have to return
      if ((*c).get_last_result() == Connection::EOF_OR_ERROR || (*c).get_last_result() == Connection::INVALID_MSG) {
        (*c).send(Message(TAG_ERR, "Invalid message received"));
        return;
      }
      // case when no message was received, so can continue
      else
        (*c).send(Message(TAG_ERR, "No message received"));
    }
    else { // the message was successfully received, so need to handle based on its tag
      if (msg.tag == TAG_ERR) { // if an error message was received, need to return right away
        cerr << msg.data;
        return;
      }
      else if (msg.data.length() == Message::MAX_LEN) // case where the message received was too long
        (*c).send(Message(TAG_ERR, "Message is too long"));

      else if (msg.tag == TAG_QUIT) { // case where the sender wants to quit
        (*c).send(Message(TAG_OK, "Quitting now"));
        return;
      }
      else if (rm == nullptr) { // if the sender is not in a room, then they need to join one to send any message
        // nullptr for rm would indicate that the user is not in a room
        if (msg.tag != TAG_JOIN) // that means if this message wasn't a join request, they have violated the use rules
          (*c).send(Message(TAG_ERR, "Not a member of a room, so can't send message"));
        else { // the user did try to join a room, so we add them to that room
          rm = (*s).find_or_create_room(msg.data); // find the room if it exists or create otherwise
          (*rm).add_member(u); // add this sender into the room
          if (!(*c).send(Message(TAG_OK, "Successfully joined room")))
            return; // return if confirmation of room join could not be sent
        }
      }
      else if (msg.tag == TAG_SENDALL) { // case where sender wants to send a message to everyone in the room
        (*rm).broadcast_message((*u).username, msg.data); // send the message first using broadcast
        if (!(*c).send(Message(TAG_OK, "Message broadcasted in room")))
          return; // return if confirmation of message send could not be sent
      }
      else if (msg.tag == TAG_LEAVE) { // case where the sender wants to leave their room (not quit though)
        if (rm == nullptr) // if the user wants to leave a room, but aren't actually in a room
          (*c).send(Message(TAG_ERR, "Sender not in a room!"));
        else { // the sender is in a room, so can leave from there
          (*rm).remove_member(u); // remove the sender from the room
          rm = nullptr; // sender now no longer has a room
          if (!(*c).send(Message(TAG_OK, "Successfully left room")))
            return;  // return if confirmation of leaving could not be sent
        }
      }
      else if (msg.tag == TAG_JOIN) { // case where sender wants to join a room, but they're already in a different room
        (*rm).remove_member(u); // first remove them from their prior room
        rm = (*s).find_or_create_room(msg.data); // find the room if it exists or create otherwise
        (*rm).add_member(u); // add this sender into the room
        if (!(*c).send(Message(TAG_OK, "Successfully joined new room")))
          return;  // return if confirmation of room join could not be sent
      }      
      else { // if we get to here, then the tag was not valid
        if (!(*c).send(Message(TAG_ERR, "Invalid message tag"))) // send message that tag was invalid
          return; // return if confirmation of invalid tag couldn't be sent
      }
    }
  }
  // when this chat between sender and server ends, delete the user and connection
  // don't delete the server because it could still be communicating with other users
  delete c;
  delete u;
}

namespace
{
  void *worker(void *arg) {
    pthread_detach(pthread_self());
    // use a static cast to convert arg from a void* to Info object that holds connection and server
    struct Info *_info = (Info *)arg; 
    std::unique_ptr<Info> info(_info);

    Message login = Message();

    // if unable to receive the login message, handle the two possible cases
    if (!(*info).connection->receive(login)) {
      // invalid message
      if ((*info).connection->get_last_result() == Connection::INVALID_MSG)
        (*info).connection->send(Message(TAG_ERR, "Invalid message received"));
      // didn't receive a message
      else
        (*info).connection->send(Message(TAG_ERR, "No message received"));
      return nullptr;
    }

    // if the user sends a FIRST message that isn't a login message (sender or receiver)
    if (!(login.tag == TAG_RLOGIN || login.tag == TAG_SLOGIN)) {
      (*info).connection->send(Message(TAG_ERR, "Sender/Receiver must first log in"));  // send message that user did not login
      return nullptr;
    }
    // a login message was sent, so can log the user in
    else {
      if (!(*info).connection->send(Message(TAG_OK, "Logged in as: " + login.data)))  // send message that user logged in
        return nullptr; // if the confirmation fails, return
    }
    // store the username and create the user since the login was valid
    string username = login.data;
    User *user = new User(login.data);

    // Facilitate communication between the user and the server

    // if user is a receiver, then use r_chat helper function
    if (login.tag == TAG_RLOGIN)
      r_chat(user, (*info).server, (*info).connection);
    // if user is a sender, then user s_chat helper function
    else
      s_chat(user, (*info).server, (*info).connection);
    return nullptr;
  }
}

////////////////////////////////////////////////////////////////////////
// Server member function implementation
////////////////////////////////////////////////////////////////////////

Server::Server(int port)
    : m_port(port), m_ssock(-1)
{
  pthread_mutex_init(&m_lock, nullptr); // initialize mutex
}

Server::~Server()
{
  pthread_mutex_destroy(&m_lock); // destroy mutex
}

bool Server::listen()
{
  m_ssock = open_listenfd(std::to_string(m_port).c_str()); // attempt to open the socket
  if (m_ssock < 0) { // if failed, then return false
    cerr << "Server socket not opened";
    return false;
  }
  return true; // socket successfully opened, so return true
}

void Server::handle_client_requests()
{ 
  // infinite loop accepting new clients, connecting with the clients and starting new threads for each
  while (true) {
    int client = accept(m_ssock, nullptr, nullptr);
    // if the client's file descriptor is negative, the connection failed
    if (client < 0) {
      cerr << "Unable to accept client connection";
      return;
    }
    //otherwise, the connection was successful, so create info for the client
    struct Info *info = new Info();
    (*info).server = this;
    (*info).connection = new Connection(client);

    // connection was successful, so also create a new thread for the client
    pthread_t thread;
    if (pthread_create(&thread, NULL, worker, info) != 0) { // if creating the thread fails, then return
      cerr << "Failed to create thread";
      return;
    }
  }
}

Room *Server::find_or_create_room(const std::string &room_name)
{
  Guard guard(m_lock); // ensure synchronization
  auto room = m_rooms.find(room_name); // try to find the room with given room_name
  // if the room wasn't found, then need to create it
  if (room == m_rooms.end()) {
    m_rooms[room_name] = new Room(room_name); // create and add the new room to the existing list for the server
    return m_rooms[room_name];
  }
  return (*room).second; // if the room was found, can just return it
}