override ROOT_DIR = $(patsubst %/,%,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))

# Customization points:
#  - OBJ_ROOT: at make-time, override the object file directory
#  - DEP_ROOT: at make-time, override the dependency file directory
OBJ_ROOT = $(ROOT_DIR)/.csconfig
DEP_ROOT = $(ROOT_DIR)/.csconfig/dep
$(shell mkdir -p $(OBJ_ROOT) $(DEP_ROOT))

# vcpkg integration
TRIPLET_DIR = $(patsubst %/,%,$(firstword $(filter-out $(ROOT_DIR)/vcpkg_installed/vcpkg/, $(wildcard $(ROOT_DIR)/vcpkg_installed/*/))))
override CPPFLAGS += -I$(ROOT_DIR)/inc -isystem $(TRIPLET_DIR)/include
override LDFLAGS  += -L$(TRIPLET_DIR)/lib -L$(TRIPLET_DIR)/lib/manual-link
override LDLIBS   += -llzma -lz -lbz2 -lfmt

.PHONY: all clean configclean test

test_main_name=$(ROOT_DIR)/test/bin/000-test-main
executable_name:=
dirs:=

# Migrate names from a source directory (and suffix) to a target directory (and suffix)
# $1 - source directory
# $2 - target directory
# $3 - source suffix
# $4 - target suffix
migrate = $(patsubst $1/%$3,$2/%$4,$(wildcard $1/*$3))

# Remove the first word from the sequence
# $1 - a sequence of words
tail = $(wordlist 2,$(words $1),$1)

# Reverse the order of the inputs
# $1 - a sequence of words
reverse = $(if $1,$(call $0,$(call tail,$1)) $(firstword $1))

# Return a nonempty string if the sequence starts with the given word
# $1 - The word to find
# $2 - The sequence to find in
startswith = $(findstring $1,$(firstword $2))

# Return a sequence with (copies of) the given word removed
# $1 - The word to remove
# $2 - The sequence to drop from
dropwhile = $(if $(call startswith,$1,$2),$(call $0,$1,$(call tail,$2)),$2)

# $1 - source dir
# $2 - hash
# $3 - executable name
# $4 - (optional) additional options files
define make_part
$(subst /,D,$2)_objs = $$(call migrate,$1,$$(OBJ_ROOT)/$2,.cc,.o)
$$($(subst /,D,$2)_objs): $$(OBJ_ROOT)/$2/%.o: $1/%.cc
$$($(subst /,D,$2)_objs): $$(ROOT_DIR)/global.options $4
$$($(subst /,D,$2)_objs): CPPFLAGS += -I$$(abspath $$(OBJ_ROOT)/$2/..) -I$1
$$($(subst /,D,$2)_objs): depdir = $$(DEP_ROOT)/$2

$(subst /,D,$2)_deps = $$(call migrate,$1,$$(DEP_ROOT)/$2,.cc,.d)
$$($(subst /,D,$2)_deps): $$(DEP_ROOT)/$2/%.d: $1/%.cc

ifeq (__legacy__,$$(findstring __legacy__,$$(wildcard $1/*)))
$$($(subst /,D,$2)_objs): $1/legacy.options
$$($(subst /,D,$2)_deps): | $$(abspath $$(OBJ_ROOT)/$2/../)/cache_module_decl.inc $$(abspath $$(OBJ_ROOT)/$2/../)/core_module_decl.inc $$(abspath $$(OBJ_ROOT)/$2/../)/module_def.inc

$$(abspath $$(OBJ_ROOT)/$2/../)/cache_module_decl.inc: | $1/__legacy__
$$(abspath $$(OBJ_ROOT)/$2/../)/core_module_decl.inc: | $1/__legacy__
$$(abspath $$(OBJ_ROOT)/$2/../)/module_def.inc: | $1/__legacy__
endif

ifeq (,$$(filter clean configclean, $$(MAKECMDGOALS)))
-include $$($(subst /,D,$2)_deps)
endif

dirs += $$(OBJ_ROOT)/$2 $$(DEP_ROOT)/$2
objs += $$($(subst /,D,$2)_objs)

$(call make_all_parts,$(sort $(dir $(wildcard $1/*/))),$2x,$3,$4)
$3: $$($(subst /,D,$2)_objs)


endef

# $1 - a sequence of source directories
# $2 - hash
# $3 - executable name
# $4 - (optional) additional options files
make_all_parts =$(if $1,\
$(call make_part,$(firstword $(call dropwhile,MODULE,$1)),$2,$3,$4$(if $(call startswith,MODULE,$1), $(ROOT_DIR)/module.options))\
$(call $0,$(wordlist 2,$(words $1),$(call dropwhile,MODULE,$1)),$2_,$3,$4)\
)

# Requires:
# $(source_root)
# $(exe)
# $(opts)
$(DEP_ROOT)/%.mkpart: $(source_roots)
	mkdir -p $(@D) $(OBJ_ROOT)/$*/obj
	$(file >$@,$(call make_all_parts,$(source_roots),$*/obj,$(exe),$(opts)))

# Remove all intermediate files
clean:
	@-find src test .csconfig branch btb prefetcher replacement $(dirs) \( -name '*.o' -o -name '*.d' \) -delete &> /dev/null
	@-find .csconfig \( -name cache_module_decl.inc -o -name ooo_cpu_module_decl.inc -o -name module_def.inc \) -delete &> /dev/null
	@-$(RM) inc/champsim_constants.h
	@-$(RM) inc/cache_modules.h
	@-$(RM) inc/ooo_cpu_modules.h
	@-$(RM) src/core_inst.cc
	@-$(RM) $(test_main_name)

# Remove all configuration files
configclean: clean
	@-$(RM) -r $(dirs) _configuration.mk

# Link test executable
$(test_main_name): CPPFLAGS += -DCHAMPSIM_TEST_BUILD -I$(ROOT_DIR)/test/cpp/src
$(test_main_name): CXXFLAGS += -g3 -Og -Wconversion
$(test_main_name): LDLIBS += -lCatch2Main -lCatch2

$(DEP_ROOT)/test_src.mkpart: source_roots = $(ROOT_DIR)/test/cpp/src $(ROOT_DIR)/src MODULE $(ROOT_DIR)/btb MODULE $(ROOT_DIR)/branch MODULE $(ROOT_DIR)/prefetcher MODULE $(ROOT_DIR)/replacement
$(DEP_ROOT)/test_src.mkpart: exe = $(test_main_name)
include $(DEP_ROOT)/test_src.mkpart

# Generated configuration makefile contains:
#  - Copies of the prior lines as setup for additional mkpart files
#  - $(executable_name), the name of the configured executables
-include _configuration.mk

all: $(executable_name)

.DEFAULT_GOAL := all

# All .o files should be made like .cc files
%.o:
	@mkdir -p $(@D) $(depdir)
	$(CXX) -MMD -MT $@ -MF $(depdir)/$(*F).d $(addprefix @,$(filter %.options, $^)) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $(filter %.cc, $^)

%.d: ;

###
# Legacy module support
###
define legacy_options
-Dinitialize_branch_predictor=b_$1_initialize_branch_predictor
-Dlast_branch_result=b_$1_last_branch_result
-Dpredict_branch=b_$1_predict_branch
-Dinitialize_btb=t_$1_initialize_btb
-Dupdate_btb=t_$1_update_btb
-Dbtb_prediction=t_$1_btb_prediction
-Dprefetcher_initialize=p_$1_prefetcher_initialize
-Dl1i_prefetcher_initialize=p_$1_prefetcher_initialize
-Dl1d_prefetcher_initialize=p_$1_prefetcher_initialize
-Dl2c_prefetcher_initialize=p_$1_prefetcher_initialize
-Dllc_prefetcher_initialize=p_$1_prefetcher_initialize
-Dprefetcher_cache_operate=p_$1_prefetcher_cache_operate
-Dl1i_prefetcher_cache_operate=p_$1_prefetcher_cache_operate
-Dl1d_prefetcher_operate=p_$1_prefetcher_cache_operate
-Dl2c_prefetcher_operate=p_$1_prefetcher_cache_operate
-Dllc_prefetcher_operate=p_$1_prefetcher_cache_operate
-Dprefetcher_cache_fill=p_$1_prefetcher_cache_fill
-Dl1i_prefetcher_cache_fill=p_$1_prefetcher_cache_fill
-Dl1d_prefetcher_cache_fill=p_$1_prefetcher_cache_fill
-Dl2c_prefetcher_cache_fill=p_$1_prefetcher_cache_fill
-Dllc_prefetcher_cache_fill=p_$1_prefetcher_cache_fill
-Dprefetcher_cycle_operate=p_$1_prefetcher_cycle_operate
-Dl1i_prefetcher_cycle_operate=p_$1_prefetcher_cycle_operate
-Dprefetcher_final_stats=p_$1_prefetcher_final_stats
-Dl1i_prefetcher_final_stats=p_$1_prefetcher_final_stats
-Dl1d_prefetcher_final_stats=p_$1_prefetcher_final_stats
-Dl2c_prefetcher_final_stats=p_$1_prefetcher_final_stats
-Dllc_prefetcher_final_stats=p_$1_prefetcher_final_stats
-Dprefetcher_branch_operate=p_$1_prefetcher_branch_operate
-Dl1i_prefetcher_branch_operate=p_$1_prefetcher_branch_operate
-Dinitialize_replacement=p_$1_initialize_replacement
-Dfind_victim=p_$1_find_victim
-Dupdate_replacement_state=p_$1_update_replacement_state
-Dreplacement_final_stats=p_$1_replacement_final_stats
endef
%/legacy.options: | %/__legacy__
	$(file >$@,$(call legacy_options,$(lastword $(subst /, ,$*))))

%/cache_module_decl.inc %/core_module_decl.inc %/module_def.inc &:
	mkdir -p $(@D)
	python3 -m config.legacy --prefix=$* $|

%/__legacy__: ;
###
# END Legacy module support
###

$(executable_name):
	mkdir -p $(@D)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)

$(test_main_name):
	mkdir -p $(@D)
	$(CXX) $(LDFLAGS) -o $@ $(filter-out main.o,$^) $(LOADLIBES) $(LDLIBS)

# Tests: build and run
test: $(test_main_name)
	$(test_main_name)

pytest:
	PYTHONPATH=$(PYTHONPATH):$(ROOT_DIR) python3 -m unittest discover -v --start-directory='test/python'

