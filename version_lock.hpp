#ifndef VERSION_LOCK_HPP
#define VERSION_LOCK_HPP

#define LOCKED  true

#include <atomic>

using namespace std;

class Version_Lock
{
private:
  atomic<int> version;
  atomic<bool> locked;

public:
  Version_Lock();

  int get_version();

  int lock();

  void unlock(int new_version);

  void unlock(void);

  bool is_locked(void);
};

#endif
