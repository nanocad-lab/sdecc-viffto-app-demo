/**
 * Author: Mark Gottscho
 * Email: mgottscho@ucla.edu
 */

#ifndef HANDLERS_H
#define HANDLERS_H

#include <memory_due.h>

//Declare relevant global data structures that are needed for DUE handlers at runtime (but extern -- they should be defined in handlers.c)
DECL_DUE_INFO_EXTERN(main, overall)
DECL_DUE_INFO_EXTERN(main, init)
DECL_DUE_INFO_EXTERN(main, compute)
DECL_RECOVERY_EXTERN(main,i,unsigned long)
DECL_RECOVERY_EXTERN(main,x,float)
DECL_RECOVERY_EXTERN(main,y,float)

//Declare handler functions
int DUE_RECOVERY_HANDLER(main, overall, dueinfo_t *recovery_context);
int DUE_RECOVERY_HANDLER(main, init, dueinfo_t *recovery_context);
int DUE_RECOVERY_HANDLER(main, compute, dueinfo_t *recovery_context); 

#endif
