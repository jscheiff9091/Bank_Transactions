#include <iostream>
#include <fstream>
#include <sstream>
#include <string.h>
#include "main.hpp"
#include "account_txn.hpp"

/* Global Variables */
Account_t bank_accts[NUM_ACCOUNTS];
Account_SWHW_t swhw_bank_accts[NUM_ACCOUNTS];
Command_Line_Args_t cmd_line_args;
pthread_mutex_t txn_q_lock;
pthread_mutex_t io_lock;                  //Used in some cases
queue<Txn_t> txn_q;
atomic<int> global_version_clock;


using namespace std;

int main(int argc, char** argv) {
  get_cmd_line_args(argc, argv);       //Read in Command Line Args

  if(cmd_line_args.name_option) {      //Print name and exit (as specified in previous lab descriptions)
    cout << "Jacob Steven Scheiffler 3" << endl;
    return 0;
  }

  init_acct_balances(argv);            //Initialize Accounts
  init_txn_queue(argv);                //Initialize Transaction Queue


  
  //Execute transactions
  if(cmd_line_args.txn_implement == SGL) {          //Use Single Global lock
    pthread_mutex_t sgl;                            //Create Single Global lock and threads
    pthread_t threads[cmd_line_args.num_threads];

    pthread_mutex_init(&sgl, NULL);                 //Initialize SGL and Transaction Queue Lock                      
    pthread_mutex_init(&txn_q_lock, NULL);

    for(int i = 0; i < cmd_line_args.num_threads; i++) {     //Launch Threads
      pthread_create(&threads[i], NULL, &sgl_main, &sgl);
    }

    //Query Vector Return ??
    for(int i = 0; i < cmd_line_args.num_threads; i++) {     //Join Threads
      pthread_join(threads[i], NULL);
    }
    
    pthread_mutex_destroy(&sgl);                             //Clean Up
    pthread_mutex_destroy(&txn_q_lock);
  }
  else if(cmd_line_args.txn_implement == Two_Phase) {     //Two phase locking implementation
    pthread_mutex_t acct_locks[NUM_ACCOUNTS];         //Create locks and threads
    pthread_t threads[cmd_line_args.num_threads];

    for(int i = 0; i < NUM_ACCOUNTS; i++) {             //Initialize locks
      pthread_mutex_init(&acct_locks[i], NULL);
    }
    pthread_mutex_init(&txn_q_lock, NULL);
    pthread_mutex_init(&io_lock, NULL);

    for(int i = 0; i < cmd_line_args.num_threads; i++) { //Launch threads
      pthread_create(&threads[i], NULL, &two_phase_main, &acct_locks);
    }

    //Query vector return ???
    for(int i = 0; i < cmd_line_args.num_threads; i++) { //Join Threads
      pthread_join(threads[i], NULL);
    }

    for(int i = 0; i < NUM_ACCOUNTS+1; i++) {            //Clean up
      pthread_mutex_destroy(&acct_locks[i]);
    }
    pthread_mutex_destroy(&txn_q_lock);
    pthread_mutex_destroy(&io_lock);
  }
  else if(cmd_line_args.txn_implement == SW_Txn) {       //Software transactional memory implementation
    pthread_t threads[cmd_line_args.num_threads];        //Create thread variables

    pthread_mutex_init(&io_lock, NULL);                   //Initialize I/O Lock and Txn Queue Lock
    pthread_mutex_init(&txn_q_lock, NULL);

    for(int i = 0; i < cmd_line_args.num_threads; i++) { //Launch Threads
      pthread_create(&threads[i], NULL, &sw_txn_main, NULL);
    }

    for(int i = 0; i < cmd_line_args.num_threads; i++) { //Join Threads
      pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&io_lock);                     //Clean Up
    pthread_mutex_destroy(&txn_q_lock);
  }
  else if(cmd_line_args.txn_implement == HW_Txn) {       //Hardware transactional memory implementation
    pthread_t threads[cmd_line_args.num_threads];        //Create thread objects
    Version_Lock fallback_lock;
    
    pthread_mutex_init(&txn_q_lock, NULL);               //Initialize IO lock and transaction queue lock
    pthread_mutex_init(&io_lock, NULL);

    for(int i = 0; i < cmd_line_args.num_threads; i++) { //Launch Threads
      pthread_create(&threads[i], NULL, &hw_txn_main, &fallback_lock);
    }
    
    for(int i = 0; i < cmd_line_args.num_threads; i++) { //Join threads
      pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&txn_q_lock);                  //Clean up
    pthread_mutex_destroy(&io_lock);
  }
  else if(cmd_line_args.txn_implement == Optimistic) {
    pthread_t threads[cmd_line_args.num_threads];        //Create threads objects
    
    pthread_mutex_init(&txn_q_lock, NULL);               //Initialize variables
    pthread_mutex_init(&io_lock, NULL);
    global_version_clock.store(GLOBAL_VERSION_INITIAL);
    
    for(int i = 0;  i < cmd_line_args.num_threads; i++) {  //Launch Threads
      pthread_create(&threads[i], NULL, &optimistic_main, NULL);
    }

    for(int i = 0; i < cmd_line_args.num_threads; i++) {   //Join Threads
      pthread_join(threads[i], NULL); 
    }

    pthread_mutex_destroy(&txn_q_lock);                   //Clean up
    pthread_mutex_destroy(&io_lock);

  }

  

  if(cmd_line_args.print_final_balance) {                //Output account balances if necessary
    if(cmd_line_args.txn_implement == HW_Txn || cmd_line_args.txn_implement == SW_Txn) {
      for(int i = 0; i < NUM_ACCOUNTS; i++) {
	cout << swhw_bank_accts[i].acct_num << ": $" << swhw_bank_accts[i].balance << endl;
      }
    }
    else {
      for(int i = 0; i < NUM_ACCOUNTS; i++) {
	cout << bank_accts[i].acct_num << ": $" << bank_accts[i].balance << endl;
      }
    }
  }
  else if(cmd_line_args.output_to_file) {                //Print final account values to file
    ofstream final_balance;                              //Initialize output file stream
    final_balance.open(argv[cmd_line_args.outfile_place]);

    if(final_balance.is_open()) {                        //Check if outfile stream opened successful
      if(cmd_line_args.txn_implement == HW_Txn || cmd_line_args.txn_implement == SW_Txn) {
	for(int i = 0; i < NUM_ACCOUNTS; i++) {            //write balances to outfile stream
	  final_balance << swhw_bank_accts[i].acct_num << ": $" << swhw_bank_accts[i].balance << endl;
	}
      }
      else {
	for(int i = 0; i < NUM_ACCOUNTS; i++) {            //write balances to outfile stream
	  final_balance << bank_accts[i].acct_num << ": $" << bank_accts[i].balance << endl;
	}
      }
      
      final_balance.close();                             //Close outfile stream
    }
    else {                                               //File stream not open
      cout << "Unable to open account balance output file." << endl;
    }
  }

  cout << "Transactions Complete!" << endl;
  return 0;
}


/*
  Read in arguments from the command line, to get user desired program function.
  -Initialize Accounts?
  -Supply List of Transactions?
  -Print Name?
  -Number of Threads Specfied?
  -TXN implementation type?
*/
void get_cmd_line_args(int argc, char** argv) {

  //Initialize Arguments Struct
  cmd_line_args.name_option = false;
  cmd_line_args.acct_init = false;
  cmd_line_args.acct_file_place = 0;
  cmd_line_args.txn_script = false;
  cmd_line_args.txn_script_place = 0;
  cmd_line_args.num_threads_given = false;
  cmd_line_args.num_threads = DEFAULT_NUM_THREADS;
  cmd_line_args.txn_implement = SGL;
  cmd_line_args.print_final_balance = false;
  cmd_line_args.output_to_file = false;
  cmd_line_args.outfile_place = 0;

  //Scan through each argument passed to the command line
  for(int i = 1; i < argc; i++) {
    if(!strcmp(argv[i], "--name")) {                   //Check if name should be printed
      cmd_line_args.name_option = true;
    }
    else if(!strcmp(argv[i], "-ai")) {                 //User is supplying initial account balances
      cmd_line_args.acct_init = true;
      cmd_line_args.acct_file_place = ++i;
    }
    else if(!strcmp(argv[i], "-txn_s")) {                //User is supplying  list of transactions to run
      cmd_line_args.txn_script = true;
      cmd_line_args.txn_script_place = ++i;
    }
    else if(!strcmp(argv[i], "-txn_i")) {                //User specified which implementation of the transaction is used
      i++;
      if(!strcmp(argv[i], "SGL")){                       //Single Global Lock
	cmd_line_args.txn_implement = SGL;
      }
      else if(!strcmp(argv[i], "Two_Phase")) {           //Two phase locking scheme
	cmd_line_args.txn_implement = Two_Phase;
      }
      else if(!strcmp(argv[i], "SW_Txn")) {              //Software transactions
	cmd_line_args.txn_implement = SW_Txn;
      }
      else if(!strcmp(argv[i], "HW_Txn")) {              //Hardware transactions
	cmd_line_args.txn_implement = HW_Txn;
      }
      else if(!strcmp(argv[i], "Optimistic")) {          //Optimistic concurrency implementation
	cmd_line_args.txn_implement = Optimistic;
      }
      else {
	cout << "Not valid transaction implementation. Using default SGL." << endl;
      }
    }
    else if(!strcmp(argv[i], "-t_cnt")) {                  //User Specified thread count
      cmd_line_args.num_threads_given = true;
      cmd_line_args.num_threads = stoi(argv[++i]);
    }
    //else if(!strcmp(argv[i], "-h")) {                   //Print exection instructions
    //exit after printing
    //}
    else if(!strcmp(argv[i], "-p")) {                     //Print final account balances
      cmd_line_args.print_final_balance = true;
    }
    else if(!strcmp(argv[i], "-o")) {                     //Print final account balances to specified file
      cmd_line_args.output_to_file = true;
      cmd_line_args.outfile_place = ++i;
    }
    else {
      cout << "Argument passed was not valid flag or argument. Please re-run program with valid arguments." << endl;
      exit(-1);
    }
  }
}


/*
 For each account in the bank initialize to default value or user defined account values  
*/
void init_acct_balances(char** argv) {
  if(cmd_line_args.txn_implement == HW_Txn || cmd_line_args.txn_implement == SW_Txn) {
    if(cmd_line_args.acct_init) {                    //User specified file with initial account balances
      ifstream acct_balance_file;                    //initialize data stream
      acct_balance_file.open(argv[cmd_line_args.acct_file_place]);

      if(acct_balance_file.is_open()) {              //File opened succesfully?
	string balance_init;                         //init stream variables
	int i = 0;
      
	//get each account value and limit number of balances to the number of accounts in the bank
	while(getline(acct_balance_file, balance_init) && i < NUM_ACCOUNTS) {
	  swhw_bank_accts[i].acct_num = i;
	  swhw_bank_accts[i].balance = stoi(balance_init);
	  i++;
	}

	//Check to make sure all account balances have been initialized (will fall through if account limit reached)
	while(i < NUM_ACCOUNTS) {
	  swhw_bank_accts[i].acct_num = i;
	  swhw_bank_accts[i].balance = DEFAULT_BALANCE;
	  i++;
	}

	acct_balance_file.close();                   //Close Data stream
      }
      else {                                         //User file not found
	cout << "Unable to open account initialization file." << endl;
	exit(-1);
      }
    }
    else {
      //Account balance initialization file not passed, set each account to default value
      for(int i = 0; i < NUM_ACCOUNTS; i++) {
	swhw_bank_accts[i].acct_num = i;
	swhw_bank_accts[i].balance = DEFAULT_BALANCE;
      }
    }
  }
  else {
    if(cmd_line_args.acct_init) {                    //User specified file with initial account balances
      ifstream acct_balance_file;                    //initialize data stream
      acct_balance_file.open(argv[cmd_line_args.acct_file_place]);

      if(acct_balance_file.is_open()) {              //File opened succesfully?
	string balance_init;                         //init stream variables
	int i = 0;
      
	//get each account value and limit number of balances to the number of accounts in the bank
	while(getline(acct_balance_file, balance_init) && i < NUM_ACCOUNTS) {
	  bank_accts[i].acct_num = i;
	  bank_accts[i].balance.store(stoi(balance_init));
	  i++;
	}

	//Check to make sure all account balances have been initialized (will fall through if account limit reached)
	while(i < NUM_ACCOUNTS) {
	  bank_accts[i].acct_num = i;
	  bank_accts[i].balance.store(DEFAULT_BALANCE);
	  i++;
	}

	acct_balance_file.close();                   //Close Data stream
      }
      else {                                         //User file not found
	cout << "Unable to open account initialization file." << endl;
	exit(-1);
      }
    }
    else {
      //Account balance initialization file not passed, set each account to default value
      for(int i = 0; i < NUM_ACCOUNTS; i++) {
	bank_accts[i].acct_num = i;
	bank_accts[i].balance.store(DEFAULT_BALANCE);
      }
    }
  }
}



/*
  Set up queue of transactions to be executed by the program
*/
void init_txn_queue(char** argv) {
  if(cmd_line_args.txn_script) {     //Ensure user supplied transaction script
    ifstream txns;                   //initialize file stream
    txns.open(argv[cmd_line_args.txn_script_place]);

    if(txns.is_open()) {
      //initialize variables
      string temp;
      string clause;
      Txn_t txn;
      int i = 0;
      
      while(getline(txns, temp)) {
	stringstream txn_info(temp);                      //Get single line in the file

	getline(txn_info, clause, ',');                   //Get TXN type
	if(!strcmp(clause.c_str(), "query")) {            //balance query transaction type
	  txn.txn_type = Balance_Query;
	}
	else if(!strcmp(clause.c_str(), "transfer")) {    //transfer transaction type
	  txn.txn_type = Transfer;
	}
	else {                                            //invalid transaction type
	  cout << "\"" << clause << "\" is not a valid transaction type." << endl;
	  continue;
	}
	getline(txn_info, clause, ',');                   //Get Account 1
	txn.acct_1 = stoi(clause);                        
	getline(txn_info, clause, ',');                   //Get account 2
	txn.acct_2 = stoi(clause);
	getline(txn_info, clause);                        //Get transfer amount
	txn.xfer_amnt = stoi(clause);
	txn.txn_number = i++;                             //Set Transaction number

	txn_q.push(txn);                                  //Add transaction to the queue
      }
    }
    else {                                                //Transaction queue file not found
      cout << "Unable to open transaction script file." << endl;
      exit(-1);
    }
  }
  else {
    cout << "Transaction script not supplied. Please re-run and specify script name after '-txn_s' flag." << endl;
    exit(-1);
  }
}
