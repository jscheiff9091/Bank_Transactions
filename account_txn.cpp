#include "account_txn.hpp"

using namespace std;

/*
  Function for thread to execute if Single Global Lock implementation selected
*/
void* sgl_main(void* lock) {
  pthread_mutex_t* sgl = (pthread_mutex_t *) lock;
  Txn_t txn;

  while(true) {
    //Check Transaction list
    pthread_mutex_lock(&txn_q_lock);

    //Check if transactions need to be executed
    if(txn_q.empty()) {
      pthread_mutex_unlock(&txn_q_lock);
      break;
    }
    else {
      //Get details
      txn = txn_q.front();
      txn_q.pop();
      pthread_mutex_unlock(&txn_q_lock);
    }

    //Perform Appropriate Transaction
    if(txn.txn_type == Balance_Query) {
      sgl_balance_query(txn.txn_number, txn.acct_1, sgl);
    }
    else if(txn.txn_type == Transfer) {
      sgl_transfer(txn.txn_number, txn.acct_1, txn.acct_2, txn.xfer_amnt, sgl);
    }
  }

  //maybe return query results ???
  return lock;
}

/*
Balance query function for Single Global Lock Transaction Implementation
*/
void sgl_balance_query(int txn_number, int acct, pthread_mutex_t* sgl) {
  //Acquire transaction lock
  pthread_mutex_lock(sgl);

  //Check valid bank account number supplied
  if(acct > -1 && acct < NUM_ACCOUNTS) {
    cout << "Account " << bank_accts[acct].acct_num << " : $" << bank_accts[acct].balance << endl;
  }
  else {
    cout << txn_number << ": Invalid bank account number, cannot get balance." << endl;
  }

  //Unlock
  pthread_mutex_unlock(sgl);
}

/*
Transfer funds between accounts using SGL implementation.
*/
void sgl_transfer(int txn_number, int acct1, int acct2, int xfer_amnt, pthread_mutex_t* sgl) {
  //Check valid account numbers supplied
  if(acct1 > -1 && acct2 > -1 && acct1 < NUM_ACCOUNTS && acct2 < NUM_ACCOUNTS) {
    //Acquire Lock
    pthread_mutex_lock(sgl);

    if(bank_accts[acct1].balance - xfer_amnt < 0) {
      cout << txn_number << ": Account overdraw! Transaction did not complete." << endl;
    }
    else {
      bank_accts[acct1].balance -= xfer_amnt;
      bank_accts[acct2].balance += xfer_amnt;
    }

    //Unlock
    pthread_mutex_unlock(sgl);
  }
  else {
    //Acquire Lock
    pthread_mutex_lock(sgl);

    cout << "Invalid account number supplied, unable to complete funds transfer. " << endl;

    //Unlock
    pthread_mutex_unlock(sgl);
  }
}



/*
Main thread function for Two Phase locking.
*/
void* two_phase_main(void* two_phase_locks) {
  //Cast arguments and create variables
  pthread_mutex_t* acct_locks = (pthread_mutex_t*) two_phase_locks;
  Txn_t txn;

  while(true) {
    //lock transaction q
    pthread_mutex_lock(&txn_q_lock);

    //check if empty, if not get next transaction
    if(txn_q.empty()) {
      pthread_mutex_unlock(&txn_q_lock);
      break;
    }
    else {
      txn = txn_q.front();
      txn_q.pop();
    }

    //Unlock Transaction q
    pthread_mutex_unlock(&txn_q_lock);

    //Execute appropriate transaction
    if(txn.txn_type == Balance_Query) {
      two_phase_balance_query(txn.txn_number, txn.acct_1, acct_locks);
    }
    else if(txn.txn_type == Transfer) {
      two_phase_transfer(txn.txn_number, txn.acct_1, txn.acct_2, txn.xfer_amnt, acct_locks);
    }
  }

  return two_phase_locks;
}


/*
Two phase locking balance query function.
*/
void two_phase_balance_query(int txn_number, int acct1, pthread_mutex_t* acct_locks) {
  //Check if account number valid
  if(acct1 < 0 || acct1 >= NUM_ACCOUNTS) {
    pthread_mutex_lock(&acct_locks[NUM_ACCOUNTS]);
    cout << txn_number << ": Account" << acct1 << " invalid. Unable to get balance." << endl;
    pthread_mutex_lock(&acct_locks[NUM_ACCOUNTS]);
  }
  else {
    //Print account balance
    pthread_mutex_lock(&acct_locks[acct1]);
    cout << "Account " << acct1 << " : $" << bank_accts[acct1].balance << endl;
    pthread_mutex_lock(&acct_locks[acct1]);
  }
}


/*
Two phase locking funds transfer function.
*/
void two_phase_transfer(int txn_number, int acct1, int acct2, int xfer_amnt, pthread_mutex_t* acct_locks) {
  //Ensure both account numbers are valid
  if(acct1 < 0 || acct2 < 0 || acct1 >= NUM_ACCOUNTS || acct2 >= NUM_ACCOUNTS) {
    pthread_mutex_lock(&acct_locks[NUM_ACCOUNTS]);
    cout << txn_number << ": Account number given is invalid. Unable to comple funds transfer." << endl;
    pthread_mutex_unlock(&acct_locks[NUM_ACCOUNTS]);
    return;
  }
  
  //Create ordering constraint
  if(acct1 == acct2) {
    return;
  }
  else if(acct2 > acct1) {
    int temp = acct2;
    acct2 = acct1;
    acct1 = temp;
  }

  //Check account 1 has sufficient funds to complete transfer
  if(bank_accts[acct1].balance - xfer_amnt < 0) {
    pthread_mutex_lock(&acct_locks[NUM_ACCOUNTS]);
    cout << txn_number << ": Account " << acct1 << "has insufficient funds to complete transfer." << endl;
    pthread_mutex_unlock(&acct_locks[NUM_ACCOUNTS]);
  }
  else {
     //Complete Funds transfer
     pthread_mutex_lock(&acct_locks[acct1]);
     pthread_mutex_lock(&acct_locks[acct2]);

     bank_accts[acct1].balance -= xfer_amnt;
     bank_accts[acct2].balance += xfer_amnt;

     pthread_mutex_unlock(&acct_locks[acct1]);
     pthread_mutex_unlock(&acct_locks[acct2]);
  }
    
}
