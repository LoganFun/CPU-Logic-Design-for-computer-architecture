// --------------------------------------------------------------------- //
// For part B, you will need to modify this file.                        //
// You may add any code you need, as long as you correctly implement the //
// three required BPred methods already listed in this file.             //
// --------------------------------------------------------------------- //

// bpred.cpp
// Implements the branch predictor class.

#include "bpred.h"
#include <iostream>
#include <vector>
#include <bitset>

/**
 * Construct a branch predictor with the given policy.
 * 
 * In part B of the lab, you must implement this constructor.
 * 
 * @param policy the policy this branch predictor should use
 */
BPred::BPred(BPredPolicy policy)
{
    // TODO: Initialize member variables here.
    this->policy = policy;
    stat_num_branches = 0;
    stat_num_mispred = 0;
    GHR = 0;
    PHT.resize(entries, 2);
    // As a reminder, you can declare any additional member variables you need
    // in the BPred class in bpred.h and initialize them here.
}

/**
 * Get a prediction for the branch with the given address.
 * 
 * In part B of the lab, you must implement this method.
 * 
 * @param pc the address (program counter) of the branch to predict
 * @return the prediction for whether the branch is taken or not taken
 */
BranchDirection BPred::predict(uint64_t pc)
{
    // TODO: Return a prediction for whether the branch at address pc will be
    // TAKEN or NOT_TAKEN according to this branch predictor's policy.

    // Note that you do not have to handle the BPRED_PERFECT policy here; this
    // function will not be called for that policy.    
    if (policy == BPRED_ALWAYS_TAKEN)
    {
        return TAKEN;
    }
    else if(policy == BPRED_GSHARE)
    {
        //get lower 12 bits of pc
        uint32_t lower_12_PC = pc & mask;
        //XOR
        xor_result = GHR ^ lower_12_PC;
        //return section
        if(PHT[xor_result]>1)
        {
            return TAKEN;
        }
        else
        {
            return NOT_TAKEN;
        }
    }
    // This is just a placeholder.
    return TAKEN;
}


/**
 * Update the branch predictor statistics (stat_num_branches and
 * stat_num_mispred), as well as any other internal state you may need to
 * update in the branch predictor.
 * 
 * In part B of the lab, you must implement this method.
 * 
 * @param pc the address (program counter) of the branch
 * @param prediction the prediction made by the branch predictor
 * @param resolution the actual outcome of the branch
 */
void BPred::update(uint64_t pc, BranchDirection prediction,
                   BranchDirection resolution)
{
    // TODO: Update the stat_num_branches and stat_num_mispred member variables
    // according to the prediction and resolution of the branch.
    stat_num_branches++;
    //std::cout<<"branches ++ "<<std::endl;
    if(prediction != resolution) 
    {
        //std::cout<<"mis ++ "<<std::endl;
        stat_num_mispred++;
    }

    // TODO: Update any other internal state you may need to keep track of.
    // Update GHR
    if (resolution== NOT_TAKEN)
    {
        GHR = GHR << 1;
        GHR = GHR & mask;
    }
    else
    {
        GHR = (GHR << 1);
        GHR += 1;
        GHR = GHR & mask;
    }
    
    // Update PHT
    old_state = PHT[xor_result];
    if(resolution)
    {
        new_state = sat_increment(old_state,max);
    }
    else
    {
        new_state = sat_decrement(old_state);
    }

    PHT[xor_result] = new_state;

    // Note that you do not have to handle the BPRED_PERFECT policy here; this
    // function will not be called for that policy.
}
