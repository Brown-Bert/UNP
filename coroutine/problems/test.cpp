#include <unistd.h>

#include <iostream>

#include "mythreadspoll.h"

int main() {
  {
    ThreadsPoll poll;
    poll.createPoll();
  }
  exit(0);
}