#include "client_util.h"
#include "connection.h"
#include "csapp.h"
#include "message.h"
#include <iostream>
#include <string>

using std::cerr;
using std::cin;
using std::getline;
using std::string;
using std::stoi;

int main(int argc, char **argv) {
  if (argc != 4) {
    cerr << "Usage: ./sender [server_address] [port] [username]\n";
    return 1;
  }

  // initialize the client variables based on input args
  string server_hostname = argv[1];
  int server_port = stoi(argv[2]);
  string username = argv[3];

  // connect to the server
  Connection connection;
  connection.connect(server_hostname, server_port);
  if (!connection.is_open()) { // if failed, then exit with code 1
    cerr << "Connection failed";
    return 1;
  }

  // send the slogin message for this sender client
  connection.send(Message(TAG_SLOGIN, username));

  // receive the response to this slogin request
  Message login_resp = Message();
  connection.receive(login_resp);

  // if the response to the login returned an error tag, then exit with code 1
  if (login_resp.tag == TAG_ERR) {
    cerr << login_resp.data; // output the error data
    connection.close();
    return 1;
  }

  // Loop indefinitely, read commands from user, send messages to server when
  // needed
  while (1) {
    string in;        // temp variable to hold each line of input
    Message msg;      // will eventually be the sent message
    getline(cin, in); // get line of input from user

    // check for the possible commands (start with /)
    // for both leave and quit, don't care about what comes after command tag
    if (in == "/leave") {
      msg.tag = TAG_LEAVE; // assign tag accordingly
    } else if (in == "/quit") {
      msg.tag = TAG_QUIT; // assign tag accordingly
      connection.send(msg);
      Message quit_resp = Message();
      connection.receive(quit_resp); // receive response
      // if error occurs, output error to cerr
      if (quit_resp.tag == TAG_ERR ||
          connection.get_last_result() == Connection::INVALID_MSG) {
        cerr << quit_resp.data;
      }
      break; // but break out of loop regardless
    } else if (in.substr(0, 6) == "/join ") {
      msg.tag = TAG_JOIN;
      msg.data = in.substr(6); // grab the data that comes after tag
    } else { // if we get to here, then this is a sendall message
      msg.tag = TAG_SENDALL;
      msg.data = in;
    }

    connection.send(msg);
    Message msg_response = Message();
    connection.receive(msg_response); // receive response back
    if (msg_response.tag == TAG_ERR ||
        connection.get_last_result() ==
            Connection::INVALID_MSG) { // if an error occurs, output error data
                                       // but continue looping
      cerr << msg_response.data;
    }
  }
  connection.close();
  return 0;
}