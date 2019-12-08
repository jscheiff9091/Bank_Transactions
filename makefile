#declare compiler and flags used

CXX = g++
CXXFLAGS = -fgnu-tm -mrtm -Wall -std=c++11 -pthread

#identify executable dependencies

bank_txn: main.o account_txn.o version_lock.o
	$(CXX) $(CXXFLAGS) -o bank_txn main.cpp account_txn.cpp version_lock.cpp

#identify object file dependencies dependencies

main.o: main.cpp main.hpp account_txn.hpp version_lock.hpp
#	$(CXX) $(CXXFLAGS) -c main.cpp

account_txn.o: account_txn.cpp account_txn.hpp main.hpp
#	$(CXX) $(CXXFLAGS) -c account_txn.cpp

cersion_lock.o: version_lock.cpp version_lock.hpp
#	$(CXX) $(CXXFLAGS) -c account_lock.cpp
