// Searches for good delimiters to cut streams into relatively well sized
// segments.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <boost/cstdint.hpp>
#include <boost/array.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <iostream>
#include <vector>
#include <map>

// Desired size range
#define MIN_DESIRED_SIZE 4096
#define MAX_DESIRED_SIZE 131072

#define DELIMITER_SET_SIZE 1
typedef boost::array<boost::uint16_t,DELIMITER_SET_SIZE> DelimArray;

struct BestEntry
{
  DelimArray best;
  double bestScore;
  std::vector<unsigned char> data;
};

boost::mutex bestLock;
boost::mutex outLock;
std::map<std::string,BestEntry> best;

static void runThread(const std::string &fileName)
{
  char tmp[4096];

  boost::mt19937 prng;
  {
    boost::uint32_t seed;
    FILE *ur = fopen("/dev/urandom","r");
    fread((void *)&seed,1,sizeof(seed),ur);
    fclose(ur);
    prng.seed(seed);
  }

  BestEntry *myEntry;
  {
    boost::mutex::scoped_lock l(bestLock);
    myEntry = &(best[fileName]);
    myEntry->bestScore = 99999999.0;
  }

  {
    boost::mutex::scoped_lock l(outLock);

    std::cout << "*** Reading test data from: " << fileName << std::endl;
    FILE *f = fopen(fileName.c_str(),"r");
    if (f) {
      int n;
      while ((n = fread((void *)tmp,1,sizeof(tmp),f)) > 0) {
        for(int i=0;i<n;++i)
          myEntry->data.push_back((unsigned char)tmp[i]);
      }
      fclose(f);
    }

    if (myEntry->data.size() <= 0) {
      std::cout << "Error: no data read." << std::endl;
      exit(1);
    } else std::cout << "*** Read " << myEntry->data.size() << " bytes of test data." << std::endl;

    std::cout.flush();
  }

  DelimArray current;
  for(unsigned int i=0;i<DELIMITER_SET_SIZE;++i)
    current[i] = (boost::uint16_t)prng();

  for(;;) {
    unsigned long numTooShort = 0;
    unsigned long numTooLong = 0;
    unsigned long numGood = 0;

    boost::uint32_t shiftRegister = 0;
    unsigned long segSize = 0;
    for(std::vector<unsigned char>::iterator i=myEntry->data.begin();i!=myEntry->data.end();++i) {
      shiftRegister <<= 1;
      shiftRegister |= (((boost::uint32_t)*i) & 1);

      ++segSize;

      boost::uint16_t transformedShiftRegister = (boost::uint16_t)(shiftRegister);

      for(DelimArray::iterator d=current.begin();d!=current.end();++d) {
        if (transformedShiftRegister == *d) {
          if (segSize < MIN_DESIRED_SIZE)
            ++numTooShort;
          else if (segSize > MAX_DESIRED_SIZE)
            ++numTooLong;
          else ++numGood;
          segSize = 0;
          break;
        }
      }
    }
    if (segSize) {
      if (segSize < MIN_DESIRED_SIZE)
        ++numTooShort;
      else if (segSize > MAX_DESIRED_SIZE)
        ++numTooLong;
      else ++numGood;
    }

    if (numGood) {
      double score = ((double)(numTooShort + numTooLong)) / ((double)numGood);

      if (score < myEntry->bestScore) {
        myEntry->best = current;
        myEntry->bestScore = score;

        boost::mutex::scoped_lock l(outLock);

        std::cout << fileName << ": ";

        for(DelimArray::iterator d=current.begin();d!=current.end();++d) {
          sprintf(tmp,"0x%.4x",(unsigned int)*d);
          if (d != current.begin())
            std::cout << ',';
          std::cout << tmp;
        }

        std::cout << ": " << numTooShort << " / " << numGood << " / " << numTooLong << " (" << score << ")" << std::endl;
        std::cout.flush();

        if ((numTooShort == 0)&&(numTooLong == 0))
          break;
      }
    }

    for(DelimArray::iterator i=current.begin();i!=current.end();++i)
      *i = (boost::uint16_t)prng();
  }
}

int main(int argc,char **argv)
{
  std::vector< boost::shared_ptr<boost::thread> > threads;

  for(int i=1;i<argc;++i) {
    boost::shared_ptr<boost::thread> t(new boost::thread(boost::bind(&runThread,std::string(argv[i]))));
    threads.push_back(t);
  }

  for(std::vector< boost::shared_ptr<boost::thread> >::iterator i=threads.begin();i!=threads.end();++i)
    (*i)->join();

  return 0;
}
