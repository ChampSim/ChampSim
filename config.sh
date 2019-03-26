#!/bin/bash

config_mk="configure.mk"

usage() {
    echo "config.sh [-h] [-v]"
    echo "          [-1 l1_prefetcher] [-2 l2_prefetcher] [-3 ll_prefetcher]"
    echo "          [-r llc_replacement] [-b branch_predictor] [-n num_cores]"
    echo "          [-o executable_name]"
    echo "          [config_file]"
    echo ""
    echo "Configures ChampSim before building."
    echo ""
    echo "where:"
    echo "    -h   show this help text"
    echo "    -v   configure with debugging output"
    echo "    -1   use the given L1 prefetcher"
    echo "    -2   use the given L2 prefetcher"
    echo "    -3   use the given LL prefetcher"
    echo "    -r   use the given LLC replacement policy"
    echo "    -b   use the given branch predictor"
    echo "    -n   build with the given number of cores"
    echo "    -o   the executable to be created"
}

# GETOPTS
while getopts "1:2:3:b:r:n:o:vh" opt; do
    case $opt in
        1)
            l1prefetcher="$OPTARG"
            echo "(override) Using L1 prefetcher at $OPTARG"
            ;;
        2)
            l2prefetcher="$OPTARG"
            echo "(override) Using L2 prefetcher at $OPTARG"
            ;;
        3)
            llprefetcher="$OPTARG"
            echo "(override) Using LL prefetcher at $OPTARG"
            ;;
        b)
            branch_predictor="$OPTARG"
            echo "(override) Using branch predictor at $OPTARG"
            ;;
        n)
            # Check num_core
            if ! [ $OPTARG -gt 0 ] ; then
                echo "[ERROR]: num_core is NOT a number" >&2;
                exit 1
            else
                num_cores=$OPTARG
                echo "(override) Building for $OPTARG cores"
            fi
            ;;
        o)
            executable_name="$OPTARG"
            echo "(override) Creating executable $OPTARG"
            ;;
        r)
            llreplacement="$OPTARG"
            echo "(override) Using LLC replacement at $OPTARG"
            ;;
        v)
            CFLAGS="$CFLAGS -g"
            CXXFLAGS="$CXXFLAGS -g"
            CPPFLAGS="$CPPFLAGS -DDEBUG_PRINT"
            ;;
        h)
            # print help
            usage
            exit
            ;;
        :)
            echo "Missing option argument for -$OPTARG" >&2
            usage
            exit 1
            ;;
        \?)
            echo "Unrecognized Argument: -$OPTARG" >&2
            usage
            exit 1
            ;;
    esac
done

# Now, read config file
shift $(($OPTIND - 1))
file="$1"

if [ -f "$file" ]; then
    # Read properties file
    while read line; do
        # Ignore comments that start with (whitespace, then) "#"
        [[ "$line" =~ ^[[:space:]]*# ]] && continue

        # Extract the key and the value from the config line
        key=$(echo "$line" | cut -d"=" -f1)
        value=$(echo "$line" | cut -d"=" -f2)

        case "$key" in
            "l1prefetcher")
                if [ -z ${l1prefetcher+x} ]; then
                    l1prefetcher="$value"
                    echo "(config) Using L1 prefetcher at $value"
                fi
                ;;
            "l2prefetcher")
                if [ -z ${l2prefetcher+x} ]; then
                    l2prefetcher="$value"
                    echo "(config) Using L2 prefetcher at $value"
                fi
                ;;
            "llprefetcher")
                if [ -z ${llprefetcher+x} ]; then
                    llprefetcher="$value"
                    echo "(config) Using LL prefetcher at $value"
                fi
                ;;
            "llreplacement")
                if [ -z ${llreplacement+x} ]; then
                    llreplacement="$value"
                    echo "(config) Using LLC replacement at $value"
                fi
                ;;
            "branch_predictor")
                if [ -z ${branch_predictor+x} ]; then
                    branch_predictor="$value"
                    echo "(config) Using branch predictor at $value"
                fi
                ;;
            "num_cores")
                if [ -z ${num_cores+x} ]; then
                    if ! [ $value -gt 0 ] ; then
                        echo "[ERROR]: num_cores is NOT a number" >&2;
                        exit 1
                    else
                        num_cores="$value"
                        echo "(config) Using $value cores"
                    fi
                fi
                ;;

            "executable_name")
                if [ -z ${executable_name+x} ]; then
                    executable_name="$value"
                    echo "(config) Creating executable $value"
                fi
                ;;

            # passing to compiler
            "CC")
                CC="$value"
                echo "Using C compiler $value"
                ;;
            "CXX")
                CXX="$value"
                echo "Using C++ compiler $value"
                ;;
            "CFLAGS")
                CFLAGS="$value"
                echo "Found C compiler flags $value"
                ;;
            "CXXFLAGS")
                CXXFLAGS="$value"
                echo "Found C++ compiler flags $value"
                ;;
            "CPPFLAGS")
                CPPFLAGS="$value"
                echo "Found preprocessor flags $value"
                ;;
            "LDFLAGS")
                LDFLAGS="$value"
                echo "Found linker flags $value"
                ;;
            "LDLIBS")
                LDLIBS="$value"
                echo "Found linker libs $value"
                ;;
            # ...more?

            "")
                #skip empty line
                ;;
            *)
                echo "(config) Unrecognized option $key"
            ;;
        esac
    done < "$file"
fi

if [ -z ${l1prefetcher+x} ]; then
    l1prefetcher="prefetcher/no.l1d_pref"
    echo "No L1 prefetcher found. Defaulting to no prefetcher."
fi

if [ -z ${l2prefetcher+x} ]; then
    l2prefetcher="prefetcher/no.l2c_pref"
    echo "No L2 prefetcher found. Defaulting to no prefetcher."
fi

if [ -z ${llprefetcher+x} ]; then
    llprefetcher="prefetcher/no.llc_pref"
    echo "No LL prefetcher found. Defaulting to no prefetcher."
fi

if [ -z ${llreplacement+x} ]; then
    llreplacement="replacement/lru.llc_repl"
    echo "No LLC replacement policy found. Defaulting to LRU."
fi

if [ -z ${branch_predictor+x} ]; then
    branch_predictor="branch/bimodal.bpred"
    echo "No branch predictor found. Defaulting to bimodal predictor."
fi

if [ -z ${num_cores+x} ]; then
    num_cores=1
    echo "Number of cores not specified. Defaulting to 1."
fi

# Check for multi-core
if [ "$num_cores" -gt 1 ]; then
    echo "Building multi-core ChampSim..."
    CPPFLAGS="$CPPFLAGS -DNUM_CPUS=$num_cores -DDRAM_CHANNELS=2 -DLOG2_DRAM_CHANNELS=1"
else
    echo "Building single-core ChampSim..."
    CPPFLAGS="$CPPFLAGS -DNUM_CPUS=1 -DDRAM_CHANNELS=1 -DLOG2_DRAM_CHANNELS=0"
fi

#Write to $config_mk
#Makefile then includes that file, and compiles accordingly.
echo "L1PREFETCHER=$l1prefetcher" >  $config_mk
echo "L2PREFETCHER=$l2prefetcher" >> $config_mk
echo "LLPREFETCHER=$llprefetcher" >> $config_mk
echo "LLREPLACEMENT=$llreplacement" >> $config_mk
echo "BRANCH_PREDICTOR=$branch_predictor" >> $config_mk
if [ -n "$executable_name" ]; then
    echo "executable_name=$executable_name" >> $config_mk
fi
if [ -n "$CC" ]; then
    echo "CC=$CC" >> $config_mk
fi
if [ -n "$CXX" ]; then
    echo "CXX=$CXX" >> $config_mk
fi
if [ -n "$CFLAGS" ]; then
    echo "CFLAGS=$CFLAGS" >> $config_mk
fi
if [ -n "$CXXFLAGS" ]; then
    echo "CXXFLAGS=$CXXFLAGS" >> $config_mk
fi
if [ -n "$CPPFLAGS" ]; then
    echo "CPPFLAGS=$CPPFLAGS" >> $config_mk
fi
if [ -n "$LDFLAGS" ]; then
    echo "LDFLAGS=$LDFLAGS" >> $config_mk
fi
if [ -n "$LDLIBS" ]; then
    echo "LDLIBS=$LDLIBS" >> $config_mk
fi

echo "Wrote configuration out to $config_mk"
echo "Run 'make' to build ChampSim"

