#include<inttypes.h>
#include<stdio.h>
#include "renamer.h"
#include <bits/stdc++.h> 
#include "pipeline.h"
using namespace std;

renamer::renamer(uint64_t n_log_regs, uint64_t n_phys_regs, uint64_t n_branches, uint64_t n_active):
		num_log_regs(n_log_regs),
		num_phys_regs(n_phys_regs),
		num_branches(n_branches),
		num_active_inst(n_active)
		{
//------- RMT ------------------------------------------------
	rmt=new uint64_t[num_log_regs];
//-------- AMT ------------------------------------------------
	amt=new uint64_t[num_log_regs];
	
//-------RMT AND AMT INITIALISATION----------------------------
for (uint64_t i=0;i<num_log_regs;i++)
{
//----------------setting the initial commited state registers as PHY registers 0 t0 31}
	rmt[i]=i;
	amt[i]=i;
}
//-----Free_List Initialisation------------------------
	//Free_list free_List;

	free_List.f_preg= new uint64_t[num_phys_regs-num_log_regs];
	for (uint64_t i=0;i<num_phys_regs-num_log_regs;i++)
	{
	//--Since we are initialising the commited states as prf 0 to 31-----
		free_List.f_preg[i]=num_log_regs+i;
	}
	free_List.f_head=0;
	free_List.f_tail=0;
	free_List.f_size=num_phys_regs-num_log_regs;
//------Active_List Initialisation----------------------
//	active_list active_List;
	active_List.act_list=new Active_list[num_active_inst];
	active_List.act_head=0;
	active_List.act_tail=0;
	active_List.act_size=0;
	for (uint64_t i=0;i<num_active_inst;i++)
	{
	active_List.act_list[i].dest_flag=false;
	active_List.act_list[i].logical_reg=0;
	active_List.act_list[i].phy_reg=0;
	active_List.act_list[i].comp_bit=false;
	active_List.act_list[i].exe_bit=false;
	active_List.act_list[i].load_vio=false;
	active_List.act_list[i].branch_mis=false;
	active_List.act_list[i].val_mis=false;
	active_List.act_list[i].load_flag=false;
	active_List.act_list[i].store_flag=false;
	active_List.act_list[i].branch_flag=false;
	active_List.act_list[i].amo_flag=false;
	active_List.act_list[i].csr_flag=false;
	active_List.act_list[i].pc=0;
	}
//---------- PRF -----------------------------------------------
	prf=new uint64_t[num_phys_regs];
	prf_rdy_bit= new bool[num_phys_regs];
//-----Initialising prf ready bits as 1
	for( uint64_t i=0;i<num_phys_regs;i++)
	{
		prf_rdy_bit[i]=true;
		prf_unmapped_bit=true;
		prf_usage_counter=0;
	}
//-----Initialising the unmapped bits of phys_reg
	for(uint64_t i=0;i<num_log_regs;i++)
	{
		prf_unmapped_bit[rmt[i]]=false;
	}
		
//================MOD_CPR==========================
//------CPR_Checkpoint initialisation----------------------
	chk_buffer.Chk_buffer=new Checkpoints[num_branches];
	//chk_buffer.Chk_valid=new bool[num_branches];
	chk_buffer.Chkbuf_head=0;
	chk_buffer.Chkbuf_tail=0;
	chk_buffer.Chkbuf_size=0;
	for(uint64_t i=0;i<num_branches;i++)
	{
		chk_buffer.Chk_buffer[i].chkpt_RMT=new uint64_t[num_log_regs];
		chk_buffer.Chk_buffer[i].chkpt_unmapped_bit= new bool[num_phys_regs];
	}
	
//-----Creating Oldest Checkpoint to start with -----------------
	checkpoint();
for (uint64_t i=0;i<num_log_regs;i++)
{	
	chk_buffer.Chk_buffer[0].chkpt_RMT=i;
}
//==================MOD_CPR================================


//---------------Branch Check Points------------------
//Branch_checkpoints *branch_checkpoints;
branch_checkpoints= new Branch_checkpoints[num_branches];
for (uint64_t i=0;i<num_branches;i++)
{
	branch_checkpoints[i].shadow_map_table=new uint64_t[num_log_regs];
	//branch_checkpoints[i].saved_flist_head=0;
	//branch_checkpoints[i].saved_GBM=0;
}
//-----------------GBM init----------------------------
GBM=0;
}
renamer::~renamer()
{
	delete[] amt;
	delete[] rmt;
	delete[] prf;
	delete[] prf_rdy_bit;
	delete[] branch_checkpoints;
	delete[] free_List.f_preg;
	delete[] active_List.act_list;
}
//----To stall when we dont have enough free physical registers
bool renamer::stall_reg( uint64_t bundle_dst)
{
	//printf("---------------------------Stall_reg-------------------\n");
	if(free_List.f_size<bundle_dst)
	{
	//	printf(" Stall_reg: No free space in free list\n");
		return true;
	}
	else 
	{
	//	printf("Stall_reg: free_list is available\n");
		return false;
	}
}
//----To stall when there are no checkpoints available for the branches
bool renamer::stall_branch( uint64_t bundle_branch)
{
	//printf("--------------------------stall_branch-------------------\n");
	uint64_t avail_checkpoints=0;
	uint64_t X=GBM;
	//printf("%x",GBM);
	for (uint64_t i=0;i<num_branches;i++)
	{
		if(X%2==0)
		{
			avail_checkpoints++;
		}
		X>>=1;
	}
	if (bundle_branch>avail_checkpoints)
	{
	//	printf(" stall_branch: CHECKPOINT branch NOT available\n");
		return true;
	}
	else
	{
		//printf("stall_branch: CHECKPOINT branch available\n");
		return false;
	}
			
}
//=========MOD_CPR============================================
//--To stall renaming when there are not enough checkpoints available
bool renamer::stall_checkpoint(uint64_t bundle_chkpts)
{
	//printf("----------------------------stall_checkpoint---------------------\n");
	if(bundle_chkpts>num_branches-chk_buffer.Chkbuf_size)
	{
		return true;
	}
	else
	{
		return false;
	}
}
//========MOD_CPR=======================================
//---- Getting Branch Mask -----
uint64_t renamer::get_branch_mask()
{
	//printf("-----------------------get_branch_mask----------------------\n");
	return GBM;
}
//---- Rename source Registers ----
uint64_t renamer::rename_rsrc(uint64_t log_reg)
{
	//printf("-------------------------rename_rsrc------------------------\n");
	return rmt[log_reg];
}
//---- Renaming Producers ----------
uint64_t renamer::rename_rdst(uint64_t log_reg)
{
	//printf("--------------------------rename_rdst------------------------\n");
	assert(free_List.f_size!=0);
	uint64_t phy_name=free_List.f_preg[free_List.f_head];
	free_List.f_size--;
	free_List.f_head++;
	if(free_List.f_head==num_phys_regs-num_log_regs)
	{
		free_List.f_head=0;
	}
	prf_rdy_bit[free_List.f_preg[free_List.f_head]]=0;
	rmt[log_reg]=phy_name;
	return phy_name;
}
//=======MOD_CPR================================
void renamer::checkpoint()
{
	//printf("----------------------creating checkpoint----------------------\n");
	/*uint64_t X=GBM;
	uint64_t branch_id;
	for( uint64_t i=0;i<num_branches;i++)
	{
		if(X%2==0)
		{
			branch_id=i;
			break;
		}
		X>>=1;
	}
	GBM=GBM|((uint64_t)1<<branch_id);	
	//-------Saving_ GBM------------------
	branch_checkpoints[branch_id].saved_GBM=GBM;
	//-------Save Free_List Head----------
	branch_checkpoints[branch_id].saved_flist_head=free_List.f_head;
	//-------Save RMT-------------------
	for (uint64_t i=0;i<num_log_regs;i++)
	{
		branch_checkpoints[branch_id].shadow_map_table[i]=rmt[i];
	}
	return branch_id;*/
	assert(chk_buffer.Chkbuf_size!=num_branches);
	for(uint64_t i=0;i<num_log_regs;i++)
	{
		//-----Checkpointing RMT--------
		chk_buffer.Chk_buffer[chk_buffer.Chkbuf_tail].chkpt_RMT[i]=rmt[i];
		//-----Incrementing corresponding usage counters of the phy regs
		prf_usage_counter[rmt[i]]++;
	}
	for(uint64_t i=0;i<num_phys_regs;i++)
	{
		//-----Checkpointing unmapped bit of phy regs-------
		chk_buffer.Chk_buffer[chk_buffer.Chkbuf_tail].chkpt_unmapped_bit[i]=prf_unmapped_bit[i];
	}
	chk_buffer.Chkbuf_tail++;
	if(chk_buffer.Chkbuf_tail==num_branches)
	{
		Chkbuf_tail=0;
	}
	chk_buffer.Chkbuf_size++;
	instr_renamed_since_last_checkpoint=0;
}

uint64_t renamer::get_checkpoint_ID(bool load, bool store, bool branch, bool amo, bool csr)
{
	chk_buffer.Chk_buffer[chk_buffer.Chkbuf_tail-1].uncomp_inst_cnt++;
	if(load)
	{
		chk_buffer.Chk_buffer[chk_buffer.Chkbuf_tail-1].load_cnt++;
	}
	else if(store)
	{
		chk_buffer.Chk_buffer[chk_buffer.Chkbuf_tail-1].store_cnt++;
	}
	else if(branch)
	{
		chk_buffer.Chk_buffer[chk_buffer.Chkbuf_tail-1].br_cnt++;
	}
	else if(amo)
	{
		chk_buffer.Chk_buffer[chk_buffer.Chkbuf_tail-1].amo=true;
	}
	else if(csr)
	{
		chk_buffer.Chk_buffer[chk_buffer.Chkbuf_tail-1].csr=true;
	}
	return chk_buffer.Chkbuf_tail-1;
}
//=============MOD_CPR==============================
bool renamer::stall_dispatch(uint64_t bundle_inst)
{
	//printf("----------------------------stall_dispatch---------------------\n");
	uint64_t avail_list=num_active_inst-active_List.act_size;
	if(bundle_inst>avail_list)
	{
	//	printf("Stalling_Dispatch");
		return true;
	}
	else
	{
	//	printf("not stalling_dispatch");
		return false;
	}
}

uint64_t renamer::dispatch_inst( bool dest_valid, uint64_t log_reg, uint64_t phys_reg, bool load, bool store, bool branch, bool amo, bool csr, uint64_t PC)
{
	//printf("----------------------------dispatch_inst---------------------\n");
	assert(active_List.act_size!=num_active_inst);
	active_List.act_list[active_List.act_tail].dest_flag=dest_valid;
	if(dest_valid==true)
	{
		active_List.act_list[active_List.act_tail].logical_reg=log_reg;
		active_List.act_list[active_List.act_tail].phy_reg=phys_reg;
	}
	active_List.act_list[active_List.act_tail].load_flag=load;
	active_List.act_list[active_List.act_tail].store_flag=store;
	active_List.act_list[active_List.act_tail].branch_flag=branch;
	active_List.act_list[active_List.act_tail].amo_flag=amo;
	active_List.act_list[active_List.act_tail].csr_flag=csr;
	active_List.act_list[active_List.act_tail].pc=PC;
	active_List.act_list[active_List.act_tail].comp_bit=false;
	active_List.act_list[active_List.act_tail].exe_bit=false;
	active_List.act_list[active_List.act_tail].load_vio=false;
	active_List.act_list[active_List.act_tail].branch_mis=false;
	active_List.act_list[active_List.act_tail].val_mis=false;
	active_List.act_size++;
	uint64_t actinst_index=active_List.act_tail;
	active_List.act_tail++;
	if(active_List.act_tail==num_active_inst)
	{
		active_List.act_tail=0;
	}
	return actinst_index;
}

bool renamer:: is_ready(uint64_t phys_reg)
{
	//printf("--------------------------------is_ready-----------------------\n");
	//printf("readiness: %d  phy reg: %d \n",prf_rdy_bit[phys_reg],phys_reg);
	return prf_rdy_bit[phys_reg];
}

void renamer::clear_ready(uint64_t phys_reg)
{
	//printf("-----------------------------clear_ready----------------------\n");
	prf_rdy_bit[phys_reg]=false;
}

void renamer::set_ready(uint64_t phys_reg)
{
	//printf("------------------------------set_ready-----------------------\n");
	prf_rdy_bit[phys_reg]=true;
}

uint64_t renamer::read(uint64_t phys_reg)
{
	//printf("-------------------------------read--------------------------\n");
	return prf[phys_reg];
}

void renamer::write(uint64_t phys_reg, uint64_t value)
{
	//printf("------------------------------write--------------------------\n");
	prf[phys_reg]=value;
}

void renamer::set_complete(uint64_t chkpt_ID)
{
	//printf("---------------------------set_complete-----------------------\n");
	chk_buffer.Chk_buffer[chkpt_ID].uncomp_inst_cnt--;
}

void renamer::resolve( uint64_t AL_index, uint64_t branch_ID, bool correct)
{
	//printf("-----------------------------resolve-------------------------\n");
	uint64_t rsto_GBM,clear_mask;
	if(correct==true)
	{
		GBM = GBM&(~((uint64_t)1<<branch_ID));
		for (uint64_t i=0;i<num_branches;i++)
		{
			branch_checkpoints[i].saved_GBM=branch_checkpoints[i].saved_GBM&(~((uint64_t)1<<branch_ID));
		}
	}
	else
	{
		//------------Restoring the GBM-----------------
		rsto_GBM=branch_checkpoints[branch_ID].saved_GBM&(~((uint64_t)1<<branch_ID));
		//------------Clearing the Branch_bit in check pointed GBM of other checkpoints
		clear_mask=(uint64_t)1<<branch_ID;
		
		GBM=rsto_GBM;
		//------------Restoring the checkpoints---------
		for( uint64_t i=0;i<num_log_regs;i++)
		{
			rmt[i]=branch_checkpoints[branch_ID].shadow_map_table[i];
		}
		AL_index++;
		//see if the tail and head are wrapped
		if(AL_index==num_active_inst)
		{
			AL_index=0;
		}
		//------Propogating back in the active list to free all the registers untill the offending branch is reached
		while(active_List.act_tail!=AL_index)
		{
			//-------if Active_List is full---------
			if(active_List.act_tail==0)
			{
				active_List.act_tail=num_active_inst-1;
			}
			else
			{
				active_List.act_tail--;
			}
			active_List.act_size--;
			//assert(active_List.act_size==0);
		}
		//As we propogate back the active list, we have to add the registers back to the free_list.
		while(free_List.f_head!=branch_checkpoints[branch_ID].saved_flist_head)
		{
			if(free_List.f_head==0)
			{
				free_List.f_head=num_phys_regs-num_log_regs-1;
			}
			else
			{
				free_List.f_head--;
			}
		free_List.f_size++;
		}
		/*uint64_t start=free_List.f_size;
		uint64_t end =free_List.f_head;
		while(start>0)
		{
			prf_rdy_bit[free_List.f_preg[end]]=1;
			end++;
			if(end==num_phys_regs-num_log_regs)
			{
				end=0;
			}
			start--;
		}*/
			
	}
}
//==========================MOD_CPR===================================================
bool renamer::precommit(uint64_t &chkpt_id, uint64_t &num_loads, uint64_t &num_stores, uint64_t &num_branches, bool &amo, bool &csr, bool &exception)
			   {	
			   //printf("---------------------precommit---------------------------------\n");
			//	printf("active_List.act_head: %d\n", active_List.act_head);
			//printf("active_List.act_tail: %d\n", active_List.act_tail);
			//printf("freelist.f_head: %d\n", free_List.f_head);
			//printf("freelist.f_tail: %d\n", free_List.f_tail);
			//printf("freelist.f_size: %d\n", free_List.f_size);
			//printf("active_List.act_size: %d\n", active_List.act_size);
			//printf("GBM: %ld\n", GBM);
			//printf("Comp bit: %d\n", active_List.act_list[active_List.act_head].comp_bit);
			//printf("Phy reg: %d\n", active_List.act_list[active_List.act_head].phy_reg);
			//printf("Exe bit: %d\n", active_List.act_list[active_List.act_head].exe_bit);
			//printf("Load vio: %d\n", active_List.act_list[active_List.act_head].load_vio);
			//printf("Branch mis: %d\n", active_List.act_list[active_List.act_head].branch_mis);
			//printf("Value mis: %d\n", active_List.act_list[active_List.act_head].val_mis);
			/*if(active_List.act_size!=0)
			{
				completed=active_List.act_list[active_List.act_head].comp_bit;
				exception=active_List.act_list[active_List.act_head].exe_bit;
				load_viol=active_List.act_list[active_List.act_head].load_vio;
				br_misp=active_List.act_list[active_List.act_head].branch_mis;
				val_misp=active_List.act_list[active_List.act_head].val_mis;
				load=active_List.act_list[active_List.act_head].load_flag;
				store=active_List.act_list[active_List.act_head].store_flag;
				branch=active_List.act_list[active_List.act_head].branch_flag;
				amo=active_List.act_list[active_List.act_head].amo_flag;
				csr=active_List.act_list[active_List.act_head].csr_flag;
				PC=active_List.act_list[active_List.act_head].pc;
				return true;
			}
			else
			{
				return false;
			}*/
			if(chk_buffer.Chkbuf_size>1)
			{
				chkpt_id=chk_buffer.Chkbuf_head;
				num_loads=chk_buffer.Chk_buffer[chk_buffer.Chkbuf_head].load_cnt;
				num_stores=chk_buffer.Chk_buffer[chk_buffer.Chkbuf_head].store_cnt;
				num_branches=chk_buffer.Chk_buffer[chk_buffer.Chkbuf_head].br_cnt;
				amo=chk_buffer.Chk_buffer[chk_buffer.Chkbuf_head].amo;
				csr=chk_buffer.Chk_buffer[chk_buffer.Chkbuf_head].csr;
				exception=chk_buffer.Chk_buffer[chk_buffer.Chkbuf_head].exe;
				if(chk_buffer.Chk_buffer[chk_buffer.Chkbuf_head].uncomp_inst==0)
				{
					return true;
				}
				else
				{
					return false;
				}
			   }
			else
			{
				return false
			}
		}
	//===============================MOD_CPR=============================================
			   

void renamer::commit()
{
	//printf("--------------------------commit----------------------------------------\n");
	assert(active_List.act_size!=0);
	assert(active_List.act_list[active_List.act_head].comp_bit==1);
	assert(active_List.act_list[active_List.act_head].exe_bit!=1);
	assert(active_List.act_list[active_List.act_head].load_vio!=1);
	assert(active_List.act_list[active_List.act_head].branch_mis!=1);
	//assert(active_List.act_List[active_List.act_head].val_mis!=1);
	if(active_List.act_list[active_List.act_head].dest_flag==true)
	{
		free_List.f_size++;
		free_List.f_preg[free_List.f_tail]=amt[active_List.act_list[active_List.act_head].logical_reg];
		free_List.f_tail++;
		if(free_List.f_tail==num_phys_regs-num_log_regs)
		{
			free_List.f_tail=0;
		}
		amt[active_List.act_list[active_List.act_head].logical_reg]=active_List.act_list[active_List.act_head].phy_reg;
	}
		active_List.act_head++;
		active_List.act_size--;
		if(active_List.act_head==num_active_inst)
		{
			active_List.act_head=0;
		}
}
void renamer::squash()
{
	//printf("--------------------------squash-----------------------------\n");
	for(uint64_t i=0;i<num_log_regs;i++)
	{
		rmt[i]=amt[i];
	}
	free_List.f_head=free_List.f_tail;
	free_List.f_size=num_phys_regs-num_log_regs;
	active_List.act_head=active_List.act_tail;
	active_List.act_size=0;
	for(uint64_t i=0;i<num_phys_regs;i++)
	{
		prf_rdy_bit[i]=1;
	}
	GBM=0;

}
void renamer::set_exception(uint64_t chkpt_ID)
{
	//printf("--------------------------set_exception------------------------\n");
	chk_buffer.Chk_buffer[chkpt_ID].exe=true;
}

void renamer::set_load_violation(uint64_t AL_index)
{
	//printf("----------------------set_load_violation------------------------\n");
	active_List.act_list[AL_index].load_vio=true;
}
void renamer::set_branch_misprediction(uint64_t AL_index)
{
	//printf("------------------set_branch_misprediction-----------------------\n");
	active_List.act_list[AL_index].branch_mis=true;
}
void renamer::set_value_misprediction(uint64_t AL_index)
{
	//printf("----------------------set_value_misprediction---------------------\n");
	active_List.act_list[AL_index].val_mis=true;
}
bool renamer::get_exception(uint64_t AL_index)
{
	//printf("------------------------get_exception-----------------------------\n");
	 return active_List.act_list[AL_index].exe_bit;
}



