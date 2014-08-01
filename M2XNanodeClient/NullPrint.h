#ifndef NullPrint_h
#define NullPrint_h

#include "Print.h"

// Null Print class used to calculate length to print
class NullPrint : public Print {
public:
  virtual size_t write(uint8_t b) {
    count += 1;
    return 1;
  }

  virtual size_t write(const uint8_t* buf, size_t size) {
    count += size;
    return size;
  }
  size_t count;
};

#endif  /* NullPrint_h */
