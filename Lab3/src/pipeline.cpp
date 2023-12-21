//////////////////////////////////////////////////////////////////////
// In part B, you must modify this file to implement the following: //
// - void pipe_cycle_issue(Pipeline *p)                             //
// - void pipe_cycle_schedule(Pipeline *p)                          //
// - void pipe_cycle_writeback(Pipeline *p)                         //
// - void pipe_cycle_commit(Pipeline *p)                            //
//////////////////////////////////////////////////////////////////////

// pipeline.cpp
// Implements the out-of-order pipeline.

#include "pipeline.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * The width of the pipeline; that is, the maximum number of instructions that
 * can be processed during any given cycle in each of the issue, schedule, and
 * commit stages of the pipeline.
 * 
 * (Note that this does not apply to the writeback stage: as many as
 * MAX_WRITEBACKS instructions can be written back to the ROB in a single
 * cycle!)
 * 
 * When the width is 1, the pipeline is scalar.
 * When the width is greater than 1, the pipeline is superscalar.
 */
extern uint32_t PIPE_WIDTH;

/**
 * The number of entries in the ROB; that is, the maximum number of
 * instructions that can be stored in the ROB at any given time.
 * 
 * You should use only this many entries of the ROB::entries array.
 */
extern uint32_t NUM_ROB_ENTRIES;

/**
 * Whether to use in-order scheduling or out-of-order scheduling.
 * 
 * The possible values are SCHED_IN_ORDER for in-order scheduling and
 * SCHED_OUT_OF_ORDER for out-of-order scheduling.
 * 
 * Your implementation of pipe_cycle_sched() should check this value and
 * implement scheduling of instructions accordingly.
 */
extern SchedulingPolicy SCHED_POLICY;

/**
 * The number of cycles an LD instruction should take to execute.
 * 
 * This is used by the code in exeq.cpp to determine how long to wait before
 * considering the execution of an LD instruction done.
 */
extern uint32_t LOAD_EXE_CYCLES;

/**
 * Read a single trace record from the trace file and use it to populate the
 * given fe_latch.
 * 
 * You should not modify this function.
 * 
 * @param p the pipeline whose trace file should be read
 * @param fe_latch the PipelineLatch struct to populate
 */
void pipe_fetch_inst(Pipeline *p, PipelineLatch *fe_latch)
{
    InstInfo *inst = &fe_latch->inst;
    TraceRec trace_rec;
    uint8_t *trace_rec_buf = (uint8_t *)&trace_rec;
    size_t bytes_read_total = 0;
    ssize_t bytes_read_last = 0;
    size_t bytes_left = sizeof(TraceRec);

    // Read a total of sizeof(TraceRec) bytes from the trace file.
    while (bytes_left > 0)
    {
        bytes_read_last = read(p->trace_fd, trace_rec_buf, bytes_left);
        if (bytes_read_last <= 0)
        {
            // EOF or error
            break;
        }

        trace_rec_buf += bytes_read_last;
        bytes_read_total += bytes_read_last;
        bytes_left -= bytes_read_last;
    }

    // Check for error conditions.
    if (bytes_left > 0 || trace_rec.op_type >= NUM_OP_TYPES)
    {
        fe_latch->valid = false;
        p->halt_inst_num = p->last_inst_num;

        if (p->stat_retired_inst >= p->halt_inst_num)
        {
            p->halt = true;
        }

        if (bytes_read_last == -1)
        {
            fprintf(stderr, "\n");
            perror("Couldn't read from pipe");
            return;
        }

        if (bytes_read_total == 0)
        {
            // No more trace records to read
            return;
        }

        // Too few bytes read or invalid op_type
        fprintf(stderr, "\n");
        fprintf(stderr, "Error: Invalid trace file\n");
        return;
    }

    // Got a valid trace record!
    fe_latch->valid = true;
    fe_latch->stall = false;
    inst->inst_num = ++p->last_inst_num;
    inst->op_type = (OpType)trace_rec.op_type;

    inst->dest_reg = trace_rec.dest_needed ? trace_rec.dest_reg : -1;
    inst->src1_reg = trace_rec.src1_needed ? trace_rec.src1_reg : -1;
    inst->src2_reg = trace_rec.src2_needed ? trace_rec.src2_reg : -1;

    inst->dr_tag = -1;
    inst->src1_tag = -1;
    inst->src2_tag = -1;
    inst->src1_ready = false;
    inst->src2_ready = false;
    inst->exe_wait_cycles = 0;
}

/**
 * Allocate and initialize a new pipeline.
 * 
 * You should not need to modify this function.
 * 
 * @param trace_fd the file descriptor from which to read trace records
 * @return a pointer to a newly allocated pipeline
 */
Pipeline *pipe_init(int trace_fd)
{
    printf("\n** PIPELINE IS %d WIDE **\n\n", PIPE_WIDTH);

    // Allocate pipeline.
    Pipeline *p = (Pipeline *)calloc(1, sizeof(Pipeline));

    // Initialize pipeline.
    p->rat = rat_init();
    p->rob = rob_init();
    p->exeq = exeq_init();
    p->trace_fd = trace_fd;
    p->halt_inst_num = (uint64_t)(-1) - 3;

    for (unsigned int i = 0; i < PIPE_WIDTH; i++)
    {
        p->FE_latch[i].valid = false;
        p->ID_latch[i].valid = false;
        p->SC_latch[i].valid = false;
    }
    for (unsigned int i = 0; i < MAX_WRITEBACKS; i++)
    {
        p->EX_latch[i].valid = false;
    }

    return p;
}

/**
 * Commit the given instruction.
 * 
 * This updates counters and flags on the pipeline.
 * 
 * This function is implemented for you. You should not modify it.
 * 
 * @param p the pipeline to update.
 * @param inst the instruction to commit.
 */
void pipe_commit_inst(Pipeline *p, InstInfo inst)
{
    p->stat_retired_inst++;

    if (inst.inst_num >= p->halt_inst_num)
    {
        p->halt = true;
    }
}

/**
 * Print out the state of the pipeline for debugging purposes.
 * 
 * You may use this function to help debug your pipeline implementation, but
 * please remove calls to this function before submitting the lab.
 * 
 * @param p the pipeline
 */
void pipe_print_state(Pipeline *p)
{
    printf("\n");
    // Print table header
    for (unsigned int latch_type = 0; latch_type < 4; latch_type++)
    {
        switch (latch_type)
        {
        case 0:
            printf(" FE:    ");
            break;
        case 1:
            printf(" ID:    ");
            break;
        case 2:
            printf(" SCH:   ");
            break;
        case 3:
            printf(" EX:    ");
            break;
        default:
            printf(" ------ ");
        }
    }
    printf("\n");

    // Print row for each lane in pipeline width
    unsigned int ex_i = 0;
    for (unsigned int i = 0; i < PIPE_WIDTH; i++)
    {
        if (p->FE_latch[i].valid)
        {
            printf(" %6lu ",
                   (unsigned long)p->FE_latch[i].inst.inst_num);
        }
        else
        {
            printf(" ------ ");
        }
        if (p->ID_latch[i].valid)
        {
            printf(" %6lu ",
                   (unsigned long)p->ID_latch[i].inst.inst_num);
        }
        else
        {
            printf(" ------ ");
        }
        if (p->SC_latch[i].valid)
        {
            printf(" %6lu ",
                   (unsigned long)p->SC_latch[i].inst.inst_num);
        }
        else
        {
            printf(" ------ ");
        }
        bool flag = false;
        for (; ex_i < MAX_WRITEBACKS; ex_i++)
        {
            if (p->EX_latch[ex_i].valid)
            {
                printf(" %6lu ",
                       (unsigned long)p->EX_latch[ex_i].inst.inst_num);
                ex_i++;
                flag = true;
                break;
            }
        }
        if (!flag) {
            printf(" ------ ");
        }
        printf("\n");
    }
    printf("\n");

    rat_print_state(p->rat);
    exeq_print_state(p->exeq);
    rob_print_state(p->rob);
}

/**
 * Simulate one cycle of all stages of a pipeline.
 * 
 * You should not need to modify this function except for debugging purposes.
 * If you add code to print debug output in this function, remove it or comment
 * it out before you submit the lab.
 * 
 * @param p the pipeline to simulate
 */
void pipe_cycle(Pipeline *p)
{
    p->stat_num_cycle++;

    #ifdef DEBUG
        printf("\n--------------------------------------------\n");
        printf("Cycle count: %lu, retired instructions: %lu\n\n",
           (unsigned long)p->stat_num_cycle,
           (unsigned long)p->stat_retired_inst);
    #endif
    
    // In our simulator, stages are processed in reverse order.
    pipe_cycle_commit(p);
    pipe_cycle_writeback(p);
    pipe_cycle_exe(p);
    pipe_cycle_schedule(p);
    pipe_cycle_issue(p);
    pipe_cycle_decode(p);
    pipe_cycle_fetch(p);

    // Compile with "make debug" to have this show!
    #ifdef DEBUG
        pipe_print_state(p);
    #endif
}

/**
 * Simulate one cycle of the fetch stage of a pipeline.
 * 
 * This function is implemented for you. You should not modify it.
 * 
 * @param p the pipeline to simulate
 */
void pipe_cycle_fetch(Pipeline *p)
{
    for (unsigned int i = 0; i < PIPE_WIDTH; i++)
    {
        if (!p->FE_latch[i].stall && !p->FE_latch[i].valid)
        {
            // No stall and latch empty, so fetch a new instruction.
            pipe_fetch_inst(p, &p->FE_latch[i]);
        }
    }
}

/**
 * Simulate one cycle of the instruction decode stage of a pipeline.
 * 
 * This function is implemented for you. You should not modify it.
 * 
 * @param p the pipeline to simulate
 */
void pipe_cycle_decode(Pipeline *p)
{
    static uint64_t next_inst_num = 1;
    for (unsigned int i = 0; i < PIPE_WIDTH; i++)
    {
        if (!p->ID_latch[i].stall && !p->ID_latch[i].valid)
        {
            // No stall and latch empty, so decode the next instruction.
            // Loop to find the next in-order instruction.
            for (unsigned int j = 0; j < PIPE_WIDTH; j++)
            {
                if (p->FE_latch[j].valid &&
                    p->FE_latch[j].inst.inst_num == next_inst_num)
                {
                    p->ID_latch[i] = p->FE_latch[j];
                    p->FE_latch[j].valid = false;
                    next_inst_num++;
                    break;
                }
            }
        }
    }
}

/**
 * Simulate one cycle of the execute stage of a pipeline. This handles
 * instructions that take multiple cycles to execute.
 * 
 * This function is implemented for you. You should not modify it.
 * 
 * @param p the pipeline to simulate
 */
void pipe_cycle_exe(Pipeline *p)
{
    // If all operations are single-cycle, just copy SC latches to EX latches.
    if (LOAD_EXE_CYCLES == 1)
    {
        for (unsigned int i = 0; i < PIPE_WIDTH; i++)
        {
            if (p->SC_latch[i].valid)
            {
                p->EX_latch[i] = p->SC_latch[i];
                p->SC_latch[i].valid = false;
            }
        }
        return;
    }

    // Otherwise, we need to handle multi-cycle instructions with EXEQ.

    // All valid entries from the SC latches are inserted into the EXEQ.
    for (unsigned int i = 0; i < PIPE_WIDTH; i++)
    {
        if (p->SC_latch[i].valid)
        {
            if (!exeq_insert(p->exeq, p->SC_latch[i].inst))
            {
                fprintf(stderr, "Error: EXEQ full\n");
                p->halt = true;
                return;
            }

            p->SC_latch[i].valid = false;
        }
    }

    // Cycle the EXEQ to reduce wait time for each instruction by 1 cycle.
    exeq_cycle(p->exeq);

    // Transfer all finished entries from the EXEQ to the EX latch.
    for (unsigned int i = 0; i < MAX_WRITEBACKS && exeq_check_done(p->exeq); i++)
    {
        p->EX_latch[i].valid = true;
        p->EX_latch[i].stall = false;
        p->EX_latch[i].inst = exeq_remove(p->exeq);
    }
}

/**
 * Simulate one cycle of the issue stage of a pipeline: insert decoded
 * instructions into the ROB and perform register renaming.
 * 
 * You must implement this function in pipeline.cpp in part B of the
 * assignment.
 * 
 * @param p the pipeline to simulate
 */
void pipe_cycle_issue(Pipeline *p)
{
    
    //1 TODO: For each valid instruction from the ID stage:
    for (uint32_t i = 0; i < PIPE_WIDTH; i++)
    {  
        uint64_t oldest_num_ins;
        int oldest_id = -1;
        int inst_id=-1;
        
        for (uint32_t m = 0; m <PIPE_WIDTH; m++)
        {
            if(p->ID_latch[m].valid)
            {
                oldest_num_ins = p->ID_latch[m].inst.inst_num;
            }    
        }

        //uint64_t ID_young = -1;
        //1 TODO: If there is space in the ROB, insert the instruction.
        //1 TODO: Set the entry invalid in the previous latch when you do so.
        if(rob_check_space(p->rob))
        {            
            for (uint32_t j = 0; j < PIPE_WIDTH; j++)
            {   
                // find the youngest id already in id
                if(p->ID_latch[j].valid)
                {
                    //printf("i:%d, j:%d", i,j);
                    if (p->ID_latch[j].inst.inst_num<=oldest_num_ins)
                    {
                        //printf("test signal");
                        oldest_num_ins = p->ID_latch[j].inst.inst_num;
                        oldest_id = j;
                        if (p->ID_latch[oldest_id].valid==true)
                        {
                            //1 Insert the oldest ins into ROB
                            inst_id = rob_insert(p->rob,p->ID_latch[oldest_id].inst);
                            //1 Invalidate the ID stage ins
                            p->ID_latch[oldest_id].valid = false;
                        }
                        else
                        {
                            oldest_id = 1-oldest_id;
                        if (p->ID_latch[oldest_id].valid==true)
                        {
                            //1 Insert the oldest ins into ROB
                            inst_id = rob_insert(p->rob,p->ID_latch[oldest_id].inst);
                            //1 Invalidate the ID stage ins
                            p->ID_latch[oldest_id].valid = false;
                        }
                        }
                    }

                }
                
            }        
        }

        //1 TODO: Then, check RAT for this instruction's source operands:
        //1 If src1 is not needed
        if(inst_id!=-1)
        {
            if (p->rob->entries[inst_id].inst.src1_reg<0)
            {
                p->rob->entries[inst_id].inst.src1_ready = true;
                p->rob->entries[inst_id].inst.src1_tag=-1;
            }
            else
            {   
                //1 If we need src1
                //1 TODO: If src1 is not remapped, mark src1 as ready.        
                if (rat_get_remap(p->rat,p->rob->entries[inst_id].inst.src1_reg) == -1)
                {
                    p->rob->entries[inst_id].inst.src1_ready = true;
                    p->rob->entries[inst_id].inst.src1_tag = -1;
                }

                //1 TODO: If src1 is remapped, set src1 tag accordingly, and set src1 ready
                //1       according to whether the ROB entry with that tag is ready.
                //1 Src1 is remapped and RAT entry is valid
                else
                {
                    //1 set src1 tag
                    int prf_id = rat_get_remap(p->rat,p->rob->entries[inst_id].inst.src1_reg);
                    p->rob->entries[inst_id].inst.src1_tag = prf_id;

                    //Wrong code v1
                    //rat_set_remap(p->rat, p->rob->entries[inst_id].inst.src1_reg, 
                    //p->rob->entries[inst_id].inst.src1_tag);

                    //1 set src1 ready
                    //1 using prfid to check the position
                    if (rob_check_ready(p->rob,p->rob->entries[inst_id].inst.src1_tag))
                    {
                        p->rob->entries[inst_id].inst.src1_ready = true;
                        p->rob->entries[inst_id].inst.src1_tag=-1;
                    }
                    else
                    {

                        //1 set false if not prepared
                        p->rob->entries[inst_id].inst.src1_ready = false;
                    }
                }
            }
        


            //1 TODO: Then, check RAT for this instruction's source operands:
            //1 If src2 is not needed
            if (p->rob->entries[inst_id].inst.src2_reg<0)
            {
                p->rob->entries[inst_id].inst.src2_ready = true;
                p->rob->entries[inst_id].inst.src2_tag=-1;
            }
            else
            {   
                //1 If we need src2
                //1 TODO: If src2 is not remapped, mark src2 as ready.        
                if (rat_get_remap(p->rat,p->rob->entries[inst_id].inst.src2_reg) == -1)
                {
                    p->rob->entries[inst_id].inst.src2_ready = true;
                    p->rob->entries[inst_id].inst.src2_tag = -1;
                }

                //1 TODO: If src2 is remapped, set src2 tag accordingly, and set src2 ready
                //1       according to whether the ROB entry with that tag is ready.
                //1 src2 is remapped and RAT entry is valid
                else
                {
                    //1 set src2 tag
                    int prf_id = rat_get_remap(p->rat,p->rob->entries[inst_id].inst.src2_reg);
                    p->rob->entries[inst_id].inst.src2_tag = prf_id;

                    //Wrong code v1
                    //rat_set_remap(p->rat, p->rob->entries[inst_id].inst.src2_reg, 
                    //p->rob->entries[inst_id].inst.src2_tag);

                    //1 set src2 ready
                    //1 using prfid to check the position
                    if (rob_check_ready(p->rob,p->rob->entries[inst_id].inst.src2_tag))
                    {
                        p->rob->entries[inst_id].inst.src2_ready = true;
                        p->rob->entries[inst_id].inst.src2_tag=-1;
                    }
                    else
                    {

                        //1 set false if not prepared
                        p->rob->entries[inst_id].inst.src2_ready = false;
                    }
                }
            }

            //1 TODO: Set the tag for this instruction's destination register.
            //1 TODO: If this instruction writes to a register, update the RAT
            //       accordingly.
            //1 IF this ins does not write to somewhere
            if(p->rob->entries[inst_id].inst.dest_reg == -1)
            {
                p->rob->entries[inst_id].inst.dr_tag = -1;
            }
            //1 IF this ins does write
            else
            {
                rat_set_remap(p->rat, p->rob->entries[inst_id].inst.dest_reg, 
                inst_id);            
            }
        }
    }
}

/**
 * Simulate one cycle of the scheduling stage of a pipeline: schedule
 * instructions to execute if they are ready.
 * 
 * You must implement this function in pipeline.cpp in part B of the
 * assignment.
 * 
 * @param p the pipeline to simulate
 */
void pipe_cycle_schedule(Pipeline *p)
{
    //1 TODO: Implement two scheduling policies:
    //1 TODO: Repeat for each lane of the pipeline.
    //1 So that you only need 1 ins for one cycle
    for (uint32_t line = 0; line < PIPE_WIDTH; line++)    
    {
        if (SCHED_POLICY == SCHED_IN_ORDER)
        {
            //1 In-order scheduling:
            //1 TODO: Find the oldest valid entry in the ROB that is not already
            //1       executing.
            //1 ?????????????????????? how about all the entries are executing
            //1 Skip
            uint32_t new_tag=-1; 
            for (uint32_t i = 0; i < NUM_ROB_ENTRIES; i++)
            {
                //1 Start from head
                new_tag = p->rob->head_ptr+i;
                //1 round off
                if (new_tag>=NUM_ROB_ENTRIES)
                {
                    new_tag-=NUM_ROB_ENTRIES;
                }
                //1 If exec is false, we find it
                //1 If exec is true, still find it
                if(p->rob->entries[new_tag].valid)
                {
                    if (!p->rob->entries[new_tag].exec)
                    {
                        break;
                    }
                }
                new_tag = -1;
            }
            if (new_tag!=uint32_t(-1))
            {
            //1 TODO: Check if it is stalled, i.e., if at least one source operand
            //1       is not ready.
                bool scr1_isready = p->rob->entries[new_tag].inst.src1_ready;
                bool scr2_isready = p->rob->entries[new_tag].inst.src2_ready;
                //1 if both are ready, begin to mark
                if (scr1_isready && scr2_isready)
                {
                    //1  TODO: Otherwise, mark it as executing in the ROB and send it to the
                    //1        next latch.            
                    if (p->SC_latch[line].valid==false)
                    {
                        p->SC_latch[line].valid = true;
                        p->SC_latch[line].inst = p->rob->entries[new_tag].inst;
                        // if (p->rob->entries[new_tag].valid==true)                       
                        //     printf("test 0 for new tag ins valid or not");
                        //after test, rob mark exe should be called but not correctly
                        rob_mark_exec(p->rob,p->rob->entries[new_tag].inst);
                    }        
                                
                }
                //1 if not ready, because of in order, Stall
                else
                {
                    //1 TODO: If so, stop scheduling instructions.
                    //1 how
                    //1 Do nothing?????????????????????????????????
                }
            }



        }

        if (SCHED_POLICY == SCHED_OUT_OF_ORDER)
        {
            //1 Out-of-order scheduling:
            //1 TODO: Find the oldest valid entry in the ROB that has both source
            //       operands ready but is not already executing.
            uint32_t new_tag;
            //1 for the entries in rob            
            for (uint32_t i = 0; i < NUM_ROB_ENTRIES; i++)
            {
                //1 find the new id and round off
                new_tag = p->rob->head_ptr+i;
                if (new_tag>=NUM_ROB_ENTRIES)
                {
                    new_tag-=NUM_ROB_ENTRIES;
                }
                //1 check executing state
                //1 false is good
                if(p->rob->entries[new_tag].valid)
                {
                    if (!p->rob->entries[new_tag].exec)
                    {   
                        //1 check the ready state of two src
                        if (p->rob->entries[new_tag].inst.src1_ready 
                        && p->rob->entries[new_tag].inst.src2_ready)
                        {
                            break;
                        }
                    }
                }
                new_tag = -1;
            }
            //make sure after check all the ins we do not 
            //find any, so that should stall
            if (new_tag!=uint32_t(-1))
            {
                //1  TODO: Mark it as executing in the ROB and send it to the next latch.
                //1 find the sc latch position
                if (p->SC_latch[line].valid==false)
                {
                    p->SC_latch[line].valid = true;
                    p->SC_latch[line].inst = p->rob->entries[new_tag].inst;
                    rob_mark_exec(p->rob,p->rob->entries[new_tag].inst);
                }              
                // TODO: Repeat for each lane of the pipeline.
                // for all the lines
            }
        }
    }
}

/**
 * Simulate one cycle of the writeback stage of a pipeline: update the ROB
 * with information from instructions that have finished executing.
 * 
 * You must implement this function in pipeline.cpp in part B of the
 * assignment.
 * 
 * @param p the pipeline to simulate
 */
void pipe_cycle_writeback(Pipeline *p)
{
    //1 TODO: For each valid instruction from the EX stage:
    //1 Using MaxWriteBack
    for (int i = 0; i < MAX_WRITEBACKS; i++)
    {
        if (p->EX_latch[i].valid)
        {
            //1 TODO: Update the ROB: mark the instruction ready to commit.            
            rob_mark_ready(p->rob,p->EX_latch[i].inst);

            //1 TODO: Broadcast the result to all ROB entries.
            rob_wakeup(p->rob,p->EX_latch[i].inst.dr_tag); 

            //1 TODO: Invalidate the instruction in the previous latch.
            p->EX_latch[i].valid = false;
        }
    }
    // Remember: how many instructions can the EX stage send to the WB stage
    // in one cycle?
    //1 MAX_WRITEBACKS
}

/**
 * Simulate one cycle of the commit stage of a pipeline: commit instructions
 * in the ROB that are ready to commit.
 * 
 * You must implement this function in pipeline.cpp in part B of the
 * assignment.
 * 
 * @param p the pipeline to simulate
 */
void pipe_cycle_commit(Pipeline *p)
{
    //1 MAX commit limitation
    for (uint32_t i = 0; i < PIPE_WIDTH; i++)
    { 
        // TODO: Check if the instruction at the head of the ROB is ready to
        //       commit.
        if (rob_check_head(p->rob))
        {
            //1 TODO: If so, remove it from the ROB.
            InstInfo removed_ins = rob_remove_head(p->rob);
            //1 TODO: Commit that instruction.
            pipe_commit_inst(p, removed_ins);
            // TODO: If a RAT mapping exists and is still relevant, update the RAT.    
            // how to decide whether it is relevant or not
            if (removed_ins.dest_reg!=-1)
            {
                if(p->rat->entries[removed_ins.dest_reg].prf_id
                ==uint64_t(removed_ins.dr_tag))
                {
                    //printf("do commit RAT");
                    rat_reset_entry(p->rat,removed_ins.dest_reg);
                }
            }
        }
    }
    // TODO: Repeat for each lane of the pipeline.

    // // The following code is DUMMY CODE to ensure that the base code compiles
    // // and that the simulation terminates. Replace it with a correct
    // // implementation!
    // for (unsigned int i = 0; i < PIPE_WIDTH; i++)
    // {
    //     if (p->FE_latch[i].valid)
    //     {
    //         pipe_commit_inst(p, p->FE_latch[i].inst);
    //         p->FE_latch[i].valid = false;
    //     }
    // }
}
