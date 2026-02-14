override ROOT_DIR = $(patsubst %/,%,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))

# Customization points:
#  - BIN_ROOT: at make-time, override the binary directory
#  - OBJ_ROOT: at make-time, override the object file directory
#  - DEP_ROOT: at make-time, override the dependency file directory
BIN_ROOT:=bin
OBJ_ROOT:=.csconfig
DEP_ROOT:=$(OBJ_ROOT)

override MODULE_ROOT += $(ROOT_DIR)
override BRANCH_ROOT += $(addsuffix /branch,$(MODULE_ROOT))
override BTB_ROOT += $(addsuffix /btb,$(MODULE_ROOT))
override PREFETCH_ROOT += $(addsuffix /prefetcher,$(MODULE_ROOT))
override REPLACEMENT_ROOT += $(addsuffix /replacement,$(MODULE_ROOT))

# vcpkg integration
TRIPLET_DIR = $(patsubst %/,%,$(firstword $(filter-out $(ROOT_DIR)/vcpkg_installed/vcpkg/, $(wildcard $(ROOT_DIR)/vcpkg_installed/*/))))
override CPPFLAGS += -I$(OBJ_ROOT)
override LDFLAGS  += -L$(TRIPLET_DIR)/lib -L$(TRIPLET_DIR)/lib/manual-link
override LDLIBS   += -lCLI11 -llzma -lz -lbz2 -lfmt

.PHONY: all clean compile_commands compile_commands_clean configclean test pytest maketest

test_main_name=test/bin/000-test-main
build_ids:=
executable_name:=
prereq_for_generated:=

# List all subdirectories of a given directory
# $1 - parent directory
ls_dirs = $(patsubst %/,%,$(filter %/,$(wildcard $1/*/)))

# Expands to the given legacy file if the module folder contains a file named "__legacy__"
# $1 - path to search
# $2 - file to produce
maybe_legacy_file = $(if $(filter %/__legacy__,$(wildcard $(dir $1)*)),$(addprefix $(dir $1),$2))

# Migrate names from a source directory (and suffix) to a target directory (and suffix)
# $1 - source directory
# $2 - target directory
# $3 - unique build id
migrate = $(patsubst $1/%.cc,$2/%.o,$(join $(dir $4),$(patsubst %main.cc,$3_%main.cc,$(notdir $4))))
get_object_list = $(call migrate,$1,$2,$3,$(wildcard $1/*.cc) $(call maybe_legacy_file,$1/,legacy_bridge.cc)) $(foreach subdir,$(call ls_dirs,$1),$(call $0,$(subdir),$(patsubst $1/%,$2/%,$(subdir)),$3))

# Return the trailing portion of a word sequence
# $1 - the sequence
tail = $(wordlist 2,$(words $1),$1)

# Split a path into a series of words that are path componenents
# $1 - the path to split
_root_standin=__ROOT__
split_path = $(subst /, ,$(patsubst /%,$(_root_standin)/%,$1))

# Join a series of words into a path
# $1 - the path componenents
join_path = $(subst $(eval) $(eval),,$(filter-out $(_root_standin),$(firstword $1) $(addprefix /,$(call tail,$1))))

# Return the common prefix between two paths
# $1 - the first path
# $2 - the second path
common_prefix_impl = $(if $(and $1,$2),$(if $(findstring $(firstword $1),$(firstword $2)),$(firstword $1) $(call $0,$(call tail,$1),$(call tail,$2))))
common_prefix = $(call join_path,$(call $0_impl,$(call split_path,$1),$(call split_path,$2)))

# Remove the given prefix from each word
# $1 - the prefix to remove
# $2 - the words to remove from
remove_prefix_impl = $(if $1,$(if $(findstring $(firstword $1),$(firstword $2)),$(call $0,$(call tail,$1),$(call tail,$2))),$2)
remove_prefix = $(call join_path,$(call $0_impl,$(call split_path,$1),$(call split_path,$2)))

# Given a prefix, return the relative prefix of the same length
# $1 - the prefix
make_relative_prefix = $(call join_path,$(patsubst %,..,$(call split_path,$1)))

# Return the relative path from one path to another
# $1 - the destination path
# $2 - the origin path
#relative_path_impl = $(if $2,$(call make_relative_prefix,$2)/$1,$1)
#relative_path = $(call $0_impl,$(call remove_prefix,$(call common_prefix,$1,$2),$1),$(call remove_prefix,$(call common_prefix,$1,$2),$2))
relative_path = $(shell python3 -c "import os.path; print(os.path.relpath(\"$1\", start=\"$2\"))")

# Recursively find all files matching a pattern within a directory
# $1 - the directory to search
# $2 - the pattern to match
rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

# Get the parent directory of a path
# $1 - the path
parent_dir = $(patsubst %/,%,$(dir $1))

.DEFAULT_GOAL := all

generated_files = $(OBJ_ROOT)/module_decl.inc $(OBJ_ROOT)/legacy_bridge.h
module_dirs = $(foreach d,$(BRANCH_ROOT) $(BTB_ROOT) $(PREFETCH_ROOT) $(REPLACEMENT_ROOT),$(call relative_path,$(abspath $d),$(ROOT_DIR)))

# Remove all intermediate files
clean:
	@-find src test .csconfig $(OBJ_ROOT) $(DEP_ROOT) $(module_dirs) \( -name '*.o' -o -name '*.d' \) -delete &> /dev/null
	@-$(RM) inc/champsim_constants.h
	@-$(RM) inc/cache_modules.h
	@-$(RM) inc/ooo_cpu_modules.h
	@-$(RM) src/core_inst.cc
	@-$(RM) $(test_main_name)

# Remove all compile_commands.json files
compile_commands_clean:
	@find $(ROOT_DIR) $(module_dirs) -type f -name 'compile_commands.json' -delete &> /dev/null
	@find $(ROOT_DIR) $(module_dirs) -type d -name '.cache' -exec rm -r {} \; &> /dev/null

# Remove all configuration files
configclean: clean compile_commands_clean
	@-find $(module_dirs) -name 'legacy*' -delete &> /dev/null
	@-$(RM) $(generated_files) _configuration.mk

reverse = $(if $(wordlist 2,2,$(1)),$(call reverse,$(call tail,$1)) $(firstword $(1)),$(1))

absolute.options:
	@echo "-I$(realpath inc) -isystem $(realpath $(TRIPLET_DIR)/include)" > $@

attach_options = $(call reverse, $(addprefix @,$(filter %.options, $^)))

# All .o files should be made like .cc files
define obj_recipe
	$(CXX) $(attach_options) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $(filter %.cc, $^)
endef

# All .d files should be preprocessed only
DEPFLAGS = -MM -MT $@ -MT $(@:.d=.o)
define dep_recipe
	$(CXX) $(attach_options) $(DEPFLAGS) $(CPPFLAGS) -MF $@ $(filter %.cc, $^)
endef

### Module support

get_module_obj_dir=$(OBJ_ROOT)/modules/$(patsubst ..%,externUPdir%,$(subst /..,_UPdir,$1))
get_module_src_dir=$(patsubst externUPdir%,..%,$(subst _UPdir,/..,$(patsubst $(DEP_ROOT)/modules/%,%,$(patsubst $(OBJ_ROOT)/modules/%,%,$1))))

# Get a list of module objects descended from the given directories
# $1 - list of directories to traverse
get_module_list = $(foreach mod_type,$1,$(call get_object_list,$(mod_type),$(call get_module_obj_dir,$(mod_type))))

# The base modules shipped with ChampSim
base_module_objs = $(call get_module_list, $(module_dirs))

# The module objects that are not base
nonbase_module_objs =

# Secondary expansion is required to pass the build ID into executables and also to connect legacy options as prerequisites
.SECONDEXPANSION:

# Make the legacy support structure
define python_legacy_recipe
python3 -m config.legacy $(addprefix --kind=,$1) $(dir $(abspath $@))
endef

%/legacy.options: config/legacy.py | %/__legacy__
	$(call python_legacy_recipe, options)

%/legacy_bridge.h: config/legacy.py | %/__legacy__
	$(call python_legacy_recipe, header)

%/legacy_bridge.cc: config/legacy.py | %/__legacy__
	$(call python_legacy_recipe, source)

%/legacy_bridge.inc: config/legacy.py | %/__legacy__
	$(call python_legacy_recipe, mangle)

# This is a hacky way to get this to work:
# Examine the module object files to learn which functions are defined, and legacy_bridge.h will select them at constexpr time
function_patch_options_prereqs = $(filter-out %/legacy_bridge.o,$(call get_object_list,$*,$(call get_module_obj_dir,$*)))
%/function_patch.options: $$(function_patch_options_prereqs) | %/__legacy__
	@echo -DCHAMPSIM_LEGACY_FUNCTION_NAMES="\"\\\"$(shell nm --demangle $^ | cut -c20- | sed -n "s/CACHE:://gp" | sed "s/(.*)//g")\"\\\"" > $@

# Write a file that is a sequence of included files
define include_sequence_lines_impl
$(if $1,echo "#include \"$(call relative_path,$(firstword $(patsubst %/,%,$(dir $1))),$(@D))/$(firstword $(notdir $1))\"" >> $@)
$(if $1,$(call $0,$(call tail,$1)))
endef
define include_sequence_lines
$(info Building $@ with modules $^)
echo "#ifndef $1" > $@
echo "#define $1" >> $@
$(call $0_impl,$^)
echo "#endif" >> $@
endef

### Object Files

base_source_dir = src
base_include_dir = inc
test_source_dir = test/cpp/src
base_options = absolute.options global.options

ifeq (,$(OBJ_ROOT))
	$(error The value of OBJ_ROOT cannot be empty)
endif

# Get prerequisites for module_decl.inc
# $1 - object file paths
module_decl_prereqs = $(foreach mod,$(call get_module_src_dir,$1),$(call maybe_legacy_file,$(mod),legacy_bridge.inc))
$(OBJ_ROOT)/module_decl.inc: $$(call module_decl_prereqs,$$(dir $(base_module_objs)) $$(prereq_for_generated)) | $$(dir $$@)
	@$(call include_sequence_lines, CHAMPSIM_LEGACY_MODULE_DECL)

legacy_bridge_prereqs = $(foreach mod,$(call get_module_src_dir,$1),$(call maybe_legacy_file,$(mod),legacy_bridge.h))
$(OBJ_ROOT)/legacy_bridge.h: $$(call legacy_bridge_prereqs,$$(dir $(base_module_objs)) $$(prereq_for_generated)) | $$(dir $$@)
	@$(call include_sequence_lines, CHAMPSIM_LEGACY_BRIDGE)

# Generated configuration makefile contains:
#  - $(executable_name), the list of all executables in the configuration
#  - All dependencies and flags assigned according to the modules
ifeq (,$(filter clean compile_commands_clean configclean pytest maketest, $(MAKECMDGOALS)))
include _configuration.mk
endif

all: $(executable_name)

# Get the base object files, with the 'main' file mangled
# $1 - A unique key identifying the build
get_base_objs = $(call get_object_list,$(base_source_dir),$(OBJ_ROOT),$1)
test_base_objs = $(call get_object_list,$(test_source_dir),$(OBJ_ROOT)/test,TEST)

# Pass the build ID into the main file
$(OBJ_ROOT)/%_main.o: CPPFLAGS += -DCHAMPSIM_BUILD=0x$*
$(DEP_ROOT)/%_main.d: CPPFLAGS += -DCHAMPSIM_BUILD=0x$*

# Connect the main sources to the src/ directory
base_main_prereqs = $(base_source_dir)/main.cc $(base_options)
$(OBJ_ROOT)/%_main.o: $(base_main_prereqs) | $(@:$(OBJ_ROOT)/%.o=$(DEP_ROOT)/%.d) $$(dir $$@)
	$(obj_recipe)
$(DEP_ROOT)/%_main.d: $(base_main_prereqs) | $(generated_files) $$(dir $$@)
	$(dep_recipe)

# Connect non-main sources to the src/ directory
base_nonmain_prereqs = $(base_source_dir)/$*.cc $(base_options)
$(OBJ_ROOT)/%.o: $$(base_nonmain_prereqs) | $(@:$(OBJ_ROOT)/%.o=$(DEP_ROOT)/%.d) $$(dir $$@)
	$(obj_recipe)
$(DEP_ROOT)/%.d: $$(base_nonmain_prereqs) | $(generated_files) $$(dir $$@)
	$(dep_recipe)

# Connect the test main to the test/cpp/src/ directory
test_main_prereqs = $(test_source_dir)/000-test-main.cc $(base_options)
$(OBJ_ROOT)/test/TEST_000-test-main.o: $(test_main_prereqs) | $(@:$(OBJ_ROOT)/%.o=$(DEP_ROOT)/%.d) $$(dir $$@)
	$(obj_recipe)
$(DEP_ROOT)/test/TEST_000-test-main.d: $(test_main_prereqs) | $(generated_files) $$(dir $$@)
	$(dep_recipe)

# Connect non-main test sources to the test/cpp/src/ drirctory
test_nonmain_prereqs = $(test_source_dir)/$*.cc $(base_options)
$(OBJ_ROOT)/test/%.o: $$(test_nonmain_prereqs) | $(@:$(OBJ_ROOT)/%.o=$(DEP_ROOT)/%.d) $$(dir $$@)
	$(obj_recipe)
$(DEP_ROOT)/test/%.d: $$(test_nonmain_prereqs) | $(generated_files) $$(dir $$@)
	$(dep_recipe)

# Connect module objects to their sources
base_module_prereqs = $(call get_module_src_dir,$(@D))/$(basename $(@F)).cc $(call maybe_legacy_file,$(call get_module_src_dir,$@),$(if $(filter-out %/legacy_bridge,$(basename $@)),legacy.options,function_patch.options)) module.options $(base_options)
$(OBJ_ROOT)/modules/%.o: $$(base_module_prereqs) | $(@:$(OBJ_ROOT)/%.o=$(DEP_ROOT)/%.d) $$(dir $$@)
	$(obj_recipe)
$(DEP_ROOT)/modules/%.d: $$(base_module_prereqs) | $(generated_files) $$(dir $$@)
	$(dep_recipe)

$(sort $(OBJ_ROOT)/ $(DEP_ROOT)/ $(BIN_ROOT)/ test/bin/):
	mkdir -p $@

$(OBJ_ROOT)/test/ $(OBJ_ROOT)/modules/: | $(OBJ_ROOT)/
	mkdir $@

$(OBJ_ROOT)/test/%/: | $(OBJ_ROOT)/test/
	mkdir -p $@

$(OBJ_ROOT)/modules/%/: | $(OBJ_ROOT)/modules/
	mkdir -p $@

ifneq ($(OBJ_ROOT),$(DEP_ROOT))
ifeq (,$(DEP_ROOT))
	$(error The value of DEP_ROOT cannot be empty)
endif

$(DEP_ROOT)/test/ $(DEP_ROOT)/modules/: | $(DEP_ROOT)/
	mkdir $@

$(DEP_ROOT)/test/%/: | $(DEP_ROOT)/test/
	mkdir -p $@

$(DEP_ROOT)/modules/%/: | $(DEP_ROOT)/modules/
	mkdir -p $@
endif

# Give the test executable some additional options
$(test_main_name): override CPPFLAGS += -DCHAMPSIM_TEST_BUILD
$(test_main_name): override CXXFLAGS += -g3 -Og
$(test_main_name): override LDLIBS += -lCatch2Main -lCatch2

# Associate objects with executables
$(test_main_name): $(call get_base_objs,TEST) $(test_base_objs) $(base_module_objs) $(nonbase_module_objs) | $$(dir $$@)
$(executable_name): $(call get_base_objs,$$(build_id)) $(base_module_objs) $(nonbase_module_objs) | $$(dir $$@)

# Link main executables
$(executable_name) $(test_main_name):
	$(CXX) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)

# compile_commands: Create compile_commands.json file
#
# Include ALL modules by default, and creates a separate compile_commands.json
# file for each module, src, and tests.
src_compile_commands_file = $(base_source_dir)/compile_commands.json
inc_compile_commands_file = $(base_include_dir)/compile_commands.json
test_compile_commands_file = $(test_source_dir)/compile_commands.json
module_compile_commands_files = $(foreach mod,$(module_dirs),$(foreach subdir,$(call ls_dirs,$(mod)),$(subdir)/compile_commands.json))

$(src_compile_commands_file): $(call rwildcard,$(base_source_dir),*.cc)
	python3 $(ROOT_DIR)/config/compile_commands/src.py --build-id $(build_id) --champsim-dir $(ROOT_DIR) --config-dir $(OBJ_ROOT)

$(inc_compile_commands_file): $(call rwildcard,$(base_include_dir),*.h)
	python3 $(ROOT_DIR)/config/compile_commands/inc.py --champsim-dir $(ROOT_DIR) --config-dir $(OBJ_ROOT)

$(test_compile_commands_file): $(call rwildcard,$(test_source_dir),*.cc)
	python3 $(ROOT_DIR)/config/compile_commands/test.py --champsim-dir $(ROOT_DIR) --config-dir $(OBJ_ROOT)

$(module_compile_commands_files): $(call rwildcard,$(call parent_dir,$@),*.cc)
	python3 $(ROOT_DIR)/config/compile_commands/module.py --module-dir $(call parent_dir,$@) --champsim-dir $(ROOT_DIR) --config-dir $(OBJ_ROOT)

compile_commands: $(src_compile_commands_file) $(inc_compile_commands_file) $(test_compile_commands_file) $(module_compile_commands_files)

# Tests: build and run
ifdef TEST_NUM
selected_test = -\# "[$(addprefix #,$(filter $(addsuffix %,$(TEST_NUM)), $(patsubst %.cc,%,$(notdir $(wildcard $(test_source_dir)/*.cc)))))]"
endif
test: $(test_main_name)
	$(test_main_name) $(selected_test)

pytest:
	PYTHONPATH=$(PYTHONPATH):$(ROOT_DIR) python3 -m unittest discover -v --start-directory='test/python'

ifeq (,$(filter clean compile_commands compile_commands_clean configclean pytest maketest, $(MAKECMDGOALS)))
-include $(patsubst $(OBJ_ROOT)/%.o,$(DEP_ROOT)/%.d,$(foreach build_id,TEST $(build_ids),$(call get_base_objs,$(build_id))) $(test_base_objs) $(base_module_objs))
endif

ifeq (maketest,$(findstring maketest,$(MAKECMDGOALS)))
include $(ROOT_DIR)/test/make/Makefile.test
endif

.NOTINTERMEDIATE: $(dir $(base_module_objs) $(nonbase_module_objs))
#.SECONDARY: $(call maybe_legacy_file,$(call get_module_src_dir,$(dir $(base_module_objs) $(nonbase_module_objs))),legacy_bridge.cc legacy_bridge.h legacy_bridge.inc function_patch.options legacy.options)
