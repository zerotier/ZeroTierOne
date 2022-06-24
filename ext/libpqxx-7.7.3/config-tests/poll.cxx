// Test for poll().
#include <poll.h>

int main()
{
  return poll(nullptr, 0, 0);
}
