// --------------------------------------------------------------------- //
// You will need to modify this file.                                    //
// You may add any code you need, as long as you correctly implement the //
// required pipe_cycle_*() functions already listed in this file.        //
// In part B, you will also need to implement pipe_check_bpred().        //
// --------------------------------------------------------------------- //

// pipeline.cpp
// Implements functions to simulate a pipelined processor.

#include "pipeline.h"
#include <cstdlib>
#include <stdio.h>
#include <unistd.h>
#include <iostream>

/**
 * Read a single trace record from the trace file and use it to populate the
 * given fetch_op.
 * 
 * You should not modify this function.
 * 
 * @param p the pipeline whose trace file should be read
 * @param fetch_op the PipelineLatch struct to populate
 */
void pipe_get_fetch_op(Pipeline *p, PipelineLatch *fetch_op)
{
    TraceRec *trace_rec = &fetch_op->trace_rec;
    uint8_t *trace_rec_buf = (uint8_t *)trace_rec;
    size_t bytes_read_total = 0;
    ssize_t bytes_read_last = 0;
    size_t bytes_left = sizeof(*trace_rec);

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
    if (bytes_left > 0 || trace_rec->op_type >= NUM_OP_TYPES)
    {
        fetch_op->valid = false;
        p->halt_op_id = p->last_op_id;

        if (p->last_op_id == 0)
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
    fetch_op->valid = true;
    fetch_op->stall = false;
    fetch_op->is_mispred_cbr = false;
    fetch_op->op_id = ++p->last_op_id;
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
    p->trace_fd = trace_fd;
    p->halt_op_id = (uint64_t)(-1) - 3;
    //p->halt_op_id = 100;

    // Allocate and initialize a branch predictor if needed.
    if (BPRED_POLICY != BPRED_PERFECT)
    {
        p->b_pred = new BPred(BPRED_POLICY);
    }

    return p;
}

/**
 * Print out the state of the pipeline latches for debugging purposes.
 * 
 * You may use this function to help debug your pipeline implementation, but
 * please remove calls to this function before submitting the lab.
 * 
 * @param p the pipeline
 */
void pipe_print_state(Pipeline *p)
{
    printf("\n--------------------------------------------\n");
    printf("Cycle count: %lu, retired instructions: %lu\n",
           (unsigned long)p->stat_num_cycle,
           (unsigned long)p->stat_retired_inst);

    // Print table header
    for (uint8_t latch_type = 0; latch_type < NUM_LATCH_TYPES; latch_type++)
    {
        switch (latch_type)
        {
        case IF_LATCH:
            printf(" IF:    ");
            break;
        case ID_LATCH:
            printf(" ID:    ");
            break;
        case EX_LATCH:
            printf(" EX:    ");
            break;
        case MA_LATCH:
            printf(" MA:    ");
            break;
        default:
            printf(" ------ ");
        }
    }
    printf("\n");

    // Print row for each lane in pipeline width
    for (uint8_t i = 0; i < PIPE_WIDTH; i++)
    {
        for (uint8_t latch_type = 0; latch_type < NUM_LATCH_TYPES;
             latch_type++)
        {
            if (p->pipe_latch[latch_type][i].valid)
            {
                printf(" %6lu ",
                       (unsigned long)p->pipe_latch[latch_type][i].op_id);
            }
            else
            {
                printf(" ------ ");
            }
        }
        printf("\n");
    }
    printf("\n");


// Print row for each lane in pipeline width
for (uint8_t i = 0; i < PIPE_WIDTH; i++)
{
for (uint8_t latch_type = 0; latch_type < NUM_LATCH_TYPES;
latch_type++)
{
if (p->pipe_latch[latch_type][i].valid)
{
printf(" %6lu ",
(unsigned long)p->pipe_latch[latch_type][i].op_id);
}
else
{
printf(" ------ ");
}
}
printf("\n");
}
for (uint8_t i = 0; i < PIPE_WIDTH; i++)
{
for (uint8_t latch_type = 0; latch_type < NUM_LATCH_TYPES;
latch_type++)
{
if (p->pipe_latch[latch_type][i].valid)
{
int dest = (p->pipe_latch[latch_type][i].trace_rec.dest_needed) ? 
p->pipe_latch[latch_type][i].trace_rec.dest_reg : -1;
int src1 = (p->pipe_latch[latch_type][i].trace_rec.src1_needed) ? 
p->pipe_latch[latch_type][i].trace_rec.src1_reg : -1;
int src2 = (p->pipe_latch[latch_type][i].trace_rec.src2_needed) ? 
p->pipe_latch[latch_type][i].trace_rec.src2_reg : -1;
int cc_read = p->pipe_latch[latch_type][i].trace_rec.cc_read;
int cc_write = p->pipe_latch[latch_type][i].trace_rec.cc_write;
const char *op_type;
if (p->pipe_latch[latch_type][i].trace_rec.op_type == OP_ALU)
op_type = "ALU";
else if (p->pipe_latch[latch_type][i].trace_rec.op_type == OP_LD)
op_type = "LD";
else if (p->pipe_latch[latch_type][i].trace_rec.op_type == OP_ST)
op_type = "ST";
else if (p->pipe_latch[latch_type][i].trace_rec.op_type == OP_CBR)
op_type = "BR";
else if (p->pipe_latch[latch_type][i].trace_rec.op_type == OP_OTHER)
op_type = "OTHER";

printf("(%lu : %s) dest: %d, src1: %d, src2: %d , ccread: %d, ccwrite: %d\n",
(unsigned long)p->pipe_latch[latch_type][i].op_id,
op_type,
dest,
src1,
src2,
cc_read,
cc_write);
}
 else
{
 printf(" ------ \n");
}
}
 printf("\n");
 }
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

    // In hardware, all pipeline stages execute in parallel, and each pipeline
    // latch is populated at the start of the next clock cycle.

    // In our simulator, we simulate the pipeline stages one at a time in
    // reverse order, from the Write Back stage (WB) to the Fetch stage (IF).
    // We do this so that each stage can read from the latch before it and
    // write to the latch after it without needing to "double-buffer" the
    // latches.

    // Additionally, it means that earlier pipeline stages can know about
    // stalls triggered in later pipeline stages in the same cycle, as would be
    // the case with hardware stall signals asserted by combinational logic.

    pipe_cycle_WB(p);
    pipe_cycle_MA(p);
    pipe_cycle_EX(p);
    pipe_cycle_ID(p);
    pipe_cycle_IF(p);

    // You can uncomment the following line to print out the pipeline state
    // after each clock cycle for debugging purposes.
    // Make sure you comment it out or remove it before you submit the lab.
    //pipe_print_state(p);
    //std::cout<<std::endl;
    
}

/**
 * Simulate one cycle of the Write Back stage (WB) of a pipeline.
 * 
 * Some skeleton code has been provided for you. You must implement anything
 * else you need for the pipeline simulation to work properly.
 * 
 * @param p the pipeline to simulate
 */
void pipe_cycle_WB(Pipeline *p)
{   
    //std::cout<<"Enter WB Funciton"<<std::endl;
    for (unsigned int i = 0; i < PIPE_WIDTH; i++)
    {
        if (p->pipe_latch[MA_LATCH][i].valid==true)
        {
            p->stat_retired_inst++;
            //std::cout<<"Done WB Funciton"<<std::endl;

            if (p->pipe_latch[MA_LATCH][i].op_id >= p->halt_op_id)
            {
                // Halt the pipeline if we've reached the end of the trace.
                p->halt = true;
            }
        }
    }
    //std::cout<<"Finish WB Funciton"<<std::endl;
}

/**
 * Simulate one cycle of the Memory Access stage (MA) of a pipeline.
 * 
 * Some skeleton code has been provided for you. You must implement anything
 * else you need for the pipeline simulation to work properly.
 * 
 * @param p the pipeline to simulate
 */
void pipe_cycle_MA(Pipeline *p)
{
    //std::cout<<"Enter MA Funciton"<<std::endl;
    for (unsigned int i = 0; i < PIPE_WIDTH; i++)
    {
        if (p->pipe_latch[EX_LATCH][i].valid==true)
        {
            // Copy each instruction from the EX latch to the MA latch.
            p->pipe_latch[MA_LATCH][i] = p->pipe_latch[EX_LATCH][i];
            p->pipe_latch[MA_LATCH][i].valid=true;
            //std::cout<<"current MA instruction: "<<p->pipe_latch[MA_LATCH][i].op_id<<std::endl;
            //std::cout<<"Done MA Funciton True"<<std::endl;          
        }
        else
        {
            p->pipe_latch[MA_LATCH][i].valid = false;
            p->pipe_latch[MA_LATCH][i].trace_rec.dest_needed=0;
            p->pipe_latch[MA_LATCH][i].trace_rec.cc_write=0;
            //std::cout<<"Done MA Funciton False"<<std::endl;
        }

    }
    //std::cout<<"Finish MA Funciton"<<std::endl;
}

/**
 * Simulate one cycle of the Execute stage (EX) of a pipeline.
 * 
 * Some skeleton code has been provided for you. You must implement anything
 * else you need for the pipeline simulation to work properly.
 * 
 * @param p the pipeline to simulate
 */
void pipe_cycle_EX(Pipeline *p)
{   

    //std::cout<<"Enter EX Funciton"<<std::endl;
    for (unsigned int i = 0; i < PIPE_WIDTH; i++)
    {
        //std::cout<<"EX stage stall signal: "<<p->pipe_latch[ID_LATCH][i].stall<<std::endl;
        //detect whether there is a stall signal
        if(p->pipe_latch[ID_LATCH][i].stall==0||p->stat_num_cycle<4)
        {
            // Copy each instruction from the ID latch to the EX latch.
            p->pipe_latch[EX_LATCH][i].valid = true;
            p->pipe_latch[EX_LATCH][i] = p->pipe_latch[ID_LATCH][i];  
            //std::cout<<"Done EX Funciton True"<<std::endl;          
        }
        else 
        {   
            p->pipe_latch[EX_LATCH][i].valid = false; 
            p->pipe_latch[EX_LATCH][i].trace_rec.dest_needed=0;
            p->pipe_latch[EX_LATCH][i].trace_rec.cc_write=0;
            //std::cout<<"Done EX Funciton False"<<std::endl;
        }
        
    }
    //std::cout<<"Finish EX Funciton"<<std::endl;
}

/**
 * Simulate one cycle of the Instruction Decode stage (ID) of a pipeline.
 * 
 * Some skeleton code has been provided for you. You must implement anything
 * else you need for the pipeline simulation to work properly.
 * 
 * @param p the pipeline to simulate
 */
void pipe_cycle_ID(Pipeline *p)
{   
    //int ID_Hazard[PIPE_WIDTH];
    //uint64_t hazard_opid = 0;
    //ENABLE_EXE_FWD=1;
    //ENABLE_MEM_FWD=1; 
    uint64_t EX_Final_op_id=0;

    //Enter ID Function once
    //std::cout<<"Enter ID Funciton"<<std::endl;

    //For all the lines to test
    for (unsigned int i = 0; i < PIPE_WIDTH; i++)
    {   
        //Initial the pipeline
        if(p->stat_num_cycle<3)
        {   
            //std::cout<<"Initial Pipeline Stage"<<std::endl;
            p->pipe_latch[ID_LATCH][i] = p->pipe_latch[IF_LATCH][i];
            //std::cout<<"Done ID Funciton Initialization"<<std::endl;
        }

        //Detect harzard in ID stage After Input
        else 
        {   
            if(p->pipe_latch[ID_LATCH][i].stall == 0)
            {   
                p->pipe_latch[ID_LATCH][i] = p->pipe_latch[IF_LATCH][i];
            }

            //Stall Signal Initialization
            p->pipe_latch[ID_LATCH][i].stall = 0;
            EX_Final_op_id=0;
            for (unsigned int k = 0; k < PIPE_WIDTH; k++)
            {
                //ID_Hazard[k] = 0;
            }
            //hazard_opid = 0;
            
            //Test all the pipelines
            for(unsigned int j = 0; j<PIPE_WIDTH; j++)
            {
                if(p->pipe_latch[ID_LATCH][i].trace_rec.src1_needed==1)
                {
                    //Reg hazard between MA stage
                    if(p->pipe_latch[MA_LATCH][j].trace_rec.dest_needed==1)
                    {
                        if(p->pipe_latch[ID_LATCH][i].trace_rec.src1_reg==
                        p->pipe_latch[MA_LATCH][j].trace_rec.dest_reg                       
                        )
                        {
                            
                            // std::cout<<"Done ID Funciton"<<std::endl;
                            if(ENABLE_MEM_FWD==0)
                            {
                                p->pipe_latch[ID_LATCH][i].stall = 1;
                                //std::cout<<"Hazard 1 scr1 MA stage"<<std::endl;
                            }
                            else
                            {
                                p->pipe_latch[ID_LATCH][i].stall = 0;
                            }                                
                        }
                        else if(p->pipe_latch[ID_LATCH][i].trace_rec.src2_needed==1)
                        {
                            if(p->pipe_latch[ID_LATCH][i].trace_rec.src2_reg==
                            p->pipe_latch[MA_LATCH][j].trace_rec.dest_reg )
                            {
                                //std::cout<<"Hazard 1 scr2 MA stage"<<std::endl;
                                //std::cout<<"Done ID Funciton"<<std::endl;
                                if(ENABLE_MEM_FWD==0)
                                {
                                    p->pipe_latch[ID_LATCH][i].stall = 1;
                                    //std::cout<<"Hazard 1 scr2 MA stage"<<std::endl;
                                }
                                else
                                {
                                    p->pipe_latch[ID_LATCH][i].stall = 0;
                                }
                            }
                        }
                    }

                    //Reg hazard between EX stage
                    if(p->pipe_latch[EX_LATCH][j].trace_rec.dest_needed==1)
                    {
                        if(p->pipe_latch[ID_LATCH][i].trace_rec.src1_reg==
                        p->pipe_latch[EX_LATCH][j].trace_rec.dest_reg                       
                        )
                        {   
                            if (ENABLE_EXE_FWD==0)
                            {
                                p->pipe_latch[ID_LATCH][i].stall = 1;
                            }
                            else 
                            {
                                if (EX_Final_op_id < p->pipe_latch[EX_LATCH][i].op_id)
                                {
                                    EX_Final_op_id = p->pipe_latch[EX_LATCH][i].op_id ;
                                    
                                }
                                p->pipe_latch[ID_LATCH][i].stall = 1;
                            }
                        }
                        else if(p->pipe_latch[ID_LATCH][i].trace_rec.src2_needed)
                        {
                            if(p->pipe_latch[ID_LATCH][i].trace_rec.src2_reg==
                            p->pipe_latch[EX_LATCH][j].trace_rec.dest_reg )
                            {
                                if (ENABLE_EXE_FWD==0)
                                {
                                    p->pipe_latch[ID_LATCH][i].stall = 1;
                                }
                                else 
                                {
                                    if (EX_Final_op_id < p->pipe_latch[EX_LATCH][i].op_id)
                                    {
                                        EX_Final_op_id = p->pipe_latch[EX_LATCH][i].op_id ;
                                        
                                    }
                                    p->pipe_latch[ID_LATCH][i].stall = 1;
                                }
                            }
                        }
                    }
                }

                //CC hazard detection
                if(p->pipe_latch[ID_LATCH][i].trace_rec.cc_read==1)
                {
                    if(p->pipe_latch[MA_LATCH][j].trace_rec.cc_write==1)
                    {
                        
                        //std::cout<<"Done ID Funciton"<<std::endl;
                        if(ENABLE_MEM_FWD==0)
                        {
                            //std::cout<<"CC Hazard 2 MA stage"<<std::endl;
                            p->pipe_latch[ID_LATCH][i].stall = 1;
                        }
                        else
                            p->pipe_latch[ID_LATCH][i].stall = 0;
                    }   

                    if(p->pipe_latch[EX_LATCH][j].trace_rec.cc_write==1)
                    {
                        //std::cout<<"CC Hazard 2 EX stage"<<std::endl;
                        //std::cout<<"Done ID Funciton"<<std::endl;
                        if (ENABLE_EXE_FWD==0)
                        {
                            p->pipe_latch[ID_LATCH][i].stall = 1;
                        }                        
                        else
                        {
                            if (EX_Final_op_id < p->pipe_latch[EX_LATCH][i].op_id)
                            {
                                EX_Final_op_id = p->pipe_latch[EX_LATCH][i].op_id;
                            }
                            p->pipe_latch[ID_LATCH][i].stall = 1;
                        }
                    }    
                }
            }
        }

        //EXE
        if (ENABLE_EXE_FWD && (EX_Final_op_id!=0))
        {            
            for(unsigned int j = 0; j < PIPE_WIDTH; j++)
            {
                if (p->pipe_latch[EX_LATCH][j].op_id==EX_Final_op_id)
                {
                    if (p->pipe_latch[EX_LATCH][j].trace_rec.op_type != OP_LD
                    )
                    {
                        //std::cout<<"Optype Stall Cancel"<<std::endl;
                        p->pipe_latch[ID_LATCH][i].stall = 0;
                    }
                }              
            }
        }
    }

    for (unsigned int i = 0; i < PIPE_WIDTH; i++)
    {
        for(unsigned int j = 0; j < PIPE_WIDTH; j++)
        {  
               //Hazard between ID
                if(i!=j)
                {
                    
                    if(p->pipe_latch[ID_LATCH][i].op_id > p->pipe_latch[ID_LATCH][j].op_id)
                    {
                        //dest hazard
                        if (p->pipe_latch[ID_LATCH][j].trace_rec.dest_needed==1)
                        {   
                            if(p->pipe_latch[ID_LATCH][i].trace_rec.src1_needed==1){
                                if(p->pipe_latch[ID_LATCH][i].trace_rec.src1_reg==
                                p->pipe_latch[ID_LATCH][j].trace_rec.dest_reg)    
                                {
                                    p->pipe_latch[ID_LATCH][i].stall=1;
                                    //ID_Hazard[j]=1;
                                    //std::cout<<"ID src1 Hazard ID stage"<<std::endl;
                                }
                            }     

                            if(p->pipe_latch[ID_LATCH][i].trace_rec.src2_needed==1){
                                if(p->pipe_latch[ID_LATCH][i].trace_rec.src2_reg==
                                p->pipe_latch[ID_LATCH][j].trace_rec.dest_reg)    
                                {
                                    p->pipe_latch[ID_LATCH][i].stall=1;
                                    //ID_Hazard[j]=1;
                                    //std::cout<<"ID src2 Hazard ID stage"<<std::endl;
                                }
                            }                                                
                        }
                        
                        
                        //cc hazard
                        if (p->pipe_latch[ID_LATCH][i].trace_rec.cc_read==1)
                        {
                            if(p->pipe_latch[ID_LATCH][j].trace_rec.cc_write==1)
                            {
                                
                                p->pipe_latch[ID_LATCH][i].stall=1;
                                //ID_Hazard[j]=1;
                                //std::cout<<"ID cc Hazard ID stage"<<std::endl;
                                //std::cout<<p->pipe_latch[ID_LATCH][i].op_id<<std::endl;
                                //std::cout<<p->pipe_latch[ID_LATCH][j].op_id<<std::endl;
                            }
                        }
                        
                    }
                }
        }
    }

    for (unsigned int i = 0; i < PIPE_WIDTH; i++)
    {
        for(unsigned int j = 0; j < PIPE_WIDTH; j++)
        {   
            if(i!=j)
            {
                //youger instruction stall
                if(p->pipe_latch[ID_LATCH][j].stall==1)
                {
                    if (p->pipe_latch[ID_LATCH][i].op_id > p->pipe_latch[ID_LATCH][j].op_id)
                    {
                        p->pipe_latch[ID_LATCH][i].stall=1;
                        //std::cout<<"ID Hazard Youger Stall"<<std::endl;
                    }
                    
                }
            }
        }      
    }
    //std::cout<<"Finish ID Funciton"<<std::endl;
}

/**
 * Simulate one cycle of the Instruction Fetch stage (IF) of a pipeline.
 * 
 * Some skeleton code has been provided for you. You must implement anything
 * else you need for the pipeline simulation to work properly.
 * 
 * @param p the pipeline to simulate
 */
void pipe_cycle_IF(Pipeline *p)
{
    //std::cout<<"Enter IF Funciton"<<std::endl;
    for (unsigned int i = 0; i < PIPE_WIDTH; i++)
    {
        //std::cout<<"Detected ID Stall Signal: "<<p->pipe_latch[ID_LATCH][i].stall<<std::endl;

        if(p->pipe_latch[ID_LATCH][i].stall == 0)
        {
            if(p->pipe_latch[IF_LATCH][i].stall==0)
            {
                // Read an instruction from the trace file.
                PipelineLatch fetch_op;
                pipe_get_fetch_op(p, &fetch_op);

                // Handle branch (mis)prediction.
                if (BPRED_POLICY != BPRED_PERFECT)
                {
                    pipe_check_bpred(p, &fetch_op);
                }

                // Copy the instruction to the IF latch.
                p->pipe_latch[IF_LATCH][i] = fetch_op;

                //std::cout<<"Done IF Funciton with both 0 stall signal"<<std::endl;
            }

            p->pipe_latch[IF_LATCH][i].stall=0;

        }
        else 
        {   
            if(p->pipe_latch[IF_LATCH][i].stall == 1)
            {
                
            }
            else
            {

                // Read an instruction from the trace file.
                PipelineLatch fetch_op;
                pipe_get_fetch_op(p, &fetch_op);

                // Handle branch (mis)prediction.
                if (BPRED_POLICY != BPRED_PERFECT)
                {
                    pipe_check_bpred(p, &fetch_op);
                }

                // Copy the instruction to the IF latch.
                p->pipe_latch[IF_LATCH][i] = fetch_op;
                
                //std::cout<<"Done IF Funciton"<<std::endl;

            }

            p->pipe_latch[IF_LATCH][i].stall = 1;

        }
        //std::cout<<"IF Stall Signal: "<<p->pipe_latch[IF_LATCH][i].stall<<std::endl;
    }
    
    //std::cout<<"Finish IF Funciton"<<std::endl;
}

/**
 * If the instruction just fetched is a conditional branch, check for a branch
 * misprediction, update the branch predictor, and set appropriate flags in the
 * pipeline.
 * 
 * You must implement this function in part B of the lab.
 * 
 * @param p the pipeline
 * @param fetch_op the pipeline latch containing the operation fetched
 */
void pipe_check_bpred(Pipeline *p, PipelineLatch *fetch_op)
{
    // TODO: For a conditional branch instruction, get a prediction from the
    // branch predictor.

    // TODO: If the branch predictor mispredicted, mark the fetch_op
    // accordingly.

    // TODO: Immediately update the branch predictor.

    // TODO: If needed, stall the IF stage by setting the flag
    // p->fetch_cbr_stall.
}
