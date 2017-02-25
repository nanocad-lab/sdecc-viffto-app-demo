/**
 * Author: Mark Gottscho
 * Email: mgottscho@ucla.edu
 */

#include "memory_due.h"
#include "minipk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

due_handler_t g_handler_stack[MAX_REGISTERED_HANDLERS]; 
size_t g_handler_sp = 0;

void dump_dueinfo(dueinfo_t* dueinfo) {
    if (dueinfo && dueinfo->valid) {
        static const char* regnames[] = {
          "z ", "ra", "sp", "gp", "tp", "t0",  "t1",  "t2",
          "s0", "s1", "a0", "a1", "a2", "a3",  "a4",  "a5",
          "a6", "a7", "s2", "s3", "s4", "s5",  "s6",  "s7",
          "s8", "s9", "sA", "sB", "t3", "t4",  "t5",  "t6"
        };
    
        printf("\n");
        printf("A DUE was recovered!\n");
        printf("----------- Setup ---------\n");
        dump_setup(&(dueinfo->setup));
        printf("---------------------------\n");
       
        printf("-------- Trap frame -------\n");
        dump_tf(&(dueinfo->tf));
        printf("---------------------------\n");
        
        printf("----- Error location ------\n");
        printf("error_in_stack = %d\n", dueinfo->error_in_stack);
        printf("error_in_text = %d\n", dueinfo->error_in_text);
        printf("error_in_data = %d\n", dueinfo->error_in_data);
        printf("error_in_sdata = %d\n", dueinfo->error_in_sdata);
        printf("error_in_bss = %d\n", dueinfo->error_in_bss);
        printf("error_in_heap = %d\n", dueinfo->error_in_heap);
        printf("_ftext: %p\n", &_ftext);
        printf("_etext: %p\n", &_etext);
        printf("_fdata: %p\n", &_fdata);
        printf("_edata: %p\n", &_edata);
        printf("_fbss: %p\n", &_fbss);
        printf("_end: %p\n", &_end);
        if ((void*)(dueinfo->tf.epc) < dueinfo->setup.pc_start || (void*)(dueinfo->tf.epc) > dueinfo->setup.pc_end)
            printf("DUE appears to have occurred in a subroutine.\n");
        printf("---------------------------\n");

        printf("---- Candidate messages ---\n");
        dump_candidate_messages(&(dueinfo->candidates));
        printf("---------------------------\n");
        
        printf("------ Cacheline (SI) -----\n");
        dump_cacheline(&(dueinfo->cacheline));
        printf("---------------------------\n");

        printf("-------- Load info --------\n");
        printf("Load destination register: %s\n", regnames[dueinfo->load_dest_reg]);
        printf("Load width: %d\n", dueinfo->recovered_load_value.size);
        printf("Message width: %lu\n", dueinfo->candidates.candidate_messages[0].size);
        printf("Load value offset in message: %d\n", dueinfo->load_message_offset);
        printf("---------------------------\n");

        printf("----- Recovered data ------\n");
        printf("Recovered message: 0x");
        dump_word(&(dueinfo->recovered_message));
        printf("\n");
        printf("Recovered load value: 0x");
        dump_word(&(dueinfo->recovered_load_value));
        printf("\n");
        printf("---------------------------\n");
        
        printf("----- DUE explanation -----\n");
        printf("%s", dueinfo->expl);
        printf("---------------------------\n");
    } else
        printf("No valid DUE info.\n");
}

void push_user_memory_due_trap_handler(char* name, user_defined_trap_handler fptr, void* pc_start, void* pc_end, due_region_strictness_t strict) {
    if (g_handler_sp+1 >= MAX_REGISTERED_HANDLERS) {
        printf("Failed to push new DUE handler, MAX_REGISTERED_HANDLERS has been exceeded.\n");
        return;
    }

    //Save necessary global user state
    g_handler_sp++;
    memcpy(g_handler_stack[g_handler_sp].name, name, 32);
    g_handler_stack[g_handler_sp].fptr = fptr;
    g_handler_stack[g_handler_sp].strict = strict;
    g_handler_stack[g_handler_sp].pc_start = pc_start;
    g_handler_stack[g_handler_sp].pc_end = pc_end;
    g_handler_stack[g_handler_sp].restart = 0; //Set decision should be made by user at time of DUE

    //First invocation only
    static int init = 0;
    if (!init) {
        user_trap_handler entry_trap_fptr = &memory_due_handler_entry;
        asm volatile("or a0, zero, %0;" //Load default entry trap handler fptr into register a0
                     "li a7, 447;" //Load syscall number 447 (SYS_register_user_memory_due_trap_handler) into register a7
                     "ecall;" //Make RISC-V environment call to register our user-defined trap handler
                     :
                     : "r" (entry_trap_fptr));
        init = 1;
    }
}

void pop_user_memory_due_trap_handler() {
    if (g_handler_sp == 0) {
        printf("Failed to pop DUE handler stack, none are currently registered.\n");
        return;
    }

    //Save necessary global user state
    g_handler_sp--;
}

int memory_due_handler_entry(trapframe_t* tf, due_candidates_t* candidates, due_cacheline_t* cacheline, word_t* recovered_message, word_t* recovered_load_value, short load_dest_reg, short load_message_offset) {
    static dueinfo_t user_context; //Static because we don't want this allocated on the stack, it is a large data structure

    //Init
    user_context.valid = 1;
    user_context.error_in_stack = 0;
    user_context.error_in_text = 0;
    user_context.error_in_data = 0;
    user_context.error_in_sdata = 0;
    user_context.error_in_bss = 0;
    user_context.error_in_heap = 0;
    user_context.load_dest_reg = 0;

    //Copy DUE handler setup context
    memcpy(user_context.setup.name, g_handler_stack[g_handler_sp].name, 32);
    user_context.setup.fptr = g_handler_stack[g_handler_sp].fptr;
    user_context.setup.strict = g_handler_stack[g_handler_sp].strict;
    user_context.setup.pc_start = g_handler_stack[g_handler_sp].pc_start;
    user_context.setup.pc_end = g_handler_stack[g_handler_sp].pc_end;
    user_context.setup.restart = g_handler_stack[g_handler_sp].restart;

    copy_word(&user_context.recovered_message, recovered_message);
    copy_word(&user_context.recovered_load_value, recovered_load_value);

    user_context.load_dest_reg = load_dest_reg;
    user_context.load_message_offset = load_message_offset;

    if (tf && !copy_trapframe(&(user_context.tf), tf)) {
        //Analyze trap frame, determine in which segment the memory DUE occured
        void* badvaddr = (void*)(tf->badvaddr);
        if (tf->badvaddr >= tf->gpr[2] && tf->badvaddr < tf->gpr[2]+64) //gpr[2] is sp. TODO: how to find size of stack frame dynamically, or otherwise find the base of stack? Right now we look 0 to +64 bytes from the tf's sp (because it grows down)
            user_context.error_in_stack = 1;
        if (badvaddr >= _ftext && badvaddr < _etext)
            user_context.error_in_text = 1;
        if (badvaddr >= _fdata && badvaddr < _edata)
            user_context.error_in_data = 1;
        if (badvaddr >= _edata && badvaddr < _fbss)
            user_context.error_in_sdata = 1;
        if (badvaddr >= _fbss && badvaddr < _end)
            user_context.error_in_bss = 1;
        user_context.error_in_heap = 0; //TODO
        user_context.load_dest_reg = decode_rd(tf->insn);
    } else
        user_context.valid = 0;

    if (!(candidates && !copy_candidates(&(user_context.candidates), candidates)))
        user_context.valid = 0;

    if (!(cacheline && !copy_cacheline(&(user_context.cacheline), cacheline))) 
        user_context.valid = 0;
        
    //Call user handler if we are not in strict mode or PC in error occurred in the registered PC range
    if (user_context.valid && 
        g_handler_stack[g_handler_sp].fptr &&
           (g_handler_stack[g_handler_sp].strict == STRICTNESS_DEFAULT || 
                 ((void*)(tf->epc) >= g_handler_stack[g_handler_sp].pc_start && (void*)(tf->epc) < g_handler_stack[g_handler_sp].pc_end))) {
        int retval = g_handler_stack[g_handler_sp].fptr(&user_context);
        if (!copy_word(recovered_message, &(user_context.recovered_message)))
            return retval;
        else
            return -1;
    }

    return -1;
}

void dump_word(word_t* w) {
   for (int i = 0; i < w->size; i++)
       printf("%02x", w->bytes[i]);
}

void dump_candidate_messages(due_candidates_t* cd) {
   if (cd) {
       for (int i = 0; i < cd->size; i++) {
           printf("Candidate message %d: 0x", i);
           dump_word(&(cd->candidate_messages[i]));
           printf("\n");
       }
   } else
       printf("Invalid candidate messages!\n");
}

void dump_cacheline(due_cacheline_t* cl) {
   if (cl) {
       for (int i = 0; i < cl->size; i++) {
           if (cl->blockpos != i) {
               printf("Word %d: 0x", i);
               dump_word(&(cl->words[i]));
           } else
               printf("Word %d: <CORRUPTED MESSAGE>", i);
           printf("\n");
       }
   } else
       printf("Invalid cacheline!\n");
}

void dump_setup(due_handler_t *setup) {
   printf("DUE handler name: %s\n", setup->name);
   printf("DUE handler user function: %p\n", setup->fptr); 
   printf("Handler invocations: %d\n", setup->invocations);
   printf("DUE handling strictness: %d\n", setup->strict); 
   printf("DUE PC region start: %p\n", setup->pc_start);
   printf("DUE PC region end: %p\n", setup->pc_end);
   printf("DUE region restart: %d\n", setup->restart);
}
