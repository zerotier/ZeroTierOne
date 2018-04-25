#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

namespace RethinkDB {

class TimeoutException : public std::exception {
public:
    const char *what() const throw () { return "operation timed out"; }
};

}

#endif  // EXCEPTIONS_H
