#pragma once

#include "connection.h"

namespace RethinkDB {

// The response from the server, as returned by run.
// The response is either a single datum or a stream:
//  * If it is a stream, the cursor represents each element of the stream.
//    - Batches are fetched from the server as needed.
//  * If it is a single datum, is_single() returns true.
//    - If it is an array, the cursor represents each element of that array
//    - Otherwise, to_datum() returns the datum and iteration throws an exception.
// The cursor can only be iterated over once, it discards data that has already been read.
class CursorPrivate;
class Cursor {
public:
    Cursor() = delete;
    ~Cursor();

    Cursor(Cursor&&);                   // movable
    Cursor& operator=(Cursor&&);
    Cursor(const Cursor&) = delete;     // not copyable
    Cursor& operator=(const Cursor&) = delete;

    // Returned by begin() and end()
    class iterator {
    public:
        iterator(Cursor*);
        iterator& operator++ ();
        Datum& operator* ();
        bool operator!= (const iterator&) const;

    private:
        Cursor *cursor;
    };

    // Consume the next element
    Datum& next(double wait = FOREVER) const;

    // Peek at the next element
    Datum& peek(double wait = FOREVER) const;

    // Call f on every element of the Cursor
    void each(std::function<void(Datum&&)> f, double wait = FOREVER) const;

    // Consume and return all elements
    Array&& to_array() &&;

    // If is_single(), returns the single datum. Otherwise returns to_array().
    Datum to_datum() &&;
    Datum to_datum() const &;

    // Efficiently consume and return all elements
    Array to_array() const &;

    // Close the cursor
    void close() const;

    // Returns false if there are no more elements
    bool has_next(double wait = FOREVER) const;

    // Returns false if the cursor is a stream
    bool is_single() const;

    iterator begin();
    iterator end();

private:
    explicit Cursor(CursorPrivate *dd);
    std::unique_ptr<CursorPrivate> d;

    friend class Connection;
};

}
