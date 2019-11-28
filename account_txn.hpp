#ifndef ACCOUNT_TXN_HPP
#define ACCOUNT_TXN_HPP

/* Inlcudes */
#include <pthread.h>
#include <iostream>
#include "main.hpp"

/* Defines */


/* Typedefs */


/* Function Prototypes */
/*
Function for thread to execute if Single Global Lock implementation selected
*/
void* sgl_main(void* lock);


/*
Balance query function for Single Global Lock Transaction Implementation
*/
void sgl_balance_query(int txn_number, int acct, pthread_mutex_t* sgl);


/*
Transfer funds between accounts using SGL implementation.
*/
void sgl_transfer(int txn_number, int acct1, int acct2, int xfer_amnt, pthread_mutex_t* sgl);


/*
Main thread function for Two Phase locking.
*/
void* two_phase_main(void* two_phase_locks);


/*
Two phase locking balance query function.
*/
void two_phase_balance_query(int txn_number, int acct1, pthread_mutex_t* acct_locks);


/*
Two phase locking funds transfer function.
*/
void two_phase_transfer(int txn_number, int acct1, int acct2, int xfer_amnt, pthread_mutex_t* acct_locks);


#endif
