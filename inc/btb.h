#ifndef BTB_H
#define BTB_H

// BTB
class BRANCH_TARGET_PRED
{
private:
    // BTB common structure
    struct BTB_entry {
        uint64_t tag; // (partial) tag
        uint64_t target; // target
        uint64_t indirect; // indirect branch indication for BTB
        uint64_t confidence; // confidence for indirect branches
        uint64_t rrpv; // replacement information, useful bit for ITTAGE
    };
    struct ITTAGE_entry {
        uint64_t tag; // (partial) tag
        uint64_t target; // target
        uint64_t confidence; // confidence for indirect branches
        uint64_t rrpv; // replacement information, useful bit for ITTAGE
    };

    // parameters
    static const int MIN_HIST = 4;
    static const int MAX_HIST = 260;
    static const int ITTAGE_NTAB = 8;
    static const int ITTAGE_TAG_NBIT = 13;
    static const int ITTAGE_IDX_NBIT = 9;
    static const int ITTAGE_IDX_SIZE = 1 << ITTAGE_IDX_NBIT;
    static const int UBTB_NWAY = 2;
    static const int UBTB_NBIT = 6;
    static const int UBTB_NSET = 1 << UBTB_NBIT;
    static const int BTB_NWAY = 4;
    static const int BTB_NBIT = 11;
    static const int BTB_NSET = 1 << BTB_NBIT;
    static const int RAS_SIZE = 32;
    static const uint32_t BTB_OFFSET = 4;
    static const uint32_t BTB_SET_MASK = (1 << 11) - 1;

    // branch history
    uint64_t btb_ghr[16];

    // BTB structures
    BTB_entry ubtb_set[UBTB_NSET][UBTB_NWAY];
    BTB_entry btb_set[BTB_NSET][BTB_NWAY];
    ITTAGE_entry ittage_set[ITTAGE_NTAB][ITTAGE_IDX_SIZE];

    // ITTAGE
    int ittage_hlen[ITTAGE_NTAB];
    int64_t ittage_tick;
    int64_t ittage_hit_bank;
    int64_t ittage_alt_diff;
    int64_t ittage_use_alt;
    uint64_t ittage_hit_pred;
    uint64_t ittage_alt_pred;
    uint64_t ittage_hash_idx[ITTAGE_NTAB];
    uint64_t ittage_hash_tag[ITTAGE_NTAB];

    // return stack
    uint64_t ras[RAS_SIZE], ras_size, ras_top;

    // indirect predictor access
    uint64_t ittage_predict(uint64_t ip, uint64_t target);
    void ittage_update(BTB_entry& hit_entry, uint64_t target, bool mispred);

public:
    // snoop return stack
    uint64_t predict_ras(uint64_t target) { return (ras_size > 0) ? ras[ras_top] : target; }

    // make target prediction
    uint8_t predict_target(uint64_t ip, uint8_t type, uint64_t &target);

    // update / initialize predictor
    void    update_target(uint64_t ip, uint8_t type, uint64_t target);
    void    update_history(uint64_t ip, uint8_t type, uint64_t target);
    void    initialize();
};

#endif
