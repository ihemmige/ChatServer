#include <sstream>
#include "csapp.h"
#include "message.h"
#include "connection.h"
#include <iostream>
#include <string.h>

using std::to_string;
using std::cerr;
using std::string;
using std::stringstream;

Connection::Connection()
  : m_fd(-1)
  , m_last_result(SUCCESS) {
}

Connection::Connection(int fd)
  : m_fd(fd)
  , m_last_result(SUCCESS) {
  // call rio_readinitb to initialize the rio_t object
  rio_readinitb(&m_fdbuf, m_fd);
}

void Connection::connect(const std::string &hostname, int port) {
  // call open_clientfd to connect to the server
  m_fd = open_clientfd(hostname.c_str(), to_string(port).c_str()); // convert to c char arrays instead of c++ string
  if (m_fd < 0) { // the open failed, output error
    cerr << "Failed to open clientfd";
  } else { // otherwise initialize the rio_t object using rio_readinitb
    rio_readinitb(&m_fdbuf, m_fd);
  }
}

Connection::~Connection() {
  if(is_open()){ // close the socket if it is open
    close();
  }
}

bool Connection::is_open() const {
  return m_fd >= 0; // connection is open as long as the m_fd is non-negative
}

void Connection::close() {
  if (is_open()) { // use is_open helper function
    Close(m_fd);
    m_fd = -1; // set the m_fd negative so we know it is closed in future
  }
}

bool Connection::send(const Message &msg) {
  // send a message
  const string message = msg.tag + ":" + msg.data + "\n"; // format message correctly
  ssize_t status = rio_writen(m_fd, message.c_str(), message.length()); // use rio_writen to send the message

  // return true if successful, false if not
  // make sure that m_last_result is set appropriately
  if (status < 0) { // message was not successfully sent
    m_last_result = EOF_OR_ERROR;
    return false;
  } else { // the message was successfully sent
    m_last_result = SUCCESS;
    return true;
  }
}

bool Connection::receive(Message &msg) {
  // Receive a message, storing its tag and data in msg
  char buf[Message::MAX_LEN + 1]; // one extra char for null terminator

  // return true if successful, false if not
  // make sure that m_last_result is set appropriately

  ssize_t status = rio_readlineb(&m_fdbuf, buf, Message::MAX_LEN); // use rio_readlineb to receive a message
  
  if (status < 0) { // there was a failure to receive, so return false
    m_last_result = INVALID_MSG;
    return false;
  }

  stringstream sstream(buf);
  getline(sstream, msg.tag, ':'); // grab tag up to the colon
  getline(sstream, msg.data); // then, data after the colon

  // return true as the reception was successful
  m_last_result = SUCCESS;
  return true;
}