#ifndef ACCOUNT_TXN_HPP
#define ACCOUNT_TXN_HPP

/* Inlcudes */
#include <pthread.h>
#include <iostream>
#include <immintrin.h>
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
int sgl_balance_query(int txn_number, int acct, pthread_mutex_t* sgl);


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
int two_phase_balance_query(int txn_number, int acct1, pthread_mutex_t* acct_locks);


/*
Two phase locking funds transfer function.
*/
void two_phase_transfer(int txn_number, int acct1, int acct2, int xfer_amnt, pthread_mutex_t* acct_locks);


/*
SW Transactions thread main.
*/
void* sw_txn_main(void* args);


/*
SW Transactions balance query function.
*/
int sw_txn_balance_query(int txn_number, int acct1);


/*
SW Transaction funds transfer function.
*/
void sw_txn_transfer(int txn_number, int acct1, int acct2, int xfer_amnt);


/*
HW transaction threads main function.
*/
void* hw_txn_main(void * lock);


/*
HW Transaction balance query function.
*/
int hw_txn_balance_query(int txn_number, int acct1, Version_Lock* fb_lock);


/*
HW Transaction transfer function.
*/
void hw_txn_transfer(int txn_number, int acct1, int acct2, int xfer_amnt, Version_Lock* fb_lock);

/*
TL2 Optimistic Implementation main.
*/
void* optimistic_main(void* args);

/*
TL2 Optimistic implementation transfer function (TL2 Write).
*/
void optimistic_transfer(int txn_number, int acct1, int acct2, int xfer_amnt);

/*
TL2 Optimitic implementation balance query function (TL2 Read).
*/
int optimistic_balance_query(int txn_number, int acct);

#endif
