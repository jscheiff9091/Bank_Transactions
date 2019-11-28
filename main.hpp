#ifndef MAIN_HPP
#define MAIN_HPP

using namespace std;

#include <queue>

/* Defines */
#define NUM_ACCOUNTS          100
#define DEFAULT_NUM_THREADS   6
#define DEFAULT_BALANCE       50

/* Typedefs */
typedef enum
{
  SGL,
  Two_Phase,
  SW_Txn,
  HW_Txn,
  Optimistic
}TXN_Implementation_t;

typedef struct
{
  int acct_num;
  int balance;
}Account_t;

typedef struct {
  bool name_option;          //Print name?
  bool acct_init;            //Initialize accounts?
  int acct_file_place;
  bool txn_script;           //List of transactions given?
  int txn_script_place;
  bool num_threads_given;    //Specified number of threads?
  int num_threads;
  TXN_Implementation_t txn_implement; //User Specified how txn executed?
  bool print_final_balance;  //Print final account balances?
  bool output_to_file;       //Print final account balances to a specified file?
  int outfile_place;
}Command_Line_Args_t;

typedef enum {
  Balance_Query,
  Transfer
}Txn_Type_t;

typedef struct {
  Txn_Type_t txn_type;
  int txn_number;
  int acct_1;
  int acct_2;
  int xfer_amnt;
}Txn_t;


/* Global Variables */
extern Account_t bank_accts[NUM_ACCOUNTS];
extern Command_Line_Args_t cmd_line_args;
extern pthread_mutex_t txn_q_lock;
extern queue<Txn_t> txn_q;
//extern atomic<bool> wait_for_txn;


/* Function Prototypes */

/*
  Read in arguments from the command line, to get user desired program function.
  -Initialize Accounts?
  -Supply List of Transactions?
  -Print Name?
  -Number of Threads Specfied?
*/
void get_cmd_line_args(int argc, char** argv);

/*
 For each account in the bank initialize to $50 or user defined account values
*/
void init_acct_balances(char** argv);

/*
  Set up queue of transactions to be executed by the program
*/
void init_txn_queue(char** argv);
#endif
