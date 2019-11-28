#declare compiler and flags used

CXX = g++
CXXFLAGS = -Wall -std=c++11 -pthread

#identify executable dependencies

bank_txn: main.o account_txn.o
	$(CXX) $(CXXFLAGS) -o bank_txn main.cpp account_txn.cpp

#identify object file dependencies dependencies

main.o: main.cpp main.hpp account_txn.hpp
#	$(CXX) $(CXXFLAGS) -c main.cpp

account_txn.o: account_txn.cpp account_txn.hpp main.hpp
#	$(CXX) $(CXXFLAGS) -c account_txn.cpp
