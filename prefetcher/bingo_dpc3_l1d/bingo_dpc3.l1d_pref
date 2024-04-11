/* Bingo [https://mshakerinava.github.io/papers/bingo-hpca19.pdf] */

#include "cache.h"
#include <bits/stdc++.h>

using namespace std;

namespace L1D_PREF {

/**
 * A class for printing beautiful data tables.
 * It's useful for logging the information contained in tabular structures.
 */
class Table {
  public:
    Table(int width, int height) : width(width), height(height), cells(height, vector<string>(width)) {}

    void set_row(int row, const vector<string> &data, int start_col = 0) {
        // assert(data.size() + start_col == this->width);
        for (unsigned col = start_col; col < this->width; col += 1)
            this->set_cell(row, col, data[col]);
    }

    void set_col(int col, const vector<string> &data, int start_row = 0) {
        // assert(data.size() + start_row == this->height);
        for (unsigned row = start_row; row < this->height; row += 1)
            this->set_cell(row, col, data[row]);
    }

    void set_cell(int row, int col, string data) {
        // assert(0 <= row && row < (int)this->height);
        // assert(0 <= col && col < (int)this->width);
        this->cells[row][col] = data;
    }

    void set_cell(int row, int col, double data) {
        ostringstream oss;
        oss << setw(11) << fixed << setprecision(8) << data;
        this->set_cell(row, col, oss.str());
    }

    void set_cell(int row, int col, int64_t data) {
        ostringstream oss;
        oss << setw(11) << std::left << data;
        this->set_cell(row, col, oss.str());
    }

    void set_cell(int row, int col, int data) { this->set_cell(row, col, (int64_t)data); }

    void set_cell(int row, int col, uint64_t data) {
        ostringstream oss;
        oss << "0x" << setfill('0') << setw(16) << hex << data;
        this->set_cell(row, col, oss.str());
    }

    /**
     * @return The entire table as a string
     */
    string to_string() {
        vector<int> widths;
        for (unsigned i = 0; i < this->width; i += 1) {
            int max_width = 0;
            for (unsigned j = 0; j < this->height; j += 1)
                max_width = max(max_width, (int)this->cells[j][i].size());
            widths.push_back(max_width + 2);
        }
        string out;
        out += Table::top_line(widths);
        out += this->data_row(0, widths);
        for (unsigned i = 1; i < this->height; i += 1) {
            out += Table::mid_line(widths);
            out += this->data_row(i, widths);
        }
        out += Table::bot_line(widths);
        return out;
    }

    string data_row(int row, const vector<int> &widths) {
        string out;
        for (unsigned i = 0; i < this->width; i += 1) {
            string data = this->cells[row][i];
            data.resize(widths[i] - 2, ' ');
            out += " | " + data;
        }
        out += " |\n";
        return out;
    }

    static string top_line(const vector<int> &widths) { return Table::line(widths, "┌", "┬", "┐"); }

    static string mid_line(const vector<int> &widths) { return Table::line(widths, "├", "┼", "┤"); }

    static string bot_line(const vector<int> &widths) { return Table::line(widths, "└", "┴", "┘"); }

    static string line(const vector<int> &widths, string left, string mid, string right) {
        string out = " " + left;
        for (unsigned i = 0; i < widths.size(); i += 1) {
            int w = widths[i];
            for (int j = 0; j < w; j += 1)
                out += "─";
            if (i != widths.size() - 1)
                out += mid;
            else
                out += right;
        }
        return out + "\n";
    }

  private:
    unsigned width;
    unsigned height;
    vector<vector<string>> cells;
};

template <class T> class SetAssociativeCache {
  public:
    class Entry {
      public:
        uint64_t key;
        uint64_t index;
        uint64_t tag;
        bool valid;
        T data;
    };

    SetAssociativeCache(int size, int num_ways, int debug_level = 0)
        : size(size), num_ways(num_ways), num_sets(size / num_ways), entries(num_sets, vector<Entry>(num_ways)),
          cams(num_sets), debug_level(debug_level) {
        // assert(size % num_ways == 0);
        for (int i = 0; i < num_sets; i += 1)
            for (int j = 0; j < num_ways; j += 1)
                entries[i][j].valid = false;
        /* calculate `index_len` (number of bits required to store the index) */
        for (int max_index = num_sets - 1; max_index > 0; max_index >>= 1)
            this->index_len += 1;
    }

    /**
     * Invalidates the entry corresponding to the given key.
     * @return A pointer to the invalidated entry
     */
    Entry *erase(uint64_t key) {
        Entry *entry = this->find(key);
        uint64_t index = key % this->num_sets;
        uint64_t tag = key / this->num_sets;
        auto &cam = cams[index];
        int num_erased = cam.erase(tag);
        if (entry)
            entry->valid = false;
        // assert(entry ? num_erased == 1 : num_erased == 0);
        return entry;
    }

    /**
     * @return The old state of the entry that was updated
     */
    Entry insert(uint64_t key, const T &data) {
        Entry *entry = this->find(key);
        if (entry != nullptr) {
            Entry old_entry = *entry;
            entry->data = data;
            return old_entry;
        }
        uint64_t index = key % this->num_sets;
        uint64_t tag = key / this->num_sets;
        vector<Entry> &set = this->entries[index];
        int victim_way = -1;
        for (int i = 0; i < this->num_ways; i += 1)
            if (!set[i].valid) {
                victim_way = i;
                break;
            }
        if (victim_way == -1) {
            victim_way = this->select_victim(index);
        }
        Entry &victim = set[victim_way];
        Entry old_entry = victim;
        victim = {key, index, tag, true, data};
        auto &cam = cams[index];
        if (old_entry.valid) {
            int num_erased = cam.erase(old_entry.tag);
            // assert(num_erased == 1);
        }
        cam[tag] = victim_way;
        return old_entry;
    }

    Entry *find(uint64_t key) {
        uint64_t index = key % this->num_sets;
        uint64_t tag = key / this->num_sets;
        auto &cam = cams[index];
        if (cam.find(tag) == cam.end())
            return nullptr;
        int way = cam[tag];
        Entry &entry = this->entries[index][way];
        // assert(entry.tag == tag && entry.valid);
        return &entry;
    }

    /**
     * Creates a table with the given headers and populates the rows by calling `write_data` on all
     * valid entries contained in the cache. This function makes it easy to visualize the contents
     * of a cache.
     * @return The constructed table as a string
     */
    string log(vector<string> headers) {
        vector<Entry> valid_entries = this->get_valid_entries();
        Table table(headers.size(), valid_entries.size() + 1);
        table.set_row(0, headers);
        for (unsigned i = 0; i < valid_entries.size(); i += 1)
            this->write_data(valid_entries[i], table, i + 1);
        return table.to_string();
    }

    int get_index_len() { return this->index_len; }

    void set_debug_level(int debug_level) { this->debug_level = debug_level; }

  protected:
    /* should be overriden in children */
    virtual void write_data(Entry &entry, Table &table, int row) {}

    /**
     * @return The way of the selected victim
     */
    virtual int select_victim(uint64_t index) {
        /* random eviction policy if not overriden */
        return rand() % this->num_ways;
    }

    vector<Entry> get_valid_entries() {
        vector<Entry> valid_entries;
        for (int i = 0; i < num_sets; i += 1)
            for (int j = 0; j < num_ways; j += 1)
                if (entries[i][j].valid)
                    valid_entries.push_back(entries[i][j]);
        return valid_entries;
    }

    int size;
    int num_ways;
    int num_sets;
    int index_len = 0; /* in bits */
    vector<vector<Entry>> entries;
    vector<unordered_map<uint64_t, int>> cams;
    int debug_level = 0;
};

template <class T> class LRUSetAssociativeCache : public SetAssociativeCache<T> {
    typedef SetAssociativeCache<T> Super;

  public:
    LRUSetAssociativeCache(int size, int num_ways, int debug_level = 0)
        : Super(size, num_ways, debug_level), lru(this->num_sets, vector<uint64_t>(num_ways)) {}

    void set_mru(uint64_t key) { *this->get_lru(key) = this->t++; }

    void set_lru(uint64_t key) { *this->get_lru(key) = 0; }

  protected:
    /* @override */
    int select_victim(uint64_t index) {
        vector<uint64_t> &lru_set = this->lru[index];
        return min_element(lru_set.begin(), lru_set.end()) - lru_set.begin();
    }

    uint64_t *get_lru(uint64_t key) {
        uint64_t index = key % this->num_sets;
        uint64_t tag = key / this->num_sets;
        // assert(this->cams[index].count(tag) == 1);
        int way = this->cams[index][tag];
        return &this->lru[index][way];
    }

    vector<vector<uint64_t>> lru;
    uint64_t t = 1;
};

/**
 * A very simple and efficient hash function that:
 * 1) Splits key into blocks of length `index_len` bits and computes the XOR of all blocks.
 * 2) Replaces the least significant block of key with computed block.
 * With this hash function, the index will depend on all bits in the key. As a consequence, entries
 * will be more randomly distributed among the sets.
 * NOTE: Applying this hash function twice with the same `index_len` acts as the identity function.
 */ 
uint64_t hash_index(uint64_t key, int index_len) {
    if (index_len == 0)
        return key;
    for (uint64_t tag = (key >> index_len); tag > 0; tag >>= index_len)
        key ^= tag & ((1 << index_len) - 1);
    return key;
}

/*=== End Of Cache Framework ===*/

class FilterTableData {
  public:
    uint64_t pc;
    int offset;
};

class FilterTable : public LRUSetAssociativeCache<FilterTableData> {
    typedef LRUSetAssociativeCache<FilterTableData> Super;

  public:
    FilterTable(int size, int debug_level = 0, int num_ways = 16) : Super(size, num_ways, debug_level) {
        // assert(__builtin_popcount(size) == 1);
        if (this->debug_level >= 1)
            cerr << "FilterTable::FilterTable(size=" << size << ", debug_level=" << debug_level
                 << ", num_ways=" << num_ways << ")" << dec << endl;
    }

    Entry *find(uint64_t region_number) {
        if (this->debug_level >= 2)
            cerr << "FilterTable::find(region_number=0x" << hex << region_number << ")" << dec << endl;
        uint64_t key = this->build_key(region_number);
        Entry *entry = Super::find(key);
        if (!entry) {
            if (this->debug_level >= 2)
                cerr << "[FilterTable::find] Miss!" << dec << endl;
            return nullptr;
        }
        if (this->debug_level >= 2)
            cerr << "[FilterTable::find] Hit!" << dec << endl;
        Super::set_mru(key);
        return entry;
    }

    void insert(uint64_t region_number, uint64_t pc, int offset) {
        if (this->debug_level >= 2)
            cerr << "FilterTable::insert(region_number=0x" << hex << region_number << ", pc=0x" << pc
                 << ", offset=" << dec << offset << ")" << dec << endl;
        uint64_t key = this->build_key(region_number);
        // assert(!Super::find(key));
        Super::insert(key, {pc, offset});
        Super::set_mru(key);
    }

    Entry *erase(uint64_t region_number) {
        uint64_t key = this->build_key(region_number);
        return Super::erase(key);
    }

    string log() {
        vector<string> headers({"Region", "PC", "Offset"});
        return Super::log(headers);
    }

  private:
    /* @override */
    void write_data(Entry &entry, Table &table, int row) {
        uint64_t key = hash_index(entry.key, this->index_len);
        table.set_cell(row, 0, key);
        table.set_cell(row, 1, entry.data.pc);
        table.set_cell(row, 2, entry.data.offset);
    }

    uint64_t build_key(uint64_t region_number) {
        uint64_t key = region_number & ((1ULL << 37) - 1);
        return hash_index(key, this->index_len);
    }

    /*==========================================================*/
    /* Entry   = [tag, offset, PC, valid, LRU]                  */
    /* Storage = size * (37 - lg(sets) + 5 + 16 + 1 + lg(ways)) */
    /* 64 * (37 - lg(4) + 5 + 16 + 1 + lg(16)) = 488 Bytes      */
    /*==========================================================*/
};

template <class T> string pattern_to_string(const vector<T> &pattern) {
    ostringstream oss;
    for (unsigned i = 0; i < pattern.size(); i += 1)
        oss << int(pattern[i]);
    return oss.str();
}

class AccumulationTableData {
  public:
    uint64_t pc;
    int offset;
    vector<bool> pattern;
};

class AccumulationTable : public LRUSetAssociativeCache<AccumulationTableData> {
    typedef LRUSetAssociativeCache<AccumulationTableData> Super;

  public:
    AccumulationTable(int size, int pattern_len, int debug_level = 0, int num_ways = 16)
        : Super(size, num_ways, debug_level), pattern_len(pattern_len) {
        // assert(__builtin_popcount(size) == 1);
        // assert(__builtin_popcount(pattern_len) == 1);
        if (this->debug_level >= 1)
            cerr << "AccumulationTable::AccumulationTable(size=" << size << ", pattern_len=" << pattern_len
                 << ", debug_level=" << debug_level << ", num_ways=" << num_ways << ")" << dec << endl;
    }

    /**
     * @return False if the tag wasn't found and true if the pattern bit was successfully set
     */
    bool set_pattern(uint64_t region_number, int offset) {
        if (this->debug_level >= 2)
            cerr << "AccumulationTable::set_pattern(region_number=0x" << hex << region_number << ", offset=" << dec
                 << offset << ")" << dec << endl;
        uint64_t key = this->build_key(region_number);
        Entry *entry = Super::find(key);
        if (!entry) {
            if (this->debug_level >= 2)
                cerr << "[AccumulationTable::set_pattern] Not found!" << dec << endl;
            return false;
        }
        entry->data.pattern[offset] = true;
        Super::set_mru(key);
        if (this->debug_level >= 2)
            cerr << "[AccumulationTable::set_pattern] OK!" << dec << endl;
        return true;
    }

    /* NOTE: `region_number` is probably truncated since it comes from the filter table */
    Entry insert(uint64_t region_number, uint64_t pc, int offset) {
        if (this->debug_level >= 2)
            cerr << "AccumulationTable::insert(region_number=0x" << hex << region_number << ", pc=0x" << pc
                 << ", offset=" << dec << offset << dec << endl;
        uint64_t key = this->build_key(region_number);
        // assert(!Super::find(key));
        vector<bool> pattern(this->pattern_len, false);
        pattern[offset] = true;
        Entry old_entry = Super::insert(key, {pc, offset, pattern});
        Super::set_mru(key);
        return old_entry;
    }

    Entry *erase(uint64_t region_number) {
        uint64_t key = this->build_key(region_number);
        return Super::erase(key);
    }

    string log() {
        vector<string> headers({"Region", "PC", "Offset", "Pattern"});
        return Super::log(headers);
    }

  private:
    /* @override */
    void write_data(Entry &entry, Table &table, int row) {
        uint64_t key = hash_index(entry.key, this->index_len);
        table.set_cell(row, 0, key);
        table.set_cell(row, 1, entry.data.pc);
        table.set_cell(row, 2, entry.data.offset);
        table.set_cell(row, 3, pattern_to_string(entry.data.pattern));
    }

    uint64_t build_key(uint64_t region_number) {
        uint64_t key = region_number & ((1ULL << 37) - 1);
        return hash_index(key, this->index_len);
    }

    int pattern_len;

    /*===============================================================*/
    /* Entry   = [tag, map, offset, PC, valid, LRU]                  */
    /* Storage = size * (37 - lg(sets) + 32 + 5 + 16 + 1 + lg(ways)) */
    /* 128 * (37 - lg(8) + 32 + 5 + 16 + 1 + lg(16)) = 1472 Bytes    */
    /*===============================================================*/
};

/**
 * There are 3 possible outcomes (here called `Event`) for a PHT lookup:
 * PC+Address hit, PC+Offset hit(s), or Miss.
 * NOTE: `Event` is only used for gathering stats.
 */
enum Event { PC_ADDRESS = 0, PC_OFFSET = 1, MISS = 2 };

template <class T> vector<T> my_rotate(const vector<T> &x, int n) {
    vector<T> y;
    int len = x.size();
    n = n % len;
    for (int i = 0; i < len; i += 1)
        y.push_back(x[(i - n + len) % len]);
    return y;
}

class PatternHistoryTableData {
  public:
    vector<bool> pattern;
};

class PatternHistoryTable : public LRUSetAssociativeCache<PatternHistoryTableData> {
    typedef LRUSetAssociativeCache<PatternHistoryTableData> Super;

  public:
    PatternHistoryTable(int size, int pattern_len, int min_addr_width, int max_addr_width, int pc_width,
        int debug_level = 0, int num_ways = 16)
        : Super(size, num_ways, debug_level), pattern_len(pattern_len), min_addr_width(min_addr_width),
          max_addr_width(max_addr_width), pc_width(pc_width) {
        // assert(this->pc_width >= 0);
        // assert(this->min_addr_width >= 0);
        // assert(this->max_addr_width >= 0);
        // assert(this->max_addr_width >= this->min_addr_width);
        // assert(this->pc_width + this->min_addr_width > 0);
        // assert(__builtin_popcount(pattern_len) == 1);
        if (this->debug_level >= 1)
            cerr << "PatternHistoryTable::PatternHistoryTable(size=" << size << ", pattern_len=" << pattern_len
                 << ", min_addr_width=" << min_addr_width << ", max_addr_width=" << max_addr_width
                 << ", pc_width=" << pc_width << ", debug_level=" << debug_level << ", num_ways=" << num_ways << ")"
                 << dec << endl;
    }

    /* NOTE: In BINGO, address is actually block number. */
    void insert(uint64_t pc, uint64_t address, vector<bool> pattern) {
        if (this->debug_level >= 2)
            cerr << "PatternHistoryTable::insert(pc=0x" << hex << pc << ", address=0x" << address
                 << ", pattern=" << pattern_to_string(pattern) << ")" << dec << endl;
        // assert((int)pattern.size() == this->pattern_len);
        int offset = address % this->pattern_len;
        pattern = my_rotate(pattern, -offset);
        uint64_t key = this->build_key(pc, address);
        Super::insert(key, {pattern});
        Super::set_mru(key);
    }

    /**
     * First searches for a PC+Address match. If no match is found, returns all PC+Offset matches.
     * @return All un-rotated patterns if matches were found, returns an empty vector otherwise
     */
    vector<vector<bool>> find(uint64_t pc, uint64_t address) {
        if (this->debug_level >= 2)
            cerr << "PatternHistoryTable::find(pc=0x" << hex << pc << ", address=0x" << address << ")" << dec << endl;
        uint64_t key = this->build_key(pc, address);
        uint64_t index = key % this->num_sets;
        uint64_t tag = key / this->num_sets;
        auto &set = this->entries[index];
        uint64_t min_tag_mask = (1 << (this->pc_width + this->min_addr_width - this->index_len)) - 1;
        uint64_t max_tag_mask = (1 << (this->pc_width + this->max_addr_width - this->index_len)) - 1;
        vector<vector<bool>> matches;
        this->last_event = MISS;
        for (int i = 0; i < this->num_ways; i += 1) {
            if (!set[i].valid)
                continue;
            bool min_match = ((set[i].tag & min_tag_mask) == (tag & min_tag_mask));
            bool max_match = ((set[i].tag & max_tag_mask) == (tag & max_tag_mask));
            vector<bool> &cur_pattern = set[i].data.pattern;
            if (max_match) {
                this->last_event = PC_ADDRESS;
                Super::set_mru(set[i].key);
                matches.clear();
                matches.push_back(cur_pattern);
                break;
            }
            if (min_match) {
                this->last_event = PC_OFFSET;
                matches.push_back(cur_pattern);
            }
        }
        int offset = address % this->pattern_len;
        for (int i = 0; i < (int)matches.size(); i += 1)
            matches[i] = my_rotate(matches[i], +offset);
        return matches;
    }

    Event get_last_event() { return this->last_event; }

    string log() {
        vector<string> headers({"PC", "Offset", "Address", "Pattern"});
        return Super::log(headers);
    }

  private:
    /* @override */
    void write_data(Entry &entry, Table &table, int row) {
        uint64_t base_key = entry.key >> (this->pc_width + this->min_addr_width);
        uint64_t index_key = entry.key & ((1 << (this->pc_width + this->min_addr_width)) - 1);
        index_key = hash_index(index_key, this->index_len); /* unhash */
        uint64_t key = (base_key << (this->pc_width + this->min_addr_width)) | index_key;

        /* extract PC, offset, and address */
        uint64_t offset = key & ((1 << this->min_addr_width) - 1);
        key >>= this->min_addr_width;
        uint64_t pc = key & ((1 << this->pc_width) - 1);
        key >>= this->pc_width;
        uint64_t address = (key << this->min_addr_width) + offset;

        table.set_cell(row, 0, pc);
        table.set_cell(row, 1, offset);
        table.set_cell(row, 2, address);
        table.set_cell(row, 3, pattern_to_string(entry.data.pattern));
    }

    uint64_t build_key(uint64_t pc, uint64_t address) {
        pc &= (1 << this->pc_width) - 1;            /* use `pc_width` bits from pc */
        address &= (1 << this->max_addr_width) - 1; /* use `addr_width` bits from address */
        uint64_t offset = address & ((1 << this->min_addr_width) - 1);
        uint64_t base = (address >> this->min_addr_width);
        /* key = base + hash_index( pc + offset )
         * The index must be computed from only PC+Offset to ensure that all entries with the same
         * PC+Offset end up in the same set */
        uint64_t index_key = hash_index((pc << this->min_addr_width) | offset, this->index_len);
        uint64_t key = (base << (this->pc_width + this->min_addr_width)) | index_key;
        return key;
    }

    int pattern_len;
    int min_addr_width, max_addr_width, pc_width;
    Event last_event;

    /*======================================================*/
    /* Entry   = [tag, map, valid, LRU]                     */
    /* Storage = size * (32 - lg(sets) + 32 + 1 + lg(ways)) */
    /* 8K * (32 - lg(512) + 32 + 1 + lg(16)) = 60K Bytes    */
    /*======================================================*/
};

class PrefetchStreamerData {
  public:
    /* contains the prefetch fill level for each block of spatial region */
    vector<int> pattern;
};

class PrefetchStreamer : public LRUSetAssociativeCache<PrefetchStreamerData> {
    typedef LRUSetAssociativeCache<PrefetchStreamerData> Super;

  public:
    PrefetchStreamer(int size, int pattern_len, int debug_level = 0, int num_ways = 16)
        : Super(size, num_ways, debug_level), pattern_len(pattern_len) {
        if (this->debug_level >= 1)
            cerr << "PrefetchStreamer::PrefetchStreamer(size=" << size << ", pattern_len=" << pattern_len
                 << ", debug_level=" << debug_level << ", num_ways=" << num_ways << ")" << dec << endl;
    }

    void insert(uint64_t region_number, vector<int> pattern) {
        if (this->debug_level >= 2)
            cerr << "PrefetchStreamer::insert(region_number=0x" << hex << region_number
                 << ", pattern=" << pattern_to_string(pattern) << ")" << dec << endl;
        uint64_t key = this->build_key(region_number);
        Super::insert(key, {pattern});
        Super::set_mru(key);
    }

    int prefetch(CACHE *cache, uint64_t block_address) {
        if (this->debug_level >= 2) {
            cerr << "PrefetchStreamer::prefetch(cache=" << cache->NAME << ", block_address=0x" << hex << block_address
                 << ")" << dec << endl;
            cerr << "[PrefetchStreamer::prefetch] " << cache->PQ.occupancy << "/" << cache->PQ.SIZE
                 << " PQ entries occupied." << dec << endl;
            cerr << "[PrefetchStreamer::prefetch] " << cache->MSHR.occupancy << "/" << cache->MSHR.SIZE
                 << " MSHR entries occupied." << dec << endl;
        }
        uint64_t base_addr = block_address << LOG2_BLOCK_SIZE;
        int region_offset = block_address % this->pattern_len;
        uint64_t region_number = block_address / this->pattern_len;
        uint64_t key = this->build_key(region_number);
        Entry *entry = Super::find(key);
        if (!entry) {
            if (this->debug_level >= 2)
                cerr << "[PrefetchStreamer::prefetch] No entry found." << dec << endl;
            return 0;
        }
        Super::set_mru(key);
        int pf_issued = 0;
        vector<int> &pattern = entry->data.pattern;
        pattern[region_offset] = 0; /* accessed block will be automatically fetched if necessary (miss) */
        int pf_offset;
        /* prefetch blocks that are close to the recent access first (locality!) */
        for (int d = 1; d < this->pattern_len; d += 1) {
            /* prefer positive strides */
            for (int sgn = +1; sgn >= -1; sgn -= 2) {
                pf_offset = region_offset + sgn * d;
                if (0 <= pf_offset && pf_offset < this->pattern_len && pattern[pf_offset] > 0) {
                    uint64_t pf_address = (region_number * this->pattern_len + pf_offset) << LOG2_BLOCK_SIZE;
                    if (cache->PQ.occupancy + cache->MSHR.occupancy < cache->MSHR.SIZE - 1 &&
                        cache->PQ.occupancy < cache->PQ.SIZE) {
                        int ok = cache->prefetch_line(0, base_addr, pf_address, pattern[pf_offset], 0);
                        // assert(ok == 1);
                        pf_issued += 1;
                        pattern[pf_offset] = 0;
                    } else {
                        /* prefetching limit is reached */
                        return pf_issued;
                    }
                }
            }
        }
        /* all prefetches done for this spatial region */
        Super::erase(key);
        return pf_issued;
    }

    string log() {
        vector<string> headers({"Region", "Pattern"});
        return Super::log(headers);
    }

  private:
    /* @override */
    void write_data(Entry &entry, Table &table, int row) {
        uint64_t key = hash_index(entry.key, this->index_len);
        table.set_cell(row, 0, key);
        table.set_cell(row, 1, pattern_to_string(entry.data.pattern));
    }

    uint64_t build_key(uint64_t region_number) { return hash_index(region_number, this->index_len); }

    int pattern_len;

    /*======================================================*/
    /* Entry   = [tag, map, valid, LRU]                     */
    /* Storage = size * (53 - lg(sets) + 64 + 1 + lg(ways)) */
    /* 128 * (53 - lg(8) + 64 + 1 + lg(16)) = 1904 Bytes    */
    /*======================================================*/
};

template <class T> inline T square(T x) { return x * x; }

class Bingo {
  public:
    Bingo(int pattern_len, int min_addr_width, int max_addr_width, int pc_width, int filter_table_size,
        int accumulation_table_size, int pht_size, int pht_ways, int pf_streamer_size, int debug_level = 0)
        : pattern_len(pattern_len), filter_table(filter_table_size, debug_level),
          accumulation_table(accumulation_table_size, pattern_len, debug_level),
          pht(pht_size, pattern_len, min_addr_width, max_addr_width, pc_width, debug_level, pht_ways),
          pf_streamer(pf_streamer_size, pattern_len, debug_level), debug_level(debug_level) {
        if (this->debug_level >= 1)
            cerr << "Bingo::Bingo(pattern_len=" << pattern_len << ", min_addr_width=" << min_addr_width
                 << ", max_addr_width=" << max_addr_width << ", pc_width=" << pc_width
                 << ", filter_table_size=" << filter_table_size
                 << ", accumulation_table_size=" << accumulation_table_size << ", pht_size=" << pht_size
                 << ", pht_ways=" << pht_ways << ", pf_streamer_size=" << pf_streamer_size
                 << ", debug_level=" << debug_level << ")" << endl;
    }

    /**
     * Updates BINGO's state based on the most recent LOAD access.
     * @param block_number The block address of the most recent LOAD access
     * @param pc           The PC of the most recent LOAD access
     */
    void access(uint64_t block_number, uint64_t pc) {
        if (this->debug_level >= 2)
            cerr << "[Bingo] access(block_number=0x" << hex << block_number << ", pc=0x" << pc << ")" << dec << endl;
        uint64_t region_number = block_number / this->pattern_len;
        int region_offset = block_number % this->pattern_len;
        bool success = this->accumulation_table.set_pattern(region_number, region_offset);
        if (success)
            return;
        FilterTable::Entry *entry = this->filter_table.find(region_number);
        if (!entry) {
            /* trigger access */
            this->filter_table.insert(region_number, pc, region_offset);
            vector<int> pattern = this->find_in_pht(pc, block_number);
            if (pattern.empty()) {
                /* nothing to prefetch */
                return;
            }
            /* give pattern to `pf_streamer` */
            // assert((int)pattern.size() == this->pattern_len);
            this->pf_streamer.insert(region_number, pattern);
            return;
        }
        if (entry->data.offset != region_offset) {
            /* move from filter table to accumulation table */
            uint64_t region_number = hash_index(entry->key, this->filter_table.get_index_len());
            AccumulationTable::Entry victim =
                this->accumulation_table.insert(region_number, entry->data.pc, entry->data.offset);
            this->accumulation_table.set_pattern(region_number, region_offset);
            this->filter_table.erase(region_number);
            if (victim.valid) {
                /* move from accumulation table to PHT */
                this->insert_in_pht(victim);
            }
        }
    }

    void eviction(uint64_t block_number) {
        if (this->debug_level >= 2)
            cerr << "[Bingo] eviction(block_number=" << block_number << ")" << dec << endl;
        /* end of generation: footprint must now be stored in PHT */
        uint64_t region_number = block_number / this->pattern_len;
        this->filter_table.erase(region_number);
        AccumulationTable::Entry *entry = this->accumulation_table.erase(region_number);
        if (entry) {
            /* move from accumulation table to PHT */
            this->insert_in_pht(*entry);
        }
    }

    int prefetch(CACHE *cache, uint64_t block_number) {
        if (this->debug_level >= 2)
            cerr << "Bingo::prefetch(cache=" << cache->NAME << ", block_number=" << hex << block_number << ")" << dec
                 << endl;
        int pf_issued = this->pf_streamer.prefetch(cache, block_number);
        if (this->debug_level >= 2)
            cerr << "[Bingo::prefetch] pf_issued=" << pf_issued << dec << endl;
        return pf_issued;
    }

    void set_debug_level(int debug_level) {
        this->filter_table.set_debug_level(debug_level);
        this->accumulation_table.set_debug_level(debug_level);
        this->pht.set_debug_level(debug_level);
        this->pf_streamer.set_debug_level(debug_level);
        this->debug_level = debug_level;
    }

    void log() {
        cerr << "Filter Table:" << dec << endl;
        cerr << this->filter_table.log();

        cerr << "Accumulation Table:" << dec << endl;
        cerr << this->accumulation_table.log();

        cerr << "Pattern History Table:" << dec << endl;
        cerr << this->pht.log();

        cerr << "Prefetch Streamer:" << dec << endl;
        cerr << this->pf_streamer.log();
    }

    /*========== stats ==========*/
    /* NOTE: the BINGO code submitted for DPC3 (this code) does not call any of these methods. */

    Event get_event(uint64_t block_number) {
        uint64_t region_number = block_number / this->pattern_len;
        // assert(this->pht_events.count(region_number) == 1);
        return this->pht_events[region_number];
    }

    void add_prefetch(uint64_t block_number) {
        Event ev = this->get_event(block_number);
        // assert(ev != MISS);
        this->prefetch_cnt[ev] += 1;
    }

    void add_useful(uint64_t block_number, Event ev) {
        // assert(ev != MISS);
        this->useful_cnt[ev] += 1;
    }

    void add_useless(uint64_t block_number, Event ev) {
        // assert(ev != MISS);
        this->useless_cnt[ev] += 1;
    }

    void reset_stats() {
        this->pht_access_cnt = 0;
        this->pht_pc_address_cnt = 0;
        this->pht_pc_offset_cnt = 0;
        this->pht_miss_cnt = 0;

        for (int i = 0; i < 2; i += 1) {
            this->prefetch_cnt[i] = 0;
            this->useful_cnt[i] = 0;
            this->useless_cnt[i] = 0;
        }

        this->pref_level_cnt.clear();
        this->region_pref_cnt = 0;

        this->voter_sum = 0;
        this->vote_cnt = 0;
    }

    void print_stats() {
        cout << "[Bingo] PHT Access: " << this->pht_access_cnt << endl;
        cout << "[Bingo] PHT Hit PC+Addr: " << this->pht_pc_address_cnt << endl;
        cout << "[Bingo] PHT Hit PC+Offs: " << this->pht_pc_offset_cnt << endl;
        cout << "[Bingo] PHT Miss: " << this->pht_miss_cnt << endl;

        cout << "[Bingo] Prefetch PC+Addr: " << this->prefetch_cnt[PC_ADDRESS] << endl;
        cout << "[Bingo] Prefetch PC+Offs: " << this->prefetch_cnt[PC_OFFSET] << endl;
        
        cout << "[Bingo] Useful PC+Addr: " << this->useful_cnt[PC_ADDRESS] << endl;
        cout << "[Bingo] Useful PC+Offs: " << this->useful_cnt[PC_OFFSET] << endl;
        
        cout << "[Bingo] Useless PC+Addr: " << this->useless_cnt[PC_ADDRESS] << endl;
        cout << "[Bingo] Useless PC+Offs: " << this->useless_cnt[PC_OFFSET] << endl;

        double l1_pref_per_region = 1.0 * this->pref_level_cnt[FILL_L1] / this->region_pref_cnt;
        double l2_pref_per_region = 1.0 * this->pref_level_cnt[FILL_L2] / this->region_pref_cnt;
        double l3_pref_per_region = 1.0 * this->pref_level_cnt[FILL_LLC] / this->region_pref_cnt;
        double no_pref_per_region = (double)this->pattern_len - (l1_pref_per_region +
                                                                 l2_pref_per_region +
                                                                 l3_pref_per_region);

        cout << "[Bingo] L1 Prefetch per Region: " << l1_pref_per_region << endl;
        cout << "[Bingo] L2 Prefetch per Region: " << l2_pref_per_region << endl;
        cout << "[Bingo] L3 Prefetch per Region: " << l3_pref_per_region << endl;
        cout << "[Bingo] No Prefetch per Region: " << no_pref_per_region << endl;

        double voter_mean = 1.0 * this->voter_sum / this->vote_cnt;
        double voter_sqr_mean = 1.0 * this->voter_sqr_sum / this->vote_cnt;
        double voter_sd = sqrt(voter_sqr_mean - square(voter_mean));
        cout << "[Bingo] Number of Voters Mean: " << voter_mean << endl;
        cout << "[Bingo] Number of Voters SD: " << voter_sd << endl;
    }

  private:
    /**
     * Performs a PHT lookup and computes a prefetching pattern from the result.
     * @return The appropriate prefetch level for all blocks based on PHT output or an empty vector
     *         if no blocks should be prefetched
     */
    vector<int> find_in_pht(uint64_t pc, uint64_t address) {
        if (this->debug_level >= 2) {
            cerr << "[Bingo] find_in_pht(pc=0x" << hex << pc << ", address=0x" << address << ")" << dec << endl;
        }
        vector<vector<bool>> matches = this->pht.find(pc, address);
        this->pht_access_cnt += 1;
        Event pht_last_event = this->pht.get_last_event();
        uint64_t region_number = address / this->pattern_len;
        if (pht_last_event != MISS)
            this->pht_events[region_number] = pht_last_event;
        vector<int> pattern;
        if (pht_last_event == PC_ADDRESS) {
            this->pht_pc_address_cnt += 1;
            // assert(matches.size() == 1); /* there can only be 1 PC+Address match */
            // assert(matches[0].size() == (unsigned)this->pattern_len);
            pattern.resize(this->pattern_len, 0);
            for (int i = 0; i < this->pattern_len; i += 1)
                if (matches[0][i])
                    pattern[i] = PC_ADDRESS_FILL_LEVEL;
        } else if (pht_last_event == PC_OFFSET) {
            this->pht_pc_offset_cnt += 1;
            pattern = this->vote(matches);
        } else if (pht_last_event == MISS) {
            this->pht_miss_cnt += 1;
        } else {
            /* error: unknown event! */
            // assert(0);
        }
        /* stats */
        if (pht_last_event != MISS) {
            this->region_pref_cnt += 1;
            for (int i = 0; i < (int)pattern.size(); i += 1)
                if (pattern[i] != 0)
                    this->pref_level_cnt[pattern[i]] += 1;
            // assert(this->pref_level_cnt.size() <= 3); /* L1, L2, L3 */
        }
        /* ===== */
        return pattern;
    }

    void insert_in_pht(const AccumulationTable::Entry &entry) {
        uint64_t pc = entry.data.pc;
        uint64_t region_number = hash_index(entry.key, this->accumulation_table.get_index_len());
        uint64_t address = region_number * this->pattern_len + entry.data.offset;
        if (this->debug_level >= 2) {
            cerr << "[Bingo] insert_in_pht(pc=0x" << hex << pc << ", address=0x" << address << ")" << dec << endl;
        }
        const vector<bool> &pattern = entry.data.pattern;
        this->pht.insert(pc, address, pattern);
    }

    /**
     * Uses a voting mechanism to produce a prefetching pattern from a set of footprints.
     * @param x The patterns obtained from all PC+Offset matches
     * @return  The appropriate prefetch level for all blocks based on BINGO's voting thresholds or
     *          an empty vector if no blocks should be prefetched
     */
    vector<int> vote(const vector<vector<bool>> &x) {
        if (this->debug_level >= 2)
            cerr << "Bingo::vote(...)" << endl;
        int n = x.size();
        if (n == 0) {
            if (this->debug_level >= 2)
                cerr << "[Bingo::vote] There are no voters." << endl;
            return vector<int>();
        }
        /* stats */
        this->vote_cnt += 1;
        this->voter_sum += n;
        this->voter_sqr_sum += square(n);
        /* ===== */
        if (this->debug_level >= 2) {
            cerr << "[Bingo::vote] Taking a vote among:" << endl;
            for (int i = 0; i < n; i += 1)
                cerr << "<" << setw(3) << i + 1 << "> " << pattern_to_string(x[i]) << endl;
        }
        bool pf_flag = false;
        vector<int> res(this->pattern_len, 0);
        for (int i = 0; i < n; i += 1)
            // assert((int)x[i].size() == this->pattern_len);
        for (int i = 0; i < this->pattern_len; i += 1) {
            int cnt = 0;
            for (int j = 0; j < n; j += 1)
                if (x[j][i])
                    cnt += 1;
            double p = 1.0 * cnt / n;
            if (p >= L1D_THRESH)
                res[i] = FILL_L1;
            else if (p >= L2C_THRESH)
                res[i] = FILL_L2;
            else if (p >= LLC_THRESH)
                res[i] = FILL_LLC;
            else
                res[i] = 0;
            if (res[i] != 0)
                pf_flag = true;
        }
        if (this->debug_level >= 2) {
            cerr << "<res> " << pattern_to_string(res) << endl;
        }
        if (!pf_flag)
            return vector<int>();
        return res;
    }

    /*=== Bingo Settings ===*/
    /* voting thresholds */
    const double L1D_THRESH = 0.75;
    const double L2C_THRESH = 0.25;
    const double LLC_THRESH = 0.25; /* off */
    
    /* PC+Address matches are filled into L1 */
    const int PC_ADDRESS_FILL_LEVEL = FILL_L1;
    /*======================*/

    int pattern_len;
    FilterTable filter_table;
    AccumulationTable accumulation_table;
    PatternHistoryTable pht;
    PrefetchStreamer pf_streamer;
    int debug_level = 0;

    /* stats */
    unordered_map<uint64_t, Event> pht_events;

    uint64_t pht_access_cnt = 0;
    uint64_t pht_pc_address_cnt = 0;
    uint64_t pht_pc_offset_cnt = 0;
    uint64_t pht_miss_cnt = 0;

    uint64_t prefetch_cnt[2] = {0};
    uint64_t useful_cnt[2] = {0};
    uint64_t useless_cnt[2] = {0};

    unordered_map<int, uint64_t> pref_level_cnt;
    uint64_t region_pref_cnt = 0;

    uint64_t vote_cnt = 0;
    uint64_t voter_sum = 0;
    uint64_t voter_sqr_sum = 0;
};

/**
 * The global debug level. Higher values will print more information.
 * NOTE: The size of the output file can become very large (~GBs).
 */
const int DEBUG_LEVEL = 0;

vector<Bingo> prefetchers;
}

void CACHE::l1d_prefetcher_initialize() {
    /*=== Bingo Settings ===*/
    const int REGION_SIZE = 2 * 1024;  /* size of spatial region = 2KB */
    const int PC_WIDTH = 16;           /* number of PC bits used in PHT */
    const int MIN_ADDR_WIDTH = 5;      /* number of Address bits used for PC+Offset matching */
    const int MAX_ADDR_WIDTH = 16;     /* number of Address bits used for PC+Address matching */
    const int FT_SIZE = 64;            /* size of filter table */
    const int AT_SIZE = 128;           /* size of accumulation table */
    const int PHT_SIZE = 8 * 1024;     /* size of pattern history table (PHT) */
    const int PHT_WAYS = 16;           /* associativity of PHT */
    const int PF_STREAMER_SIZE = 128;  /* size of prefetch streamer */
    /*======================*/

    /* number of PHT sets must be a power of 2 */
    // assert(__builtin_popcount(PHT_SIZE / PHT_WAYS) == 1);

    /* construct prefetcher for all cores */
    // assert(PAGE_SIZE % REGION_SIZE == 0);
    L1D_PREF::prefetchers = vector<L1D_PREF::Bingo>(
        NUM_CPUS, L1D_PREF::Bingo(REGION_SIZE >> LOG2_BLOCK_SIZE, MIN_ADDR_WIDTH, MAX_ADDR_WIDTH, PC_WIDTH, FT_SIZE,
                      AT_SIZE, PHT_SIZE, PHT_WAYS, PF_STREAMER_SIZE, L1D_PREF::DEBUG_LEVEL));
}

void CACHE::l1d_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type) {
    if (L1D_PREF::DEBUG_LEVEL >= 2) {
        cerr << "CACHE::l1d_prefetcher_operate(addr=0x" << hex << addr << ", ip=0x" << ip << ", cache_hit=" << dec
             << (int)cache_hit << ", type=" << (int)type << ")" << dec << endl;
        cerr << "[CACHE::l1d_prefetcher_operate] CACHE{core=" << this->cpu << ", NAME=" << this->NAME << "}" << dec
             << endl;
    }

    if (type != LOAD)
        return;

    uint64_t block_number = addr >> LOG2_BLOCK_SIZE;

    /* update BINGO with most recent LOAD access */
    L1D_PREF::prefetchers[cpu].access(block_number, ip);

    /* issue prefetches */
    L1D_PREF::prefetchers[cpu].prefetch(this, block_number);

    if (L1D_PREF::DEBUG_LEVEL >= 3) {
        L1D_PREF::prefetchers[cpu].log();
        cerr << "=======================================" << dec << endl;
    }
}

void CACHE::l1d_prefetcher_cache_fill(
    uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in) {
    uint64_t evicted_block_number = evicted_addr >> LOG2_BLOCK_SIZE;

    if (this->block[set][way].valid == 0)
        return; /* no eviction */

    /* inform all sms modules of the eviction */
    for (int i = 0; i < NUM_CPUS; i += 1)
        L1D_PREF::prefetchers[i].eviction(evicted_block_number);
}

void CACHE::l1d_prefetcher_final_stats() {}
