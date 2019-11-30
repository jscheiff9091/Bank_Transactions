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
void sgl_balance_query(int txn_number, int acct, pthread_mutex_t* sgl) {
  pthread_mutex_lock(sgl);                   //Acquire transaction lock

  //Check valid bank account number supplied
  if(acct > -1 && acct < NUM_ACCOUNTS) {     //Valid account number
    cout << "Account " << bank_accts[acct].acct_num << " : $" << bank_accts[acct].balance << endl;
  }
  else {                                     //Invalid account number
    cout << txn_number << ": Invalid bank account number, cannot get balance." << endl;
  }

  pthread_mutex_unlock(sgl);                 //Unlock transaction lock
}


/*
Transfer funds between accounts using SGL implementation.
*/
void sgl_transfer(int txn_number, int acct1, int acct2, int xfer_amnt, pthread_mutex_t* sgl) {
  if(acct1 > -1 && acct2 > -1 && acct1 < NUM_ACCOUNTS && acct2 < NUM_ACCOUNTS) {    //Check valid account numbers supplied
    pthread_mutex_lock(sgl);       //Acquire Lock
    
    //Perform transactions (check if account will be overdrawn)
    if(bank_accts[acct1].balance - xfer_amnt < 0) {
      cout << txn_number << ": Account overdraw! Transaction did not complete." << endl;
    }
    else {
      bank_accts[acct1].balance -= xfer_amnt;
      bank_accts[acct2].balance += xfer_amnt;
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
  if(acct1 < 0 || acct2 < 0 || acct1 >= NUM_ACCOUNTS || acct2 >= NUM_ACCOUNTS) {    //Ensure both account numbers are valid
    //Account number invalid
    pthread_mutex_lock(&acct_locks[NUM_ACCOUNTS]);     //Acquire IO Lock
    
    cout << txn_number << ": Account number given is invalid. Unable to comple funds transfer." << endl;   //Print error message

    pthread_mutex_unlock(&acct_locks[NUM_ACCOUNTS]);   //Release IO Lock
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

  pthread_mutex_lock(&acct_locks[acct1]);                 //Acquire account 1 lock

  if(bank_accts[acct1].balance - xfer_amnt < 0) {         //Account overdrawn
    pthread_mutex_unlock(&acct_locks[acct1]);             //Release account 1 lock
    pthread_mutex_lock(&acct_locks[NUM_ACCOUNTS]);        //Acquire transfer IO Lock
    
    cout << txn_number << ": Account " << acct1 << "has insufficient funds to complete transfer." << endl;  //Print error message

    pthread_mutex_unlock(&acct_locks[NUM_ACCOUNTS]);      //Release IO Lock
  }
  else {                                                  //Complete Funds transfer
    pthread_mutex_lock(&acct_locks[acct2]);               //Acquire account 2 Lock              

    bank_accts[acct1].balance -= xfer_amnt;               //Transfer
    bank_accts[acct2].balance += xfer_amnt;

    pthread_mutex_unlock(&acct_locks[acct1]);             //Relase Account locks
    pthread_mutex_unlock(&acct_locks[acct2]);
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
      sq_txn_transfer(txn.txn_number, txn.acct_1, txn.acct_2, txn.xfer_amnt);
    }
  }

  return args;
}


/*
SW Transactions balance query function.
*/
void sw_txn_balance_query(int txn_number, int acct1) {
  //Check if valid account number
  if(acct1 < 0 || acct1 >= NUM_ACCOUNTS) {             //Invalid account number
    pthread_mutex_lock(&io_lock);                      //Get IO Lock
    
    cout << txn_number << ": Account " << acct1 << " invalid. Unable to complete balance query." << endl; //Print error message

    pthread_mutex_unlock(&io_lock);                    //Release IO Lock
  }
  else {                                               //Valid Account number
    int acct_balance;
    __transaction_atomic{
      acct_balance = bank_accts[acct1].balance;        //Get account balance
    }
    
    pthread_mutex_lock(&io_lock);                      //Get IO Lock
    cout << acct1 << ": $" << acct_balance << endl;    //Print account balance
    pthread_mutex_unlock(&io_lock);                    //Release IO Lock
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
    __transaction_atomic{                                                         
      //check if account 1 will be overdrawn
      if(bank_accts[acct1].balance - xfer_amnt < 0) {         //Account overdrawn
	pthread_mutex_lock(&io_lock);                         //Get IO Lock
	cout << txn_number << ": Account " << acct1 << "has insufficient funds to complete transfer." << endl; //Print error message
	pthread_mutex_unlock(&io_lock);                       //Release IO Lock
      } 
      else {                                                  //Account not overdrawn, proceed with transfer
	bank_accts[acct1] -= xfer_amnt;                       //Transfer
	bank_accts[acct2] += xfer_amnt;
      }
    }
  }
}



/*
HW transaction threads main function.
*/
void* hw_txn_main(void * lock) {
  pthread_mutex_t* fb_lock = (pthread_mutex_t*) lock;
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
  return args;
}


/*
HW Transaction balance query function.
*/
void hw_txn_balance_query(int txn_number, int acct1, pthread_mutex_t* fb_lock) {
  int acct_balance;
  if(acct1 < 0 || acct1 >= NUM_ACCOUNTS) {    //invalid account number
    pthread_mutex_lock(&io_lock);
    cout << txn_number << ": Account " << acct1 << " invalid. Unable to complete balance query." << endl; //Print error message
    pthread_mutex_unlock(&io_lock);
    return;
  }
  else {                                      //Valid account number
    if(_xbegin() == _XBEGIN_STARTED) {        //Begin transaction
      if(fb_lock == LOCKED) {                 //Check other threads not using fall back path
	_xabort();                            //Abort
      }
      acct_balance = bank_accts[acct1].balance;
    }
    else {
      pthread_mutex_lock(&io_lock);            //Get fallback lock
      acct_balance = bank_accts[acct1].balance;
      pthread_mutex_unlock(&io_lock);          //Release fallback lock
    }
  }

  //Print balance
  pthread_mutex_lock(&io_lock);
  cout << "Account " << acct1 << ": $" << acct_balance;
  pthread_mutex_unlock(&io_lock);
}


/*
HW Transaction transfer function.
*/
void hw_txn_transfer(int txn_number, int acct1, int acct2, int xfer_amnt, pthread_mutex_t* fb_lock) {
  if(acct1 < 0 || acct2 < 0 || acct1 >= NUM_ACCOUNTS || acct2 >= NUM_ACCOUNTS) { //invlaid account number
    pthread_mutex_lock(&io_lock);
    cout << txn_number << ": Account number given is invalid. Unable to comple funds transfer." << endl;  //Print error message
    pthread_mutex_unlock(&io_lock);
    return;
  }
  else {                                          //Valid account number
    if(_xbegin() == _XBEGIN_STARTED) {
      
      if(fb_lock == LOCKED) {                     //Thread using fallback path
	_xabort();
      }
      else if(bank_accts[acct1] - xfer_amnt < 0) { //overdraw?
	pthread_mutex_lock(&io_lock);
	cout << txn_number << ": Account " << acct1 << "has insufficient funds to complete transfer." << endl; //Print error message
	pthread_mutex_unlock(&io_lock);
	_xabort();
      }
      bank_accts[acct1].balance -= xfer_amnt;    //transfer
      bank_accts[acct2].balance += xfer_amnt;
    }
    else {                                       //Fallback path
      pthread_mutex_lock(fb_lock);
      if(bank_accts[acct1].balance - xfer_amnt < 0) {  //account overdraw?
	pthread_mutex_unlock(fb_lock);
	pthread_mutex_lock(&io_lock);
	cout << txn_number << ": Account " << acct1 << "has insufficient funds to complete transfer." << endl; //Print error message
	pthread_mutex_unlock(&io_lock);
      }
      else {
	bank_accts[acct1].balance -= xfer_amnt;  //transfer
	bank_accts[acct2].balance += xfer_amnt;
	pthread_mutex_unlock(fb_lock);
      }
    }
  }
}
