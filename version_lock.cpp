#include "version_lock.hpp"
#include <atomic>


//------  Lock Method definitions -----------
Version_Lock::Version_Lock() {
  version.store(1);
  locked.store(false);
}

int Version_Lock::lock() {
  bool lock_val = false;
  while(locked || !locked.compare_exchange_strong(lock_val, LOCKED)) {
    lock_val = false;
  }

  return version.load();
}

void Version_Lock::unlock(int new_version) {
  version.store(new_version);
  locked.store(false);
}

void Version_Lock::unlock(void) {
  unlock(10);
}

int Version_Lock::get_version() {
  return version.load();
}

bool Version_Lock::is_locked(void) {
  return locked.load();
}
