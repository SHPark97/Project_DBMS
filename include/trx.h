#ifndef TRX_H
#define TRX_H

#include "lock_table.h"
#define ON (1)
#define OFF (0)
#define LINKED (1)
#define UNLINKED (0)

using namespace std;

struct trx_t {
    int trx_id;
    int last_lsn;
    lock_t* head;
    bool active;
};

struct deadlock_t {
    int count;
    bool linked;
};

int trx_begin(void);

int trx_commit(int trx_id);

void trx_delete(lock_t* lock_obj);

void trx_link(lock_t* lock_obj);

int trx_deadlock_detect(lock_t* lock_obj);

int trx_abort(int trx_id);

void rollback_results(lock_t* lock_obj);


int trx_get_last_lsn(int trx_id, int lsn);

#endif