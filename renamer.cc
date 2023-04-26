#include<inttypes.h>
#include<stdio.h>
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
	prf_unmapped_bit=new bool[num_phys_regs];
	prf_usage_counter=new uint64_t[num_phys_regs];
//-----Initialising prf ready bits as 1
	for( uint64_t i=0;i<num_phys_regs;i++)
	{
		prf_rdy_bit[i]=true;
		prf_unmapped_bit[i]=true;
		prf_usage_counter[i]=0;
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
	chk_buffer.Chk_buffer[0].chkpt_RMT[i]=i;
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
		//printf(" Stall_reg: No free space in free list\n");
		return true;
	}
	else 
	{
		//printf("Stall_reg: free_list is available\n");
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
		//printf(" stall_branch: CHECKPOINT branch NOT available\n");
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
	printf("Stall_checkpoint %d::%d\n",bundle_chkpts,chk_buffer.Chkbuf_size);
	if(bundle_chkpts>num_branches-chk_buffer.Chkbuf_size)
	{
		printf("STALLING\n");
		return true;
	}
	else
	{
		printf("NOT STALLING\n");
		return false;
		
	}
}
void renamer::inc_usage_counter(uint64_t phys_reg)
{
	prf_usage_counter[phys_reg]++;
}
void renamer::dec_usage_counter(uint64_t phys_reg)
{
	assert(prf_usage_counter[phys_reg]>0);
	prf_usage_counter[phys_reg]--;
	//==============Freeing the phys register mapping and adding to free list
	if((prf_unmapped_bit[phys_reg]==true)&&(prf_usage_counter[phys_reg]==0))
	{
		printf("RR\n");
		free_List.f_size++;
		free_List.f_preg[free_List.f_tail]=phys_reg;
		free_List.f_tail++;
		if(free_List.f_tail==num_phys_regs-num_log_regs)
		{
			free_List.f_tail=0;
		}
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
	//prf_usage_counter[rmt[log_reg]]++;
	inc_usage_counter(rmt[log_reg]);
	return rmt[log_reg];
}
void renamer::map(uint64_t phys_reg)
{
prf_unmapped_bit[phys_reg]=false;
}

// Set the unmapped bit of physical register “phys_reg”.
// Check if phys_reg’s usage counter is 0; if so,
// push phys_reg onto the Free List.

void renamer::unmap(uint64_t phys_reg)
{
 prf_unmapped_bit[phys_reg]=true;
 if(prf_usage_counter[phys_reg]==0)
	{
		free_List.f_size++;
		free_List.f_preg[free_List.f_tail]=phys_reg;
		free_List.f_tail++;
		if(free_List.f_tail==num_phys_regs-num_log_regs)
		{
			free_List.f_tail=0;
		}
	}
}
//---- Renaming Producers ----------
uint64_t renamer::rename_rdst(uint64_t log_reg)
{
	//printf("--------------------------rename_rdst------------------------\n");
	//prf_usage_counter[rmt[log_reg]]++;
	//prf_unmapped_bit[rmt[log_reg]]=true;
	unmap(rmt[log_reg]);
	assert(free_List.f_size!=0);
	uint64_t phy_name=free_List.f_preg[free_List.f_head];
	free_List.f_size--;
	free_List.f_head++;
	if(free_List.f_head==num_phys_regs-num_log_regs)
	{
		free_List.f_head=0;
	}
	prf_rdy_bit[phy_name]=0;
    rmt[log_reg]=phy_name;
	map(rmt[log_reg]);
	inc_usage_counter(rmt[log_reg]);
	return phy_name;
}
//=======MOD_CPR================================
//===MOD_CPR_AV19===
//Clear the unmapped bit of physical register “phys_reg”
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
	printf("%d\n",num_branches);
	for(uint64_t i=0;i<num_log_regs;i++)
	{
		//-----Checkpointing RMT--------
		chk_buffer.Chk_buffer[chk_buffer.Chkbuf_tail].chkpt_RMT[i]=rmt[i];
		//-----Incrementing corresponding usage counters of the phy regs
		inc_usage_counter(rmt[i]);
	}
	for(uint64_t i=0;i<num_phys_regs;i++)
	{
		//-----Checkpointing unmapped bit of phy regs-------
		chk_buffer.Chk_buffer[chk_buffer.Chkbuf_tail].chkpt_unmapped_bit[i]=prf_unmapped_bit[i];
	}
	chk_buffer.Chk_buffer[chk_buffer.Chkbuf_tail].uncomp_inst_cnt=0;
	chk_buffer.Chk_buffer[chk_buffer.Chkbuf_tail].load_cnt=0;
	chk_buffer.Chk_buffer[chk_buffer.Chkbuf_tail].store_cnt=0;
	chk_buffer.Chk_buffer[chk_buffer.Chkbuf_tail].br_cnt=0;
	chk_buffer.Chk_buffer[chk_buffer.Chkbuf_tail].amo=false;
	chk_buffer.Chk_buffer[chk_buffer.Chkbuf_tail].csr=false;
	chk_buffer.Chk_buffer[chk_buffer.Chkbuf_tail].exe=false;
	chk_buffer.Chkbuf_tail++;
	if(chk_buffer.Chkbuf_tail==num_branches)
	{
		chk_buffer.Chkbuf_tail=0;
	}
	chk_buffer.Chkbuf_size++;
	//instr_renamed_since_last_checkpoint=0;
}

uint64_t renamer::get_checkpoint_ID(bool load, bool store, bool branch, bool amo, bool csr)
{
	//printf("getting checkpoint_ID :%d\n",chk_buffer.Chk_buffer[chk_buffer.Chkbuf_tail-1].br_cnt);
	chk_buffer.Chk_buffer[((chk_buffer.Chkbuf_tail==0)?num_branches:chk_buffer.Chkbuf_tail)-1].uncomp_inst_cnt++;
	if(load)
	{
		chk_buffer.Chk_buffer[((chk_buffer.Chkbuf_tail==0)?num_branches:chk_buffer.Chkbuf_tail)-1].load_cnt++;
	}
	else if(store)
	{
		chk_buffer.Chk_buffer[((chk_buffer.Chkbuf_tail==0)?num_branches:chk_buffer.Chkbuf_tail)-1].store_cnt++;
	}
	else if(branch)
	{
		chk_buffer.Chk_buffer[((chk_buffer.Chkbuf_tail==0)?num_branches:chk_buffer.Chkbuf_tail)-1].br_cnt++;
	}
	else if(amo)
	{
		chk_buffer.Chk_buffer[((chk_buffer.Chkbuf_tail==0)?num_branches:chk_buffer.Chkbuf_tail)-1].amo=true;
	}
	else if(csr)
	{
		chk_buffer.Chk_buffer[((chk_buffer.Chkbuf_tail==0)?num_branches:chk_buffer.Chkbuf_tail)-1].csr=true;
	}
	if(chk_buffer.Chkbuf_tail==0){
		//printf("getting checkpoint_ID :%d\n",num_branches-1);
		return num_branches-1;
	}
	//printf("getting checkpoint_ID :%d\n",chk_buffer.Chkbuf_tail-1);
	return (chk_buffer.Chkbuf_tail-1);
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
	dec_usage_counter(phys_reg);
	return prf[phys_reg];
}

void renamer::write(uint64_t phys_reg, uint64_t value)
{
	//printf("------------------------------write--------------------------\n");
	dec_usage_counter(phys_reg);
	prf[phys_reg]=value;
}

void renamer::set_complete(uint64_t chkpt_ID)
{
	//printf("---------------------------set_complete----------------------- :%d\n",chkpt_ID);
	chk_buffer.Chk_buffer[chkpt_ID].uncomp_inst_cnt--;
}
//========CPR_MOD==========================
uint64_t renamer::rollback(uint64_t chkpt_id, bool next, uint64_t &total_loads, uint64_t &total_stores, uint64_t &total_branches)	
{
	printf("roll_back");
	uint64_t squash_mask=0;
	total_loads=0;
	total_stores=0;
	total_branches=0;
	//===============Determine the rollback checkpoint from chkpt_id and next.//=========================
	if(next==true)
	{
		chkpt_id = (chkpt_id+1)%chk_buffer.Chkbuf_size;
	}
	//=======Assert the rollback checkpoint is valid/allocated, i.e., it exists between the checkpoint buffer head and tail.//=====================
	bool valid=chkpt_is_valid(chkpt_id);
	assert(valid);
	//========Restore the RMT and unmapped bits from the rollback checkpoint===========
	if (valid)
	{
		//====Restore the RMT and unmapped bits from the rollback checkpoint.=====================
		for(uint64_t i=0;i<num_log_regs;i++)
		{
			rmt[i]=chk_buffer.Chk_buffer[chkpt_id].chkpt_RMT[i];
		}
		//====Generating squash mask========
		for(uint64_t i=0;i<num_branches;i++)
		{
			squash_mask=squash_mask|(is_squash_chkpt(chkpt_id,i)<<i);
		}
		//====The rollback checkpoint should be preserved (not squashed/freed). On the other hand, all allocated younger checkpoints should be squashed/freed. Decrement the usage counter of each physical register mapped in each squashed/freed checkpoint====
		for(uint64_t i=0;i<num_branches;i++)
		{
			if((i!=chkpt_id)&&((squash_mask>>i)&(0x1)))
			{
				//======Checkpoint to be squashed======
				//======Decrementing the usage counter
				for(uint64_t j=0;j<num_phys_regs;j++)
				{
					dec_usage_counter(chk_buffer.Chk_buffer[i].chkpt_RMT[j]);
				}
				chk_buffer.Chkbuf_size--;
				//=======condition for wrapping===========
				chk_buffer.Chkbuf_tail--;
				if(chk_buffer.Chkbuf_tail==-1){
					chk_buffer.Chkbuf_tail=num_branches-1;
				}
				//=======UNCOMP _INST TOZERO===SAFETY
			}
			else if(i==chkpt_id)
			{
				//===resetting the cnts and flags of the rollback chkpt_ID
				chk_buffer.Chk_buffer[i].uncomp_inst_cnt=0;
				chk_buffer.Chk_buffer[i].load_cnt=0;
				chk_buffer.Chk_buffer[i].store_cnt=0;
				chk_buffer.Chk_buffer[i].br_cnt=0;
				chk_buffer.Chk_buffer[i].csr=false;
				chk_buffer.Chk_buffer[i].amo=false;
				chk_buffer.Chk_buffer[i].exe=false;
			}
		}
		if(chk_buffer.Chkbuf_head<chkpt_id)
		{
			for(uint64_t i=chk_buffer.Chkbuf_head;i<chkpt_id;i++)
			{
				total_loads+=chk_buffer.Chk_buffer[i].load_cnt++;
				total_stores+=chk_buffer.Chk_buffer[i].store_cnt++;
				total_branches+=chk_buffer.Chk_buffer[i].br_cnt++;
			}
						
		}	
		else
		{
			for(uint64_t i=chk_buffer.Chkbuf_head;i<num_branches;i++)
			{
				total_loads+=chk_buffer.Chk_buffer[i].load_cnt++;
				total_stores+=chk_buffer.Chk_buffer[i].store_cnt++;
				total_branches+=chk_buffer.Chk_buffer[i].br_cnt++;
			}
			for(uint64_t i=0;i<chkpt_id;i++)
			{
				total_loads+=chk_buffer.Chk_buffer[i].load_cnt++;
				total_stores+=chk_buffer.Chk_buffer[i].store_cnt++;
				total_branches+=chk_buffer.Chk_buffer[i].br_cnt++;
			}
		}
	}
	for(uint64_t i=0;i<num_phys_regs;i++)
		{
			prf_unmapped_bit[i]=chk_buffer.Chk_buffer[chkpt_id].chkpt_unmapped_bit[i];
			if(prf_unmapped_bit[i]==1){
				unmap(prf_unmapped_bit[i]);
			}
			//=================UNMPAP BITS RECLAMATION===========
		}
	Squash_mask=squash_mask;
	return squash_mask;	
}
bool renamer::chkpt_is_valid(uint64_t chkpt_id)
{
	if(chk_buffer.Chkbuf_head<chk_buffer.Chkbuf_tail)
		{
			for(uint64_t i=chk_buffer.Chkbuf_head;i<chk_buffer.Chkbuf_tail;i++)
			{
				if(i==chkpt_id)
					return true;
			}			
		}
	else
		{
			for(uint64_t i=chk_buffer.Chkbuf_head;i<num_branches;i++)
			{
				if(i==chkpt_id)
					return true;
			}
			for(uint64_t i=0;i<chk_buffer.Chkbuf_tail;i++)
			{
				if(i==chkpt_id)
					return true;
			}
		}
	return false;
}
bool renamer::is_squash_chkpt(uint64_t chkpt_id,uint64_t i)
{
	if(chkpt_is_valid(i))
	{
		if(chk_buffer.Chkbuf_head<chk_buffer.Chkbuf_tail)
		{
			if(i>=chkpt_id)
			{
				return true;
			}
		}
		else
		{
			if(chkpt_id>chk_buffer.Chkbuf_tail)
			{
				if(((i<num_branches)&&(i>=chkpt_id))||(i<chk_buffer.Chkbuf_tail))
				{
					return true;
				}
			}
			else if(chkpt_id<=chk_buffer.Chkbuf_tail)
			{
				if(!((i<num_branches)&&(i>=chk_buffer.Chkbuf_head))&&(i>=chkpt_id))
				{
					return true;
				}
			}
		}
	}
	return false;
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
//=================MOD_CPR=================
void renamer::free_checkpoint()
{
	//printf("======poping the head of the checkpoint buffer (freeing the oldest checkpoint================");
	//chk_buffer.Chk_buffer[chk_buffer.Chkbuf_head].load_cnt=0;
	
	chk_buffer.Chkbuf_head++;
	if(chk_buffer.Chkbuf_head==num_branches)
	{
		chk_buffer.Chkbuf_head=0;
	}
	chk_buffer.Chkbuf_size--;
}
//===================MOD_CPR==================
//==========================MOD_CPR===================================================
bool renamer::precommit(uint64_t &chkpt_id, uint64_t &num_loads, uint64_t &num_stores, uint64_t &num_branch, bool &amo, bool &csr, bool &exception)
			   {	
				//printf("---------------------precommit---------------------------------\n");
				static uint64_t cyc_cnt = 0 ;
				cyc_cnt = cyc_cnt +1;
				printf("size:%d\n",chk_buffer.Chkbuf_size);
				//printf("cyc_cnt is %ld\n", cyc_cnt);
				//printf("num_branches::%d\n",num_branches);
				//printf("%d -> %d\n",chk_buffer.Chkbuf_head,chk_buffer.Chkbuf_tail);
				/*if(chk_buffer.Chkbuf_head<chk_buffer.Chkbuf_tail){
					for(uint64_t i=chk_buffer.Chkbuf_head;i<chk_buffer.Chkbuf_tail;i++)
						printf("|%d::%d| ",chk_buffer.Chk_buffer[i].uncomp_inst_cnt,i);
					printf("\n");
				}
				else
				{
					for(uint64_t i=chk_buffer.Chkbuf_head;i<num_branches;i++)
						printf("|%d::%d| ",chk_buffer.Chk_buffer[i].uncomp_inst_cnt,i);
					printf("-|WR|-");
					for(uint64_t i=0;i<chk_buffer.Chkbuf_tail;i++)
						printf("|%d::%d| ",chk_buffer.Chk_buffer[i].uncomp_inst_cnt,i);
					printf("\n");
				};*/
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
			if((chk_buffer.Chkbuf_size>1)&&(chk_buffer.Chk_buffer[chk_buffer.Chkbuf_head].uncomp_inst_cnt==0))
			{
				chkpt_id=chk_buffer.Chkbuf_head;
				num_loads=chk_buffer.Chk_buffer[chk_buffer.Chkbuf_head].load_cnt;
				num_stores=chk_buffer.Chk_buffer[chk_buffer.Chkbuf_head].store_cnt;
				num_branch=chk_buffer.Chk_buffer[chk_buffer.Chkbuf_head].br_cnt;
				amo=chk_buffer.Chk_buffer[chk_buffer.Chkbuf_head].amo;
				csr=chk_buffer.Chk_buffer[chk_buffer.Chkbuf_head].csr;
				exception=chk_buffer.Chk_buffer[chk_buffer.Chkbuf_head].exe;
				/*printf("Precommit::\n chkpt_id:%d\n num_loads :%d\n num_stores:%d\n num_branches:%d\n amo :%d\n csr:%d\n exception	:%d\n uncomp_inst_cnt :%d\n Chk_size: %d\n",chkpt_id,num_loads,num_stores,num_branches,amo,csr,exception,chk_buffer.Chk_buffer[chk_buffer.Chkbuf_head].uncomp_inst_cnt,chk_buffer.Chkbuf_size);*/
				return true;
			}
			else
			{
			return false;
			}
		}
	//===============================MOD_CPR=============================================
			   
//============================MOD_CPR======================
void renamer::commit(uint64_t log_reg)
{
	//printf("--------------------------commit----------------------------------------\n");
	/*assert(active_List.act_size!=0);
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
		}*/
	//printf("commiting %d\n",log_reg);
	dec_usage_counter(chk_buffer.Chk_buffer[chk_buffer.Chkbuf_head].chkpt_RMT[log_reg]);
}
//=======================MOD_CPR=============================
//=======================MOD_CPR=============================
void renamer::squash()
{
	//printf("--------------------------squash-----------------------------\n");
	/*for(uint64_t i=0;i<num_log_regs;i++)
	{
		rmt[i]=amt[i];
	}*/
	//============Instead of copying from AMT, copy from the oldest checkpoint
	uint64_t oldest_chkpt_ID=chk_buffer.Chkbuf_head;
	for(uint64_t i=0;i<num_log_regs;i++)
	{
		rmt[i]=chk_buffer.Chk_buffer[oldest_chkpt_ID].chkpt_RMT[i];
	}
	for(uint64_t i=0;i<num_phys_regs;i++)
	{
		prf_unmapped_bit[i]=chk_buffer.Chk_buffer[oldest_chkpt_ID].chkpt_unmapped_bit[i];
		/*if(prf_unmapped_bit[i]==1){
				unmap(prf_unmapped_bit[i]);
			}*/
		//============unmap reclaimm=================
	}
	//===========Traversing the chk_buffer in reverse order and performing reclamation of younger  checkpoints=========
	for(uint64_t chk_iter=chk_buffer.Chkbuf_size-1;chk_iter>oldest_chkpt_ID;chk_iter--)
	{
		for(uint64_t i=0;i<num_log_regs;i++)
		{
			dec_usage_counter(chk_buffer.Chk_buffer[chk_iter].chkpt_RMT[i]);
		}
	}
	for(uint64_t i=0;i<num_phys_regs;i++)
	{
		prf_usage_counter[i]=0;
		prf_unmapped_bit[i]=1;
	}
	for(uint64_t i=0;i<num_log_regs;i++)
	{
		prf_usage_counter[rmt[i]]=1;
		prf_unmapped_bit[rmt[i]]=0;
	}
	
	//================Initialising all counters to 0 and deasserting flags
	chk_buffer.Chk_buffer[oldest_chkpt_ID].uncomp_inst_cnt=0;
	chk_buffer.Chk_buffer[oldest_chkpt_ID].load_cnt=0;
	chk_buffer.Chk_buffer[oldest_chkpt_ID].store_cnt=0;
	chk_buffer.Chk_buffer[oldest_chkpt_ID].br_cnt=0;
	chk_buffer.Chk_buffer[oldest_chkpt_ID].amo=false;
	chk_buffer.Chk_buffer[oldest_chkpt_ID].csr=false;
	chk_buffer.Chk_buffer[oldest_chkpt_ID].exe=false;
	//==================Resetting the checkpoint buffer variables======
	chk_buffer.Chkbuf_tail=oldest_chkpt_ID+1;
	
	if(chk_buffer.Chkbuf_tail==num_branches)
	{
		chk_buffer.Chkbuf_tail=0;
	}
	chk_buffer.Chkbuf_size=1;
	//Initialising all the prf ready bits to 1=================
	for(uint64_t i=0;i<num_phys_regs;i++)
	{
		prf_rdy_bit[i]=1;
	}
	free_List.f_head=0;
	free_List.f_tail=0;
	free_List.f_size=num_phys_regs-num_log_regs;

}
//========================MOD_CPR====================================
//========================MOD_CPR==============================
void renamer::set_exception(uint64_t chkpt_ID)
{
	//printf("--------------------------set_exception------------------------\n");
	chk_buffer.Chk_buffer[chkpt_ID].exe=true;
}

void renamer::set_load_violation(uint64_t AL_index)
{
	//printf("----------------------set_load_violation------------------------\n");
	//active_List.act_list[AL_index].load_vio=true;
}
void renamer::set_branch_misprediction(uint64_t AL_index)
{
	//printf("------------------set_branch_misprediction-----------------------\n");
	//active_List.act_list[AL_index].branch_mis=true;
}
void renamer::set_value_misprediction(uint64_t AL_index)
{
	//printf("----------------------set_value_misprediction---------------------\n");
	//active_List.act_list[AL_index].val_mis=true;
}
bool renamer::get_exception(uint64_t chkpt_ID)
{
	//printf("------------------------get_exception-----------------------------\n");
	 return chk_buffer.Chk_buffer[chkpt_ID].exe;
}



