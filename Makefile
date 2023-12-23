override ROOT_DIR = $(patsubst %/,%,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
DEP_ROOT = $(ROOT_DIR)/.csconfig/dep
OBJ_ROOT = $(ROOT_DIR)/.csconfig

# vcpkg integration
TRIPLET_DIR = $(patsubst %/,%,$(firstword $(filter-out $(ROOT_DIR)/vcpkg_installed/vcpkg/, $(wildcard $(ROOT_DIR)/vcpkg_installed/*/))))
CPPFLAGS += -I$(ROOT_DIR)/inc -isystem $(TRIPLET_DIR)/include
LDFLAGS  += -L$(TRIPLET_DIR)/lib -L$(TRIPLET_DIR)/lib/manual-link
LDLIBS   += -llzma -lz -lbz2 -lfmt

.PHONY: all clean configclean test

test_main_name=$(ROOT_DIR)/test/bin/000-test-main

# $1 - source dir
# $2 - hash
# $3 - executable name
# $4 - (optional) additional options files
define make_part

$2_objs := $$(patsubst $1/%.cc, $(OBJ_ROOT)/$2/%.o, $$(wildcard $1/*.cc))
$$($2_objs): $(OBJ_ROOT)/$2/%.o: $1/%.cc
$$($2_objs): $(ROOT_DIR)/global.options $4
$$($2_objs): CPPFLAGS += -I$(OBJ_ROOT)/$2/inc -I$1

$2_deps := $$(patsubst $1/%.cc, $(DEP_ROOT)/$2/%.d, $$(wildcard $1/*.cc))
$$($2_deps): $(DEP_ROOT)/$2/%.d: $1/%.cc
$$($2_deps): objdep = $(OBJ_ROOT)/$2
$$($2_deps): CPPFLAGS += -I$(OBJ_ROOT)/$2/inc -I$1

ifeq (,$$(filter clean configclean, $$(MAKECMDGOALS)))
-include $$($2_deps)
endif

dirs += $(OBJ_ROOT)/$2 $(DEP_ROOT)/$2
objs += $$($2_objs)

$(call make_all_parts,$(sort $(dir $(wildcard $1/*/))),$2@,$3,$4)
$3: $$($2_objs)

endef

#make_all_parts = $(if $1,$(call make_part,$(firstword $1),$2,$3,$4)$(call $0,$(wordlist 2,$(words $1),$1),$2_,$3,$4))
make_all_parts = \
$(if $1,\
	$(if $(findstring MODULE,$(firstword $1)),\
		$(call make_part,$(word 2,$1),$2,$3,$4 $(ROOT_DIR)/module.options)$(call $0,$(wordlist 3,$(words $1),$1),$2_,$3,$4 $(ROOT_DIR)/module.options),\
		$(call make_part,$(firstword $1),$2,$3,$4)$(call $0,$(wordlist 2,$(words $1),$1),$2_,$3,$4)\
	)\
)

_pos = $(if $(findstring $1,$2),$(call $0,$1,$(wordlist 2,$(words $2),$2),x $3),$3)
pos = $(words $(call _pos,$1,$2))

# Requires:
# $(source_root)
# $(exe)
# $(opts)
$(DEP_ROOT)/%.mkpart: $(source_roots)
	mkdir -p $(@D)
	$(file >$@,$(call make_all_parts,$(source_roots),$*,$(exe),$(opts)))

# Generated configuration makefile contains:
#  - $(executable_name), the list of all executables in the configuration
#  - $(dirs), the list of all directories that hold object files
#  - $(objs), the list of all object files corresponding to sources
#  - All dependencies and flags assigned according to the modules
#
# Customization points:
#  - OBJ_ROOT: at make-time, override the object file directory
include _configuration.mk

all: $(executable_name)

.DEFAULT_GOAL := all

# Remove all intermediate files
clean:
	@-find src test .csconfig branch btb prefetcher replacement $(clean_dirs) \( -name '*.o' -o -name '*.d' \) -delete &> /dev/null
	@-$(RM) inc/champsim_constants.h
	@-$(RM) inc/cache_modules.h
	@-$(RM) inc/ooo_cpu_modules.h
	@-$(RM) src/core_inst.cc
	@-$(RM) $(test_main_name)

# Remove all configuration files
configclean: clean
	@-$(RM) -r $(dirs) _configuration.mk

reverse = $(if $(wordlist 2,2,$(1)),$(call reverse,$(wordlist 2,$(words $(1)),$(1))) $(firstword $(1)),$(1))

# Link test executable
#$(test_main_name): CPPFLAGS += -I$(ROOT_DIR)/test/cpp/inc
$(test_main_name): CXXFLAGS += -g3 -Og -Wconversion
$(test_main_name): LDLIBS += -lCatch2Main -lCatch2

#$(ROOT_DIR)/prefetcher/spp_dev FIXME legacy
$(DEP_ROOT)/test_src.mkpart: source_roots = $(ROOT_DIR)/test/cpp/src $(ROOT_DIR)/src MODULE $(ROOT_DIR)/btb MODULE $(ROOT_DIR)/branch MODULE $(ROOT_DIR)/prefetcher MODULE $(ROOT_DIR)/replacement
$(DEP_ROOT)/test_src.mkpart: exe = $(test_main_name)
include $(DEP_ROOT)/test_src.mkpart

ifdef POSTBUILD_CLEAN
.INTERMEDIATE: $(objs) $($(OBJS):.o=.d)
endif

# All .o files should be made like .cc files
%.o:
	mkdir -p $(@D)
	$(CXX) $(call reverse, $(addprefix @,$(filter %.options, $^))) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $(filter %.cc, $^)

%.d:
	mkdir -p $(@D)
	$(CXX) -MM -MT $@ -MT $(objdep)/$(*F).o -MF $@ $(CPPFLAGS) $(call reverse, $(addprefix @,$(filter %.options, $^))) $(filter %.cc, $^)

# Link main executables
$(executable_name) $(test_main_name):
	mkdir -p $(@D)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)

# Tests: build and run
test: $(test_main_name)
	$(test_main_name)

pytest:
	PYTHONPATH=$(PYTHONPATH):$(ROOT_DIR) python3 -m unittest discover -v --start-directory='test/python'

