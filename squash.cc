#include "pipeline.h"
#include<inttypes.h>
#include <bits/stdc++.h> 
void pipeline_t::squash_complete(reg_t jump_PC) {
	unsigned int i, j;

	//////////////////////////
	// Fetch Stage
	//////////////////////////
  
	FetchUnit->flush(jump_PC);

	//////////////////////////
	// Decode Stage
	//////////////////////////

	for (i = 0; i < fetch_width; i++) {
		DECODE[i].valid = false;
	}

	//////////////////////////
	// Rename1 Stage
	//////////////////////////

	FQ.flush();

	//////////////////////////
	// Rename2 Stage
	//////////////////////////

	for (i = 0; i < dispatch_width; i++) {
		RENAME2[i].valid = false;
	}

        //
        // FIX_ME #17c
        // Squash the renamer.
        //

        // FIX_ME #17c BEGIN
		REN->squash();
        // FIX_ME #17c END


	//////////////////////////
	// Dispatch Stage
	//////////////////////////

	for (i = 0; i < dispatch_width; i++) {
		DISPATCH[i].valid = false;
	}

	//////////////////////////
	// Schedule Stage
	//////////////////////////

	IQ.flush();

	//////////////////////////
	// Register Read Stage
	// Execute Stage
	// Writeback Stage
	//////////////////////////

	for (i = 0; i < issue_width; i++) {
		Execution_Lanes[i].rr.valid = false;
		for (j = 0; j < Execution_Lanes[i].ex_depth; j++)
		   Execution_Lanes[i].ex[j].valid = false;
		Execution_Lanes[i].wb.valid = false;
	}

	LSU.flush();
}


void pipeline_t::selective_squash(uint64_t squash_mask) {
	unsigned int i, j;

	/*if (correct) {
		// Instructions in the Rename2 through Writeback Stages have branch masks.
		// The correctly-resolved branch's bit must be cleared in all branch masks.

		for (i = 0; i < dispatch_width; i++) {
			// Rename2 Stage:
			CLEAR_BIT(RENAME2[i].chkpt_id, squash_mask);

			// Dispatch Stage:
			CLEAR_BIT(DISPATCH[i].chkpt_id, squash_mask);
		}

		// Schedule Stage:
		IQ.clear_branch_bit(squash_mask);

		for (i = 0; i < issue_width; i++) {
			// Register Read Stage:
			CLEAR_BIT(Execution_Lanes[i].rr.chkpt_id, squash_mask);

			// Execute Stage:
			for (j = 0; j < Execution_Lanes[i].ex_depth; j++)
			   CLEAR_BIT(Execution_Lanes[i].ex[j].chkpt_id, squash_mask);

			// Writeback Stage:
			CLEAR_BIT(Execution_Lanes[i].wb.chkpt_id, squash_mask);
		}
	}*/
		// Squash all instructions in the Decode through Dispatch Stages.

		// Decode Stage:
		for (i = 0; i < fetch_width; i++) {
			DECODE[i].valid = false;
		}

		// Rename1 Stage:
		FQ.flush();

		// Rename2 Stage:
		for (i = 0; i < dispatch_width; i++) {
			RENAME2[i].valid = false;
		}

		// Dispatch Stage:
		for (i = 0; i < dispatch_width; i++) {
			DISPATCH[i].valid = false;
		}

		// Selectively squash instructions after the branch, in the Schedule through Writeback Stages.

		// Schedule Stage:
		IQ.squash(squash_mask);

		for (i = 0; i < issue_width; i++) {
			// Register Read Stage:
			if (Execution_Lanes[i].rr.valid &&(squash_mask<<(Execution_Lanes[i].rr.chkpt_id)&0x1)) {
				if(PAY.buf[Execution_Lanes[i].rr.index].A_valid)
				REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].rr.index].A_phys_reg);
				if(PAY.buf[Execution_Lanes[i].rr.index].B_valid)
				REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].rr.index].B_phys_reg);
				if(PAY.buf[Execution_Lanes[i].rr.index].D_valid)
				REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].rr.index].D_phys_reg);
				if(PAY.buf[Execution_Lanes[i].rr.index].C_valid)
				REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].rr.index].C_phys_reg);
				Execution_Lanes[i].rr.valid = false;
				
			}

			// Execute Stage:
			for (j = 0; j < Execution_Lanes[i].ex_depth; j++) {
			   if (Execution_Lanes[i].ex[j].valid && (squash_mask<<(Execution_Lanes[i].rr.chkpt_id)&0x1)) {
				if(PAY.buf[Execution_Lanes[i].ex[j].index].A_valid)
				REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].ex[j].index].A_phys_reg);
				if(PAY.buf[Execution_Lanes[i].ex[j].index].B_valid)
				REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].ex[j].index].B_phys_reg);
				if(PAY.buf[Execution_Lanes[i].ex[j].index].D_valid)
				REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].ex[j].index].D_phys_reg);
				if(PAY.buf[Execution_Lanes[i].ex[j].index].C_valid)
				REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].ex[j].index].C_phys_reg);
				Execution_Lanes[i].ex[j].valid = false;
			   }
			}

			// Writeback Stage:
			if (Execution_Lanes[i].wb.valid && (squash_mask<<(Execution_Lanes[i].rr.chkpt_id)&0x1)) {
				if(PAY.buf[Execution_Lanes[i].wb.index].A_valid)
				REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].wb.index].A_phys_reg);
				if(PAY.buf[Execution_Lanes[i].wb.index].B_valid)
				REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].wb.index].B_phys_reg);
				if(PAY.buf[Execution_Lanes[i].wb.index].D_valid)
				REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].wb.index].D_phys_reg);
				if(PAY.buf[Execution_Lanes[i].wb.index].C_valid)
				REN->dec_usage_counter(PAY.buf[Execution_Lanes[i].wb.index].C_phys_reg);
				Execution_Lanes[i].wb.valid = false;
			}
	}
}
