/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <cstdint>
#include <map>

// defines for the paths for the various decompression programs and Apple/Linux differences

#ifdef __APPLE__
#define XZ_PATH		"/opt/local/bin/xz"
#define GZIP_PATH	"/usr/bin/gzip"
#define CAT_PATH	"/bin/cat"
#define UINT64		uint64_t
#else
#define XZ_PATH		"/usr/bin/xz"
#define GZIP_PATH	"/bin/gzip"
#define CAT_PATH	"/bin/cat"
#define UINT64		unsigned long long int
#endif

using namespace std;

bool verbose = false;

// ChampSim trace format
#define NUM_INSTR_DESTINATIONS 2
#define NUM_INSTR_SOURCES 4

struct trace_instr_format {
    UINT64 ip;  // instruction pointer (program counter) value

    unsigned char is_branch;    // is this branch
    unsigned char branch_taken; // if so, is this taken

    unsigned char destination_registers[NUM_INSTR_DESTINATIONS]; // output registers
    unsigned char source_registers[NUM_INSTR_SOURCES];     // input registers

    UINT64 destination_memory[NUM_INSTR_DESTINATIONS]; // output memory
    UINT64 source_memory[NUM_INSTR_SOURCES];       // input memory
};


// orginal instruction types from CVP-1 traces

typedef enum { 
	aluInstClass = 0,
	loadInstClass = 1,
	storeInstClass = 2,
	condBranchInstClass = 3,
	uncondDirectBranchInstClass = 4,
	uncondIndirectBranchInstClass = 5,
	fpInstClass = 6,
	slowAluInstClass = 7,
	undefInstClass = 8
} InstClass;

// branch types from CBP5

typedef enum {
	OPTYPE_OP               =2,
	OPTYPE_RET_UNCOND,
	OPTYPE_JMP_DIRECT_UNCOND,
	OPTYPE_JMP_INDIRECT_UNCOND,
	OPTYPE_CALL_DIRECT_UNCOND,
	OPTYPE_CALL_INDIRECT_UNCOND,
	OPTYPE_RET_COND,
	OPTYPE_JMP_DIRECT_COND,
	OPTYPE_JMP_INDIRECT_COND,
	OPTYPE_CALL_DIRECT_COND,
	OPTYPE_CALL_INDIRECT_COND,
	OPTYPE_ERROR,
	OPTYPE_MAX
} OpType;

const char *branch_names[] = {
"none",
"none",
"OPTYPE_OP",
"OPTYPE_RET_UNCOND",
"OPTYPE_JMP_DIRECT_UNCOND",
"OPTYPE_JMP_INDIRECT_UNCOND",
"OPTYPE_CALL_DIRECT_UNCOND",
"OPTYPE_CALL_INDIRECT_UNCOND",
"OPTYPE_RET_COND",
"OPTYPE_JMP_DIRECT_COND",
"OPTYPE_JMP_INDIRECT_COND",
"OPTYPE_CALL_DIRECT_COND",
"OPTYPE_CALL_INDIRECT_COND",
"OPTYPE_ERROR",
"OPTYPE_MAX",
};

long long int counts[OPTYPE_MAX];

// one record from the CVP-1 trace file format

struct trace {
	UINT64	PC,	// program counter
			EA, 	// effective address
			target;	// branch target

	uint8_t		access_size, 
			taken, 	// branch was taken
			num_input_regs, 
			num_output_regs, 
			input_reg_names[256], 
			output_reg_names[256];

	// output register values could be up to 128 bits each
	UINT64	output_reg_values[256][2]; 

	InstClass	type; // instruction type

	// read a single record from the trace file, return true on success, false on EOF

	bool read (FILE *f) {

		// initialize

		PC = 0;
		EA = 0;
		target = 0;
		access_size = 0;
		taken = 0;
		type = undefInstClass;

		// get the PC

		int n;
		n = fread (&PC, 8, 1, f);
		if (!n) return false;
		if (feof (f)) return false;
		assert (n == 1);

		// get the instruction type

		assert (fread (&type, 1, 1, f) == 1);

		// base on the type, read in different stuff

		switch (type) {
		case loadInstClass:
		case storeInstClass:
			// load or store? get the effective address and access size

			assert (fread (&EA, 8, 1, f) == 1);
			assert (fread (&access_size, 1, 1, f) == 1);
			break;
		case condBranchInstClass:
		case uncondDirectBranchInstClass:
		case uncondIndirectBranchInstClass:

			// branch? get "taken" and the target

			assert (fread (&taken, 1, 1, f) == 1);
			if (taken) {
				assert (fread (&target, 8, 1, f) == 1);
			} else {
				// if not taken, default target is fallthru, i.e. PC+4
				target = PC + 4;

				// this had better not be an unconditional branch (frickin ARM with its predicated branches)

				assert (type != uncondDirectBranchInstClass);
				assert (type != uncondIndirectBranchInstClass);
			}
			break;
		default: ;
		}

		// get the number of input registers and their names

		assert (fread (&num_input_regs, 1, 1, f) == 1);
		for (int i=0; i<num_input_regs; i++) {
			assert (fread (&input_reg_names[i], 1, 1, f) == 1);
		}

		// get the number of output registers and their names

		assert (fread (&num_output_regs, 1, 1, f) == 1);
		for (int i=0; i<num_output_regs; i++) {
			assert (fread (&output_reg_names[i], 1, 1, f) == 1);
		}

		// read the output registers

		memset (output_reg_values, 0, sizeof (output_reg_values));
		for (int i=0; i<num_output_regs; i++) {
			if (output_reg_names[i] <= 31 || output_reg_names[i] == 64) {
				// scalars or flags?
				assert (fread (&output_reg_values[i][0], 8, 1, f) == 1);
			} else if (output_reg_names[i] >= 32 && output_reg_names[i] < 64) {
				// SIMD values?
				assert (fread (&output_reg_values[i][0], 16, 1, f) == 1);
			} else 
				assert (0);
		}

		// success!

		return true;
	}
};

// is this a branch type?

bool is_branch (InstClass t) {
	return (t == uncondIndirectBranchInstClass 
		|| t == uncondDirectBranchInstClass 
		|| t == condBranchInstClass);
}

map<UINT64,bool> code_pages, data_pages;
map<UINT64,UINT64> remapped_pages;
UINT64 bump_page = 0x1000;

// this string will contain the trace file name, or "-" if we want to read from standard input

char tracefilename[1000];

#define REG_SP		6
#define REG_IP		26
#define REG_FLAGS	25
#define REG_AX		56

FILE *open_trace_file (void) {
	FILE *f = NULL;

	// read from standard input?

	if (!strcmp (tracefilename, "-")) {
		fprintf (stderr, "reading from standard input\n");
		fflush (stderr);
		f = stdin;
	} else {

		// see what kind of file this is by reading the magic number

		f = fopen (tracefilename, "r");
		if (!f) {
			perror (tracefilename);
			exit (1);
		}

		// read six bytes from the beginning of the file

		unsigned char s[6];
		int n = fread (s, 1, 6, f);
		fclose (f);
		assert (n == 6);

		// is this the magic number for XZ compression?

		if (s[0] == 0xfd && s[1] == '7' && s[2] == 'z' && s[3] == 'X' && s[4] == 'Z' && s[5] == 0) {

			// it is an XZ file or doing a good impression of one

			char cmd[1000];
			fprintf (stderr, "opening xz file \"%s\"\n", tracefilename);
			fflush (stderr);

			// start up an xz decompression and open a pipe to our standard input

			sprintf (cmd, "%s -dc %s", XZ_PATH, tracefilename);
			f = popen (cmd, "r");
			if (!f) {
				perror (cmd);
				return NULL;
			}
		} 
		// check for the magic number for GZIP compression

		else if (s[0] == 0x1f && s[1] == 0x8b) {
			// it is a GZ file
			char cmd[1000];
			fprintf (stderr, "opening gz file \"%s\"\n", tracefilename);
			fflush (stderr);

			// open a pipe to a gzip decompression process

			sprintf (cmd, "%s -dc %s", GZIP_PATH, tracefilename);
			f = popen (cmd, "r");
			if (!f) {
				perror (cmd);
				return NULL;
			}
		} else {
			// no magic number? maybe it's uncompressed?
			char cmd[1000];
			fprintf (stderr, "opening file \"%s\"\n", tracefilename);
			fflush (stderr);

			// use Unix cat to open and read this file. we could just fopen the file
			// but then we're pclosing it later so that could get weird

			sprintf (cmd, "%s %s", CAT_PATH, tracefilename);
			f = popen (cmd, "r");
			if (!f) {
				perror (cmd);
				return NULL;
			}
		}
	}
	return f;
}

void preprocess_file (void) {
	trace t;
	fprintf (stderr, "preprocessing to find code and data pages...\n");
	fflush (stderr);
	FILE *f = open_trace_file ();
	if (!f) return;
	int count = 0;
	for (;;) {
		bool good = t.read (f);
		if (!good) break;
		code_pages[t.PC>>12] = true;
		if (t.type == loadInstClass || t.type == storeInstClass)
			data_pages[t.EA>>12] = true;
		count++;
		if (count % 10000000 == 0) {
			fprintf (stderr, "."); 
			fflush (stderr);
			if (count % 600000000 == 0) {
				fprintf (stderr, "\n"); 
				fflush (stderr);
			}
		}
	}
	pclose (f);
	fprintf (stderr, "%ld code pages, %ld data pages\n", code_pages.size(), data_pages.size());
	fflush (stderr);
}

// take an address representing data and make sure it doesn't overlap with code

UINT64 transform (UINT64 a) {
	static int num_allocs = 0;
	UINT64 page = a >> 12;
	UINT64 new_page = page;
	if (code_pages.find (page) != code_pages.end()) {
		new_page = remapped_pages[page];
		if (new_page == 0) {
			num_allocs++;
			fprintf (stderr, "[%d]", num_allocs); fflush (stderr);
			// allocate a new page
			new_page = bump_page;
			for (;;) {
				if (code_pages.find (new_page) != code_pages.end() || data_pages.find (new_page) != data_pages.end())
					new_page++;
				else
					break;
			}
			bump_page = new_page + 1;
			remapped_pages[page] = new_page;
		}
	}
	a = new_page << 12 | (a & 0xfff);
	return a;
}

int main (int argc, char **argv) {
	trace t;

	// for fun we will keep a register file up to date

	UINT64 registers[256][2];
	memset (registers, 0, sizeof (registers));

	// defaults to reading from standard input

	strcpy (tracefilename, "-");

	for (int i=1; i<argc; i++) {
		if (!strcmp (argv[i], "-v")) verbose = true; else strcpy (tracefilename, argv[i]);
	}

	preprocess_file ();

	// open the trace file

	FILE *f = open_trace_file ();

	if (!f) return 1;

	// number of records read so far
	long long int n = 0;
	trace oldt;
	oldt.PC = 0;

	// loop getting records until we're done

	for (;;) {

		// one more record

		n++;

		// print something to entertain the user while they wait

		if (n % 1000000 == 0) {
			fprintf (stderr, "%lld instructions\n", n);
			fflush (stderr);
		}

		// read a record from the trace file

		bool good = t.read (f);
		if (t.PC == oldt.PC) {
			fprintf (stderr, "hmm, that's weird\n");
		}

		oldt = t;

		// are we done? then stop.

		if (!good) break;
		trace_instr_format ct;
		ct.ip = t.PC;
		ct.is_branch = false;
		// we are going to figure out the op type

		OpType c = OPTYPE_OP;

		// if this is a branch then do more stuff; we don't care about non-branches

		if (is_branch (t.type)) {
			ct.is_branch = true;

			// if this is a conditional branch then it's direct and we're done figuring out the type

			if (t.type == condBranchInstClass) {
				c = OPTYPE_JMP_DIRECT_COND;
			} else {

				// this is some other kind of branch. it should have a non-zero target

				assert (t.target);

				// on ARM, calls link the return address in register X30. let's see if this
				// instruction is doing that; if so, it's a call or wants us to believe it is

				if (t.num_output_regs == 1 && t.output_reg_names[0] == 30) {

					// is it indirect?

					if (t.type == uncondIndirectBranchInstClass)
						c = OPTYPE_CALL_INDIRECT_UNCOND;
					else
						c = OPTYPE_CALL_DIRECT_UNCOND;
				} else {
					// no X30? then it's just an unconditional jump
					// is it indirect?

					if (t.type == uncondIndirectBranchInstClass) 
						c = OPTYPE_JMP_INDIRECT_UNCOND;
					else
						c = OPTYPE_JMP_DIRECT_UNCOND;
				}

				// on ARM, returns are an indirect jump to X30. let's see if we're doing this

				if (t.num_input_regs == 1) if (t.input_reg_names[0] == 30) {

					// yes. it's a return.

					c = OPTYPE_RET_UNCOND;
				}
			}
			counts[c]++;

			// OK now make a branch instruction out of this bad boy

			memset (ct.destination_registers, 0, sizeof (ct.destination_registers));
			memset (ct.source_registers, 0, sizeof (ct.source_registers));
			memset (ct.destination_memory, 0, sizeof (ct.destination_memory));
			memset (ct.source_memory, 0, sizeof (ct.source_memory));
			switch (c) {
			case OPTYPE_JMP_DIRECT_UNCOND:
				// writes IP only
				ct.destination_registers[0] = REG_IP;
				ct.branch_taken = t.taken;
				break;
			case OPTYPE_JMP_DIRECT_COND:
				ct.branch_taken = t.taken;
				// reads FLAGS, writes IP
				ct.destination_registers[0] = REG_IP;
				// turns out pin records conditional direct branches as also reading IP. whatever.
				ct.source_registers[0] = REG_IP;
				ct.source_registers[1] = REG_FLAGS;
				break;
			case OPTYPE_CALL_INDIRECT_UNCOND:
				ct.branch_taken = true;
				// reads something else, reads IP, reads SP, writes SP, writes IP
				ct.destination_registers[0] = REG_IP;
				ct.destination_registers[1] = REG_SP;
				ct.source_registers[0] = REG_IP;
				ct.source_registers[1] = REG_SP;
				ct.source_registers[2] = REG_AX;
				break;
			case OPTYPE_CALL_DIRECT_UNCOND:
				ct.branch_taken = true;
				// reads IP, reads SP, writes SP, writes IP
				ct.destination_registers[0] = REG_IP;
				ct.destination_registers[1] = REG_SP;
				ct.source_registers[0] = REG_IP;
				ct.source_registers[1] = REG_SP;
				break;
			case OPTYPE_JMP_INDIRECT_UNCOND:
				ct.branch_taken = true;
				// reads something else, writes IP
				ct.destination_registers[0] = REG_IP;
				ct.source_registers[0] = REG_AX;
				break;
			case OPTYPE_RET_UNCOND:
				ct.branch_taken = true;
				// reads SP, writes SP, writes IP
				ct.source_registers[0] = REG_SP;
				ct.destination_registers[0] = REG_IP;
				ct.destination_registers[1] = REG_SP;
				break;
			default: assert (0);
			}
			fwrite (&ct, sizeof (ct), 1, stdout); // write a branch trace
		} else {
			memset (ct.destination_registers, 0, sizeof (ct.destination_registers));
			memset (ct.source_registers, 0, sizeof (ct.source_registers));
			memset (ct.destination_memory, 0, sizeof (ct.destination_memory));
			memset (ct.source_memory, 0, sizeof (ct.source_memory));
			counts[OPTYPE_OP]++;
			if (t.num_input_regs > NUM_INSTR_SOURCES) t.num_input_regs = NUM_INSTR_SOURCES;
			if (t.num_output_regs == 0) {
				t.num_output_regs = 1;
				t.output_reg_names[0] = 0;
			}
			//for (int a=0; a<t.num_output_regs; a++) {
			for (int a=0; a<1; a++) {
				int x = t.output_reg_names[a];
				if (x == REG_IP) x = 64;
				if (x == REG_SP) x = 65;
				if (x == REG_FLAGS) x = 66;
				if (x == 0) x = 67;
				ct.destination_registers[a] = x;
				for (int i=0; i<t.num_input_regs; i++) {
					int x = t.input_reg_names[i];
					if (x == REG_IP) x = 64;
					if (x == REG_SP) x = 65;
					if (x == REG_FLAGS) x = 66;
					if (x == 0) x = 67;
					ct.source_registers[i] = x;
				}
				switch (t.type) {
				case loadInstClass:
					ct.source_memory[0] = transform (t.EA);
					break;
				case storeInstClass:
					ct.destination_memory[0] = transform (t.EA);
					break;
				case aluInstClass:
				case fpInstClass:
				case slowAluInstClass:
					break;
				case uncondDirectBranchInstClass: 
				case condBranchInstClass:
				case uncondIndirectBranchInstClass:
				case undefInstClass: 
					assert (0);
				}
				fwrite (&ct, sizeof (ct), 1, stdout); // write a non-branch trace
			}
		}
		
		// for fun, update the register values

		for (int i=0; i<t.num_output_regs; i++) {
			int x = t.output_reg_names[i];
			registers[x][0] = t.output_reg_values[x][0];
			registers[x][1] = t.output_reg_values[x][1];
		}
		if (verbose) {
			static long long int n = 0;
			fprintf (stderr, "%lld %llx ", ++n, t.PC);
			if (c == OPTYPE_OP) {
				switch (t.type) {
				case loadInstClass:
					fprintf (stderr, "LOAD (0x%llx)", t.EA);
                                        break;
                                case storeInstClass:
					fprintf (stderr, "STORE (0x%llx)", t.EA);
                                        break;
                                case aluInstClass:
					fprintf (stderr, "ALU");
                                        break;
                                case fpInstClass:
					fprintf (stderr, "FP");
                                        break;
                                case slowAluInstClass:
					fprintf (stderr, "SLOWALU");
                                        break;
				}
				for (int i=0; i<t.num_input_regs; i++) fprintf (stderr, " I%d", t.input_reg_names[i]);
				for (int i=0; i<t.num_output_regs; i++) fprintf (stderr, " O%d", t.output_reg_names[i]);
			} else {
				fprintf (stderr, "%s %llx", branch_names[c], t.target);
			}
			fprintf (stderr, "\n");
		}
	}
	fprintf (stderr, "converted %lld instructions\n", n);
	OpType lim = OPTYPE_MAX;
	for (int i=2; i<(int)lim; i++) {
		if (counts[i])
			fprintf (stderr, "%s %lld %f%%\n", branch_names[i], counts[i], 100 * counts[i] / (double) n);
	}

	// close the pipe, if any

	if (f != stdin) pclose (f);
	return 0;
}
