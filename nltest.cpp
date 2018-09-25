#include "osdep/LinuxNetLink.hpp"

using namespace ZeroTier;

int main(int argc, char **argv)
{
    LinuxNetLink &nl = LinuxNetLink::getInstance();


    while(true) {
        Thread::sleep(1000);
    }
}