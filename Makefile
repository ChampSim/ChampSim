override ROOT_DIR = $(patsubst %/,%,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))

# Customization points:
#  - BIN_ROOT: at make-time, override the binary directory
#  - OBJ_ROOT: at make-time, override the object file directory
#  - DEP_ROOT: at make-time, override the dependency file directory
BIN_ROOT:=bin
OBJ_ROOT:=.csconfig
DEP_ROOT:=$(OBJ_ROOT)

MODULE_ROOT =
BRANCH_ROOT = branch $(addsuffix /branch,$(MODULE_ROOT))
BTB_ROOT = btb $(addsuffix /btb,$(MODULE_ROOT))
PREFETCH_ROOT = prefetcher $(addsuffix /prefetcher,$(MODULE_ROOT))
REPLACEMENT_ROOT = replacement $(addsuffix /replacement,$(MODULE_ROOT))

# vcpkg integration
TRIPLET_DIR = $(patsubst %/,%,$(firstword $(filter-out $(ROOT_DIR)/vcpkg_installed/vcpkg/, $(wildcard $(ROOT_DIR)/vcpkg_installed/*/))))
override CPPFLAGS += -I$(OBJ_ROOT)
override LDFLAGS  += -L$(TRIPLET_DIR)/lib -L$(TRIPLET_DIR)/lib/manual-link
override LDLIBS   += -llzma -lz -lbz2 -lfmt

.PHONY: all clean configclean test pytest

test_main_name=test/bin/000-test-main
executable_name:=
module_dirs = $(BRANCH_ROOT) $(BTB_ROOT) $(PREFETCH_ROOT) $(REPLACEMENT_ROOT)

# List all subdirectories of a given directory
# $1 - parent directory
ls_dirs = $(patsubst %/,%,$(filter %/,$(wildcard $1/*/)))

# Migrate names from a source directory (and suffix) to a target directory (and suffix)
# $1 - source directory
# $2 - target directory
# $3 - source suffix
# $4 - target suffix
# $5 - unique build id
migrate = $(patsubst $1/%$3,$2/$(5)_%$4,$(filter %main.cc,$(wildcard $1/*$3))) $(patsubst $1/%$3,$2/%$4,$(filter-out %main.cc,$(wildcard $1/*$3))) $(foreach subdir,$(call ls_dirs,$1),$(call $0,$(subdir),$(patsubst $1/%,$2/%,$(subdir)),$3,$4,$5))

# Generated configuration makefile contains:
#  - $(executable_name), the list of all executables in the configuration
#  - All dependencies and flags assigned according to the modules
include _configuration.mk

all: $(executable_name)

.DEFAULT_GOAL := all

generated_files = $(OBJ_ROOT)/module_decl.inc

# Remove all intermediate files
clean:
	@-find src test .csconfig $(OBJ_ROOT) $(DEP_ROOT) $(module_dirs) \( -name '*.o' -o -name '*.d' \) -delete &> /dev/null
	@-$(RM) inc/champsim_constants.h
	@-$(RM) inc/cache_modules.h
	@-$(RM) inc/ooo_cpu_modules.h
	@-$(RM) src/core_inst.cc
	@-$(RM) $(test_main_name)

# Remove all configuration files
configclean: clean
	@-find $(module_dirs) \( -name legacy_bridge.h -o -name legacy_bridge.cc -o -name legacy.options \) -delete &> /dev/null
	@-$(RM) $(generated_files) _configuration.mk

reverse = $(if $(wordlist 2,2,$(1)),$(call reverse,$(wordlist 2,$(words $(1)),$(1))) $(firstword $(1)),$(1))

absolute.options:
	@echo "-I$(realpath inc) -isystem $(realpath $(TRIPLET_DIR)/include)" > $@

attach_options = $(call reverse, $(addprefix @,$(filter %.options, $^)))

# All .o files should be made like .cc files
define obj_recipe
	@mkdir -p $(@D)
	$(CXX) $(attach_options) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $(filter %.cc, $^)
endef

# All .d files should be preprocessed only
DEPFLAGS = -MM -MT $@ -MT $(@:.d=.o)
define dep_recipe
	@mkdir -p $(@D)
	$(CXX) $(attach_options) $(DEPFLAGS) $(CPPFLAGS) -MF $@ $(filter %.cc, $^)
endef

### Module support

# Get a list of module objects descended from the given directories
# $1 - list of directories to traverse
get_module_list = $(foreach mod_type,$1,$(call migrate,$(mod_type),$(OBJ_ROOT)/modules/$(mod_type),.cc,.o))

# The base modules shipped with ChampSim
base_module_objs = $(call get_module_list, $(module_dirs))

# Get the module objects that are not base
nonbase_module_objs = $(filter-out $(base_module_objs),$1)

# Make the legacy support structure
%/legacy.options %/legacy_bridge.h %/legacy_bridge.cc:
	python3 -m config.legacy $*

# Expands to the given legacy file if the module folder contains a file named "__legacy__"
# $1 - path to search
# $2 - file to produce
maybe_legacy_file = $(and $(filter-out legacy%,$(notdir $1)),$(filter %/__legacy__,$(wildcard $(dir $1)*)),$(dir $1)$2)

# Secondary expansion is required to pass the build ID into executables and also to connect legacy options as prerequisites
.SECONDEXPANSION:

### Object Files

base_source_dir = src
test_source_dir = test/cpp/src
base_options = absolute.options global.options

# Get the base object files, with the 'main' file mangled
# $1 - A unique key identifying the build
get_base_objs = $(call migrate,src,$(OBJ_ROOT),.cc,.o,$1)
test_base_objs = $(call migrate,$(test_source_dir),$(OBJ_ROOT)/test,.cc,.o,TEST)

# Pass the build ID into the main file
$(OBJ_ROOT)/%_main.o: CPPFLAGS += -DCHAMPSIM_BUILD=0x$*
$(DEP_ROOT)/%_main.d: CPPFLAGS += -DCHAMPSIM_BUILD=0x$*

# Connect the main sources to the src/ directory
base_main_prereqs = $(base_source_dir)/main.cc $(base_options)
$(OBJ_ROOT)/%_main.o: $(base_main_prereqs) | $(@:$(OBJ_ROOT)/%.o=$(DEP_ROOT)/%.d)
	$(obj_recipe)
$(DEP_ROOT)/%_main.d: $(base_main_prereqs) | $(generated_files)
	$(dep_recipe)

# Connect non-main sources to the src/ directory
base_nonmain_prereqs = $(base_source_dir)/$*.cc $(base_options)
$(OBJ_ROOT)/%.o: $$(base_nonmain_prereqs) | $(@:$(OBJ_ROOT)/%.o=$(DEP_ROOT)/%.d)
	$(obj_recipe)
$(DEP_ROOT)/%.d: $$(base_nonmain_prereqs) | $(generated_files)
	$(dep_recipe)

# Connect the test main to the test/cpp/src/ directory
test_main_prereqs = $(test_source_dir)/000-test-main.cc $(base_options)
$(OBJ_ROOT)/test/TEST_000-test-main.o: $(test_main_prereqs) | $(@:$(OBJ_ROOT)/%.o=$(DEP_ROOT)/%.d)
	$(obj_recipe)
$(DEP_ROOT)/test/TEST_000-test-main.d: $(test_main_prereqs) | $(generated_files)
	$(dep_recipe)

# Connect non-main test sources to the test/cpp/src/ drirctory
test_nonmain_prereqs = $(test_source_dir)/$*.cc $(base_options)
$(OBJ_ROOT)/test/%.o: $$(test_nonmain_prereqs) | $(@:$(OBJ_ROOT)/%.o=$(DEP_ROOT)/%.d)
	$(obj_recipe)
$(DEP_ROOT)/test/%.d: $$(test_nonmain_prereqs) | $(generated_files)
	$(dep_recipe)

# Connect module objects to their sources
base_module_prereqs = $*.cc $(call maybe_legacy_file,$*,legacy.options) module.options $(base_options)
$(OBJ_ROOT)/modules/%.o: $$(base_module_prereqs) | $(@:$(OBJ_ROOT)/%.o=$(DEP_ROOT)/%.d)
	$(obj_recipe)
$(DEP_ROOT)/modules/%.d: $$(base_module_prereqs) | $(generated_files)
	$(dep_recipe)

# Give the test executable some additional options
$(test_main_name): override CPPFLAGS += -DCHAMPSIM_TEST_BUILD
$(test_main_name): override CXXFLAGS += -g3 -Og
$(test_main_name): override LDLIBS += -lCatch2Main -lCatch2

# Associate objects with executables
$(test_main_name): $(call get_base_objs,TEST) $(test_base_objs) $(base_module_objs)
$(executable_name): $(call get_base_objs,$$(build_id)) $(base_module_objs)

# Link main executables
$(executable_name) $(test_main_name):
	mkdir -p $(@D)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)

# Get prerequisites for module_decl.inc
# $1 - object file paths
module_decl_prereqs = $(foreach mod,$(patsubst $(OBJ_ROOT)/modules/%,%,$(basename $1)),$(call maybe_legacy_file,$(mod),legacy_bridge.h))
define module_decl_lines
$(if $^,echo "#include \"$(abspath $(firstword $^))\"" >> $@)
$(if $(wordlist 2,$(words $^),$^),$(call $0,$(wordlist 2,$(words $^),$^)))
endef
$(OBJ_ROOT)/module_decl.inc: $(call module_decl_prereqs,$(base_module_objs))
	$(info Building $@ with modules $^)
	@echo "#ifndef CHAMPSIM_LEGACY_CACHE_MODULE_DECL" > $@
	@echo "#define CHAMPSIM_LEGACY_CACHE_MODULE_DECL" >> $@
	@$(module_decl_lines)
	@echo "#endif" >> $@

# Tests: build and run
ifdef TEST_NUM
selected_test = -\# "[$(addprefix #,$(filter $(addsuffix %,$(TEST_NUM)), $(patsubst %.cc,%,$(notdir $(wildcard $(test_source_dir)/*.cc)))))]"
endif
test: $(test_main_name)
	$(test_main_name) $(selected_test)

pytest:
	PYTHONPATH=$(PYTHONPATH):$(ROOT_DIR) python3 -m unittest discover -v --start-directory='test/python'

ifeq (,$(filter clean configclean pytest, $(MAKECMDGOALS)))
-include $(patsubst $(OBJ_ROOT)/%.o,$(DEP_ROOT)/%.d,$(call get_base_objs,TEST) $(test_base_objs) $(base_module_objs))
endif
