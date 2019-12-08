#include "account_txn.hpp"

using namespace std;

/*
  Function for thread to execute if Single Global Lock implementation selected
*/
void* sgl_main(void* lock) {
  pthread_mutex_t* sgl = (pthread_mutex_t *) lock;
  Txn_t txn;

  while(true) {
    pthread_mutex_lock(&txn_q_lock);     //Get queue lock
    
    if(txn_q.empty()) {                  //Check if transactions need to be executed
      pthread_mutex_unlock(&txn_q_lock); //Release queue lock
      break;
    }
    else {
      txn = txn_q.front();               //Get next transaction
      txn_q.pop();
    }
    pthread_mutex_unlock(&txn_q_lock);   //Release queue lock

    if(txn.txn_type == Balance_Query) {  //Balance query transaction
      sgl_balance_query(txn.txn_number, txn.acct_1, sgl);
    }
    else if(txn.txn_type == Transfer) {  //Transfer transaction
      sgl_transfer(txn.txn_number, txn.acct_1, txn.acct_2, txn.xfer_amnt, sgl);
    }
  }

  //maybe return query results ???
  return lock;
}


/*
Balance query function for Single Global Lock Transaction Implementation
*/
int sgl_balance_query(int txn_number, int acct, pthread_mutex_t* sgl) {
  int acct_balance = -1;
  pthread_mutex_lock(sgl);                   //Acquire transaction lock

  //Check valid bank account number supplied
  if(acct > -1 && acct < NUM_ACCOUNTS) {     //Valid account number
    acct_balance = bank_accts[acct].balance.load();
  }
  else {                                     //Invalid account number
    cout << txn_number << ": Invalid bank account number, cannot get balance." << endl;
  }

  pthread_mutex_unlock(sgl);                 //Unlock transaction lock

  return acct_balance;
}


/*
Transfer funds between accounts using SGL implementation.
*/
void sgl_transfer(int txn_number, int acct1, int acct2, int xfer_amnt, pthread_mutex_t* sgl) {
  if(acct1 > -1 && acct2 > -1 && acct1 < NUM_ACCOUNTS && acct2 < NUM_ACCOUNTS) {    //Check valid account numbers supplied
    pthread_mutex_lock(sgl);       //Acquire Lock
    
    //Perform transactions (check if account will be overdrawn)
    if(bank_accts[acct1].balance.load() - xfer_amnt < 0) {
      cout << txn_number << ": Account overdraw! Transaction did not complete." << endl;
    }
    else {
      bank_accts[acct1].balance.store(bank_accts[acct1].balance.load() - xfer_amnt);
      bank_accts[acct2].balance.store(bank_accts[acct2].balance.load() + xfer_amnt);
    }

    pthread_mutex_unlock(sgl);     //Unlock
  }
  else {                           //Invalid account number
    pthread_mutex_lock(sgl);       //Acquire Lock
    cout << "Invalid account number supplied, unable to complete funds transfer. " << endl; 
    pthread_mutex_unlock(sgl);     //Unlock
  }
}



/*
Main thread function for Two Phase locking.
*/
void* two_phase_main(void* two_phase_locks) {
  pthread_mutex_t* acct_locks = (pthread_mutex_t*) two_phase_locks;    //Cast arguments and create variables
  Txn_t txn;

  while(true) {
    pthread_mutex_lock(&txn_q_lock);       //lock transaction q
    
    if(txn_q.empty()) {                    //check if empty, if not get next transaction
      pthread_mutex_unlock(&txn_q_lock);
      break;
    }
    else {
      txn = txn_q.front();
      txn_q.pop();
    }

    pthread_mutex_unlock(&txn_q_lock);     //Unlock Transaction q

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
int two_phase_balance_query(int txn_number, int acct1, pthread_mutex_t* acct_locks) {
  //Check if account number valid
  if(acct1 < 0 || acct1 >= NUM_ACCOUNTS) {
    pthread_mutex_lock(&io_lock);
    cout << txn_number << ": Account" << acct1 << " invalid. Unable to get balance." << endl;
    pthread_mutex_lock(&io_lock);
    return -1;
  }

  int acct_balance;
    
  //Print account balance
  for(int i = 0; i < NUM_ACCOUNTS; i++) {
    pthread_mutex_lock(&acct_locks[i]);
  }

  acct_balance = bank_accts[acct1].balance.load();

  for(int i = 0; i < NUM_ACCOUNTS; i++) {
    pthread_mutex_unlock(&acct_locks[i]);
  }

  return acct_balance;
}


/*
Two phase locking funds transfer function.
*/
void two_phase_transfer(int txn_number, int acct1, int acct2, int xfer_amnt, pthread_mutex_t* acct_locks) {
  if(acct1 < 0 || acct2 < 0 || acct1 >= NUM_ACCOUNTS || acct2 >= NUM_ACCOUNTS) {    //Ensure both account numbers are valid
    //Account number invalid
    pthread_mutex_lock(&io_lock);     //Acquire IO Lock
    
    cout << txn_number << ": Account number given is invalid. Unable to comple funds transfer." << endl;   //Print error message

    pthread_mutex_unlock(&io_lock);   //Release IO Lock
    return;
  }

  for(int i = 0; i < NUM_ACCOUNTS; i++) {
    pthread_mutex_lock(&acct_locks[i]);                 //Acquire all account lock
  }

  if(bank_accts[acct1].balance.load() - xfer_amnt < 0) {         //Account overdrawn
    pthread_mutex_lock(&io_lock);                         //Acquire transfer IO Lock
    cout << txn_number << ": Account " << acct1 << " has insufficient funds to complete transfer." << endl;  //Print error message
    pthread_mutex_unlock(&io_lock);                       //Release IO Lock
    return;
  }
  
  bank_accts[acct1].balance.store(bank_accts[acct1].balance.load() - xfer_amnt);  //Complete Funds transfer 
  bank_accts[acct2].balance.store(bank_accts[acct2].balance.load() + xfer_amnt);

  for(int i = 0; i < NUM_ACCOUNTS; i++) {
    pthread_mutex_unlock(&acct_locks[i]);                 //Release all account locks
  }
}


/*
SW Transactions thread main.
*/
void* sw_txn_main(void* args) {
  Txn_t txn;

  while(true) {
    //Check if remaining transactions need to be completed
    pthread_mutex_lock(&txn_q_lock);      //Get queue lock
    if(txn_q.empty()) {
      pthread_mutex_unlock(&txn_q_lock);  //Release queue lock
      break;
    }
    else {
      txn = txn_q.front();                //Get next transaction 
      txn_q.pop();
    }
    pthread_mutex_unlock(&txn_q_lock);    //Release queue Lock

    if(txn.txn_type == Balance_Query) {   //Perform Balance Query
      sw_txn_balance_query(txn.txn_number, txn.acct_1);
    }
    else if(txn.txn_type == Transfer) {   //Perform Transfer
      sw_txn_transfer(txn.txn_number, txn.acct_1, txn.acct_2, txn.xfer_amnt);
    }
  }

  return args;
}


/*
SW Transactions balance query function.
*/
int sw_txn_balance_query(int txn_number, int acct1) {
  //Check if valid account number
  if(acct1 < 0 || acct1 >= NUM_ACCOUNTS) {             //Invalid account number
    pthread_mutex_lock(&io_lock);                      //Get IO Lock
    cout << txn_number << ": Account " << acct1 << " invalid. Unable to complete balance query." << endl; //Print error message
    pthread_mutex_unlock(&io_lock);                    //Release IO Lock
    return -1;
  }
  else {                                               //Valid Account number
    int acct_balance;
    __transaction_atomic{
      acct_balance = swhw_bank_accts[acct1].balance;        //Get account balance
    }
    
    return acct_balance;
  }
}


/*
SW Transaction funds transfer function.
*/
void sw_txn_transfer(int txn_number, int acct1, int acct2, int xfer_amnt) {
  //check if valid acct numbers
  if(acct1 < 0 || acct2 < 0 || acct1 >= NUM_ACCOUNTS || acct2 >= NUM_ACCOUNTS) {  //Invalid account number(s)
    pthread_mutex_lock(&io_lock);                                                 //Get IO Lock
    cout << txn_number << ": Account number given is invalid. Unable to comple funds transfer." << endl;  //Print error message
    pthread_mutex_unlock(&io_lock);                                               //Release IO Lock
    return;
  }
  else {
    bool overdrawn = false;
    __transaction_atomic{                                                         
      //check if account 1 will be overdrawn
      if(swhw_bank_accts[acct1].balance - xfer_amnt < 0) {         //Account overdrawn
	overdrawn = true;
      } 
      else {                                                  //Account not overdrawn, proceed with transfer
	swhw_bank_accts[acct1].balance -= xfer_amnt;  //Complete Funds transfer 
	swhw_bank_accts[acct2].balance += xfer_amnt;
      }
    }

    if(overdrawn) {
      pthread_mutex_lock(&io_lock);                         //Get IO Lock
      cout << txn_number << ": Account " << acct1 << " has insufficient funds to complete transfer." << endl; //Print error message
      pthread_mutex_unlock(&io_lock);                       //Release IO Lock
    }
  }
}



/*
HW transaction threads main function.
*/
void* hw_txn_main(void * lock) {
  Version_Lock* fb_lock = (Version_Lock*) lock;
  Txn_t txn;

  while(true) {
    pthread_mutex_lock(&txn_q_lock);         //Acquire transaction queue lock

    if(txn_q.empty()) {                      //Queue empty
      pthread_mutex_unlock(&txn_q_lock);     //Release lock and return
      break;
    }
    else {                                   //More transactions left to complete
      txn = txn_q.front();                   //Get next transaction
      txn_q.pop();
    }

    pthread_mutex_unlock(&txn_q_lock);       //Release transaction queue lock

    if(txn.txn_type == Balance_Query) {      //Perform balance query
      hw_txn_balance_query(txn.txn_number, txn.acct_1, fb_lock);
    }
    else if(txn.txn_type == Transfer) {      //Perform transfer
      hw_txn_transfer(txn.txn_number, txn.acct_1, txn.acct_2, txn.xfer_amnt, fb_lock);
    }
  }
  return lock;
}

/*
HW Transaction balance query function.
*/
int hw_txn_balance_query(int txn_number, int acct1, Version_Lock* fb_lock) {
  int acct_balance = -1;
  if(acct1 < 0 || acct1 >= NUM_ACCOUNTS) {    //invalid account number
    pthread_mutex_lock(&io_lock);
    cout << txn_number << ": Account " << acct1 << " invalid. Unable to complete balance query." << endl; //Print error message
    pthread_mutex_unlock(&io_lock);
    return acct_balance;
  }
  else {                                      //Valid account number
    bool txn_complete = false;
    for(int i = 0; i < 10; i++) {
      if(_xbegin() == _XBEGIN_STARTED) {        //Begin transaction
	if(fb_lock->is_locked()) {                 //Check other threads not using fall back path
	  _xabort(_XABORT_EXPLICIT);            //Abort
	}
	acct_balance = swhw_bank_accts[acct1].balance;
	_xend();
	txn_complete = true;
	break;
      }
    }

    if(!txn_complete) {
      fb_lock->lock();            //Get fallback lock
      acct_balance = swhw_bank_accts[acct1].balance;
      fb_lock->unlock();          //Release fallback lock
    }
  }

  return acct_balance;
}

/*
HW Transaction transfer function.
*/
void hw_txn_transfer(int txn_number, int acct1, int acct2, int xfer_amnt, Version_Lock* fb_lock) {
  if(acct1 < 0 || acct2 < 0 || acct1 >= NUM_ACCOUNTS || acct2 >= NUM_ACCOUNTS) { //invlaid account number
    pthread_mutex_lock(&io_lock);
    cout << txn_number << ": Account number given is invalid. Unable to comple funds transfer." << endl;  //Print error message
    pthread_mutex_unlock(&io_lock);
    return;
  }
  else {                                          //Valid account number
    bool overdrawn = false;
    bool txn_complete = false;
    for(int i = 0; i < 10; i++) {
      if(_xbegin() == _XBEGIN_STARTED) {
	if(fb_lock->is_locked()) {                     //Thread using fallback path
	  _xabort(_XABORT_EXPLICIT);
	}
	
	if(swhw_bank_accts[acct1].balance - xfer_amnt < 0) { //overdraw?
	  overdrawn = true;
	}
	else {
	  swhw_bank_accts[acct1].balance -= xfer_amnt;  //Complete Funds transfer 
	  swhw_bank_accts[acct2].balance += xfer_amnt;
	}
	_xend();
	txn_complete = true;
	break;
      }
    }

    if(!txn_complete) {                                     //Txn unable to complete in 10 tries, use fallback lock
      fb_lock->lock();
      if(swhw_bank_accts[acct1].balance - xfer_amnt < 0) {  //account overdraw?
	overdrawn = true;
      }
      else {
	swhw_bank_accts[acct1].balance -= xfer_amnt;        //Complete Funds transfer 
	swhw_bank_accts[acct2].balance += xfer_amnt;
      }
      fb_lock->unlock();
    }

    if(overdrawn) {
      pthread_mutex_lock(&io_lock);
      cout << txn_number << ": Account " << acct1 << "has insufficient funds to complete transfer." << endl; //Print error message
      pthread_mutex_unlock(&io_lock);
    }
  }
}



/*
TL2 Optimistic Implementation main.
*/
void* optimistic_main(void* args) {
  Txn_t txn;

  while(true) {
    pthread_mutex_lock(&txn_q_lock);                               //Get Queue lock

    if(txn_q.empty()) {                                            //Any more transactions to complete?
      pthread_mutex_unlock(&txn_q_lock);                          
      break;
    }
    
    txn = txn_q.front();                                           //Get transaction
    txn_q.pop();                                                   //Remove transaction from queue

    pthread_mutex_unlock(&txn_q_lock);                           //Unlock queue

    if(txn.txn_type == Balance_Query) {                       //Perform balance query
      optimistic_balance_query(txn.txn_number, txn.acct_1);
    }
    else if(txn.txn_type == Transfer) {                       //Perform transfer
      optimistic_transfer(txn.txn_number, txn.acct_1, txn.acct_2, txn.xfer_amnt); 
    }
  }
  return args;
}


/*
TL2 Optimistic implementation transfer function (TL2 Write).
*/
void optimistic_transfer(int txn_number, int acct1, int acct2, int xfer_amnt) {
  if(acct1 < 0 || acct2 < 0 || acct1 >= NUM_ACCOUNTS || acct2 >= NUM_ACCOUNTS) {
    pthread_mutex_lock(&io_lock);
    cout << txn_number << ": Account number given is invalid. Unable to comple funds transfer." << endl;  //Print error message
    pthread_mutex_unlock(&io_lock);
    return;
  }
  else if (acct1 == acct2) {
    return;
  }

  int g_clk, acct1_balance, acct1_version, acct2_balance, acct2_version;

  while(true) {
    g_clk = global_version_clock.load();             //Get timestamp

    acct1_balance = bank_accts[acct1].balance.load(); //Speculative transaction execution
    acct2_balance = bank_accts[acct2].balance.load();
 
    if(acct1_balance - xfer_amnt < 0) {              //Check for overdrawn
      pthread_mutex_lock(&io_lock);
      cout << txn_number << ": Account " << acct1 << " has insufficient funds to complete transfer." << endl; //Print error message
      pthread_mutex_unlock(&io_lock);
      return;
    }

    acct1_balance -= xfer_amnt;                      //Transfer
    acct2_balance += xfer_amnt;

    if(acct1 > acct2) {                              //Acquire account locks
      acct2_version = bank_accts[acct2].acct_lock.lock();
      acct1_version = bank_accts[acct1].acct_lock.lock();
    }
    else {
      acct1_version = bank_accts[acct1].acct_lock.lock();
      acct2_version = bank_accts[acct2].acct_lock.lock();
    }

    if(acct1_version > g_clk || acct2_version > g_clk) { //Check current account timestamp
      bank_accts[acct1].acct_lock.unlock(acct1_version); //Unlock and DON'T change version
      bank_accts[acct2].acct_lock.unlock(acct2_version);
      continue;
    }

    bank_accts[acct1].balance.store(acct1_balance);  //Commit transfer
    bank_accts[acct2].balance.store(acct2_balance);

    g_clk = global_version_clock.load();
    while(!global_version_clock.compare_exchange_weak(g_clk, g_clk+1)) {  //Update version clocks
      continue;
    }

    g_clk++;
    
    bank_accts[acct1].acct_lock.unlock(g_clk);       //Update local version clocks and unlock
    bank_accts[acct2].acct_lock.unlock(g_clk);
    break;
  }
}


/*
TL2 Optimitic implementation balance query function (TL2 Read).
*/
int optimistic_balance_query(int txn_number, int acct) {
  if(acct < 0 || acct >= NUM_ACCOUNTS) {                           //Check for valid account number
    pthread_mutex_lock(&io_lock);
    cout << txn_number << ": Account " << acct << " invalid. Unable to complete balance query." << endl; //Print error message
    pthread_mutex_unlock(&io_lock);
  }

  int g_clk, acct_balance;
  while(true) {
    g_clk = global_version_clock.load();

    if(bank_accts[acct].acct_lock.is_locked()) {
      continue;
    }
    
    acct_balance = bank_accts[acct].balance.load();

    if(bank_accts[acct].acct_lock.get_version() > g_clk) {
      continue;
    }
    break;
  }
  return acct_balance;
}
