#include "ooo_cpu.h"
#include "btb.h"
#include "set.h"

// initialize the predictor
void BRANCH_TARGET_PRED::initialize()
{
    // initialize branch target predictor
    double alpha = pow((double)MAX_HIST / (double)MIN_HIST, 1.0 / (ITTAGE_NTAB-1));
    for(int i=0; i<ITTAGE_NTAB; i++)
    {
        ittage_hlen[i] = (int)((MIN_HIST * pow(alpha, i)) + 0.5);
    }
}

// make target prediction
uint8_t BRANCH_TARGET_PRED::predict_target(uint64_t ip, uint8_t type, uint64_t &target)
{
    uint64_t btb_idx = (ip >> BTB_OFFSET) & (BTB_NSET-1) & BTB_SET_MASK;
    uint64_t btb_tag = ip;
    uint64_t btb_way = 0;
    bool indirect = false;

    ittage_hit_bank = -1;
    ittage_alt_diff =  0;

    bool ubtb_hit = false;
    uint64_t ubtb_idx = (ip >> 2) & (UBTB_NSET-1);
    for(int ubtb_way=0; ubtb_way<UBTB_NWAY; ubtb_way++)
    {
        BTB_entry& entry = ubtb_set[ubtb_idx][ubtb_way];
        if(entry.tag == btb_tag)
        {
            ubtb_hit = true;
        }
    }

    for(btb_way=0; btb_way<BTB_NWAY; btb_way++)
    {
        BTB_entry& entry = btb_set[btb_idx][btb_way];
        if(entry.tag == btb_tag)
        {
            // default target setting
            target = entry.target;
            if((ras_size > 0) && (type == BRANCH_RETURN)) target = ras[ras_top];
            if(entry.indirect)
            {
                // indirect predictor lookup
                target = ittage_predict(ip, target);
                return 2; // indirect target
            }
            return ubtb_hit ? 1 : 2;
        }
    }

    // BTB miss
    return 0;
}

// update branch predictor
void BRANCH_TARGET_PRED::update_target(uint64_t ip, uint8_t type, uint64_t target)
{
    uint64_t btb_idx = (ip >> BTB_OFFSET) & (BTB_NSET-1) & BTB_SET_MASK;
    uint64_t btb_tag = ip;
    uint64_t btb_way = 0;
    uint64_t vic_way = 0;

    // micro predictor update
    if((type != BRANCH_INDIRECT) && (type != BRANCH_INDIRECT_CALL) && (target != 0))
    {
        bool ubtb_hit = false;
        uint64_t ubtb_idx = (ip >> 2) & (UBTB_NSET-1);
        uint64_t ubtb_vic = 0;
        for(int ubtb_way=0; ubtb_way<UBTB_NWAY; ubtb_way++)
        {
            BTB_entry& entry = ubtb_set[ubtb_idx][ubtb_way];
            if(entry.tag == btb_tag)
            {
                ubtb_hit = true;
                entry.rrpv = 3;
                if(entry.rrpv < ubtb_set[ubtb_idx][ubtb_vic].rrpv) ubtb_vic = ubtb_way;
                break;
            }
        }
        if(!ubtb_hit)
        {
            BTB_entry& entry = ubtb_set[ubtb_idx][ubtb_vic];
            for(int ubtb_way=0; ubtb_way<UBTB_NWAY; ubtb_way++)
            {
                if(ubtb_way != ubtb_vic)
                {
                    ubtb_set[ubtb_idx][ubtb_way].rrpv -= entry.rrpv;
                }
            }
            entry.tag = btb_tag;
            entry.rrpv = 1;
        }
    }

    // search hitting entry and victim selection
    bool btb_hit = false;
    for(btb_way=0; btb_way<BTB_NWAY; btb_way++)
    {
        BTB_entry& entry = btb_set[btb_idx][btb_way];
        if(entry.tag == btb_tag)
        {
            btb_hit = true;
            break;
        }
        if(entry.rrpv > 3)
        {
            entry.rrpv = 3;
        }
        if(entry.rrpv < btb_set[btb_idx][vic_way].rrpv)
        {
            vic_way = btb_way;
        }
    }

    if(!btb_hit)
    {
        // housekeeping
        BTB_entry& vic_entry = btb_set[btb_idx][vic_way];
        for(btb_way=0; btb_way<4; btb_way++)
        {
            BTB_entry& entry = btb_set[btb_idx][btb_way];
            if(vic_way != btb_way) entry.rrpv -= vic_entry.rrpv;
        }

        // allocate
        vic_entry.tag = btb_tag;
        vic_entry.target = target;
        vic_entry.indirect = 0;
        vic_entry.confidence = 0;
        vic_entry.rrpv = 1;
    }
    else // BTB hit
    {
        BTB_entry& hit_entry = btb_set[btb_idx][btb_way];
        hit_entry.rrpv = 3;
        if( (type != BRANCH_INDIRECT) && (type != BRANCH_INDIRECT_CALL) )
        {
            // for regular branch case
            if(target != 0) hit_entry.target = target;
            hit_entry.indirect = 0;
            hit_entry.confidence = 0;
        }
        else
        {
            assert(target != 0);
            // for indirect branch case
            if(hit_entry.target != target)
            {
                hit_entry.indirect = true;
            }
            if(hit_entry.indirect)
            {
                // indirect predictor update
                uint64_t predicted_target = 0;
                predict_target(ip, type, predicted_target);
                bool mispred = predicted_target != target;
                ittage_update(hit_entry, target, mispred);
            }
        }
    }
}

// indirect predictor lookup
uint64_t BRANCH_TARGET_PRED::ittage_predict(uint64_t ip, uint64_t target)
{
    // indirect predictor access
    const int idx_width = ITTAGE_IDX_NBIT;
    const int tag_width = ITTAGE_TAG_NBIT;

    // folding history
    for(int i=0; i<ITTAGE_NTAB; i++)
    {
        uint64_t ghr = 0;
        ittage_hash_idx[i] = (ip >> 2) ^ (ip >> (3+i));
        ittage_hash_tag[i] = (ip >> 2) ^ (ip << i);
        for(int h=0; h<ittage_hlen[i]; h+=idx_width)
        {
            ghr = (btb_ghr[h / 32] >> (h % 32));
            if(ittage_hlen[i] < (h+idx_width)) ghr &= ((1 << (ittage_hlen[i]-h)) - 1);
            ittage_hash_idx[i] ^= ghr;
        }
        for(int h=0; h<ittage_hlen[i]; h+=tag_width)
        {
            ghr = (btb_ghr[h / 32] >> (h % 32));
            if(ittage_hlen[i] < (h+tag_width)) ghr &= ((1 << (ittage_hlen[i]-h)) - 1);
            ittage_hash_tag[i] ^= ghr;
        }
        for(int h=0; h<ittage_hlen[i]; h+=tag_width-1)
        {
            ghr = (btb_ghr[h / 32] >> (h % 32));
            if(ittage_hlen[i] < (h+tag_width-1)) ghr &= ((1 << (ittage_hlen[i]-h)) - 1);
            ittage_hash_tag[i] ^= ghr << 1;
        }
        ittage_hash_idx[i] &= (1 << idx_width) - 1;
        ittage_hash_tag[i] &= (1 << tag_width) - 1;
    }

    uint64_t ittage_alt_pred = target;
    uint64_t ittage_hit_pred = target;
    uint64_t confidence = 0;
    for(int i=0; i<ITTAGE_NTAB; i=i+1)
    {
        if(ittage_set[i][ittage_hash_idx[i]].tag == ittage_hash_tag[i])
        {
            ittage_alt_pred = ittage_hit_pred;
            ittage_hit_pred = ittage_set[i][ittage_hash_idx[i]].target;
            confidence = ittage_set[i][ittage_hash_idx[i]].confidence;
            ittage_hit_bank = i;
        }
    }
    ittage_alt_diff = ittage_hit_pred != ittage_alt_pred;
    target = ( (confidence == 0) && (ittage_use_alt >= 0) ) ? ittage_alt_pred : ittage_hit_pred;
    return target;
}

// indirect predictor update
void BRANCH_TARGET_PRED::ittage_update(BTB_entry& hit_entry, uint64_t target, bool mispred)
{
    // hitting entry update
    if(ittage_hit_bank >= 0)
    {
        ITTAGE_entry& ittage_entry = ittage_set[ittage_hit_bank][ittage_hash_idx[ittage_hit_bank]];

        // update use_alt
        if(ittage_entry.confidence == 0)
        {
            if(ittage_alt_diff)
            {
                if(ittage_alt_pred == target) ittage_use_alt++;
                if(ittage_hit_pred == target) ittage_use_alt--;
                if(ittage_use_alt >  7) ittage_use_alt =  7;
                if(ittage_use_alt < -8) ittage_use_alt = -8;
            }
        }

        // update confidence and RRPV
        if(ittage_entry.target == target)
        {
            if(ittage_alt_diff)
            {
                if(ittage_entry.rrpv < 3) ittage_entry.rrpv++;
            }
            if(ittage_entry.confidence < 3)
            {
                ittage_entry.confidence++;
            }
        }
        else
        {
            if(ittage_entry.confidence > 0)
            {
                ittage_entry.confidence--;
            }
            else
            {
                ittage_entry.target = target;
            }
        }
    }
    else // default prediction
    {
        if(hit_entry.target == target)
        {
            if(hit_entry.confidence < 3)
            {
                hit_entry.confidence++;
            }
        }
        else
        {
            if(hit_entry.confidence > 0)
            {
                hit_entry.confidence--;
            }
            else
            {
                hit_entry.target = target;
            }
        }
    }

    // allocation
    if(mispred && (ittage_hit_pred != target))
    {
        for(int64_t i=ittage_hit_bank+1; i<ITTAGE_NTAB; i++)
        {
            if(ittage_set[i][ittage_hash_idx[i]].rrpv > 3)
            {
                ittage_set[i][ittage_hash_idx[i]].rrpv = 0;
            }
            if(rand() & 3) // skip 25%
            {
                if(ittage_set[i][ittage_hash_idx[i]].rrpv == 0)
                {
                    // allocate
                    ittage_set[i][ittage_hash_idx[i]].tag = ittage_hash_tag[i];
                    ittage_set[i][ittage_hash_idx[i]].target = target;
                    ittage_set[i][ittage_hash_idx[i]].confidence = 0;
                    ittage_set[i][ittage_hash_idx[i]].rrpv = 0;
                    ittage_tick -= 1;
                    if(ittage_tick < 0) ittage_tick = 0;
                    break;
                }
                else
                {
                    // fail to allocate
                    ittage_tick += 1;
                }
            }
        }

        if(ittage_tick > 500)
        {
            // reset useful
            for(int i=0; i<ITTAGE_NTAB; i++)
            {
                for(int j=0; j<ITTAGE_IDX_SIZE; j++)
                {
                    if(ittage_set[i][j].rrpv > 0) ittage_set[i][j].rrpv--;
                }
            }
            ittage_tick = 0;
        }
    }
}

// update branch predictor history
void BRANCH_TARGET_PRED::update_history(uint64_t ip, uint8_t type, uint64_t target)
{
    // branch target history hash computation
    uint64_t hash = (target >> 2) & 0xffULL ;
    for(int i=0; i<16; i=i+1) // 16 * 32 = 512-bit total
    {
        btb_ghr[i] <<= 2;
        btb_ghr[i] ^= hash;
        hash = (btb_ghr[i] >> 32) & 3;
    }

    // return stack maintenance
    if((type == BRANCH_DIRECT_CALL) || (type == BRANCH_INDIRECT_CALL))
    {
        if(ras_size < RAS_SIZE) ras_size++;
        ras_top = (ras_top + 1) & (RAS_SIZE - 1);
        ras[ras_top] = ip + 4;
    }
    else if(type == BRANCH_RETURN)
    {
        if(ras_size > 0) ras_size--;
        ras_top = (ras_top + RAS_SIZE - 1) & (RAS_SIZE - 1);
    }
    else if((ras_size > 0) && (ras[ras_top]==target))
    {
        if(ras_size > 0) ras_size--;
        ras_top = (ras_top + RAS_SIZE - 1) & (RAS_SIZE - 1);
    }
}
