#include "client_util.h"
#include "connection.h"
#include "csapp.h"
#include "message.h"
#include <iostream>
#include <string>
#include <vector>

using std::cerr;
using std::cout;
using std::string;
using std::vector;

// helper function to divide a given string into a list (vector) based on a
// given delimiter
vector<string> split_string(string str, string delimiter) {
  int s, e = -1 * delimiter.size(); // initial start and end values
  vector<string> split_list;        // list of split "words"
  do {
    s = e + delimiter.size();
    e = str.find(delimiter, s);
    split_list.push_back(str.substr(
        s, e - s)); // get the substring that is e-s chars long, starting from s
  } while (e != -1);
  
  return split_list;
}

int main(int argc, char **argv) {
  if (argc != 5) {
    cerr << "Usage: ./receiver [server_address] [port] [username] [room]\n";
    return 1;
  }

  // initialize the client variables based on input args
  string server_hostname = argv[1];
  int server_port = std::stoi(argv[2]);
  string username = argv[3];
  string room_name = argv[4];
  Connection connection;

  // connect to the server
  connection.connect(server_hostname, server_port);
  if (!connection.is_open()) { // if failed, then exit with code 1
    cerr << "Failed to connect to server";
    return 1;
  }

  // Send rlogin and join messages (expect a response from
  //       the server for each one)
  connection.send(Message(TAG_RLOGIN, username));

  // receive the response to this rlogin request
  Message login_resp = Message();
  connection.receive(login_resp);

  // if the response to the login returned an error tag, then exit with code 1
  if (login_resp.tag == TAG_ERR) {
    cerr << login_resp.data;
    return 1;
  }

  connection.send(Message(
      TAG_JOIN, room_name)); // attempt to join room (based on user's input)

  // receive the response to this join request
  Message join_resp = Message();
  connection.receive(join_resp);

  // if the response to the join returned an error tag, then exit with code 1
  if (join_resp.tag == TAG_ERR) {
    cerr << join_resp.data;
    return 1;
  }

  // Loop indefinitely waiting for messages from server (which should be tagged
  // with TAG_DELIVERY)
  while (1) {
    Message msg = Message(); // will hold the message from the server
    connection.receive(msg);

    // if a message with this correct tag is received, then need to handle it
    if (msg.tag == TAG_DELIVERY) {
      vector<string> data = split_string(
          msg.data,
          ":"); // split the string using helper function with colon delimiter
      cout << data[1] << ": "
           << data[2]; // output the user, and their sent message, w/ colon
    }
  }
  connection.close();
  // receiver will close from a SIGINT, so infinite loop is acceptable
  return 0;
}