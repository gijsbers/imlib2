# Unit test makefile
#
noinst_PROGRAMS = $(GTESTS)

CLEANFILES = file.c img_save-*.*

 GTEST_LIBS = -lgtest -lstdc++

 GTESTS  = test_file
 GTESTS += test_load
 GTESTS += test_load_2
 GTESTS += test_save
 GTESTS += test_grab
 GTESTS += test_scale
 GTESTS += test_rotate

 AM_CFLAGS  = -Wall -Wextra -Werror -Wno-unused-parameter
 AM_CFLAGS += $(CFLAGS_ASAN)

 AM_CXXFLAGS = $(AM_CFLAGS)

 AM_CPPFLAGS  = -I $(top_builddir) -I $(top_srcdir)/src/lib
 AM_CPPFLAGS += -I $(top_builddir)/src/lib
 AM_CPPFLAGS += -D SRC_DIR='"$(top_srcdir)"'
 AM_CPPFLAGS += -D BUILD_TEST=1

# Must have proper -lImlib2 on all progs
 LIBS  = -L$(top_builddir)/src/lib -lImlib2
 LIBS += $(GTEST_LIBS)

%.c: $(top_srcdir)/src/lib/%.c
	ln -s $< $@

test_file_SOURCES = test_file.cpp
nodist_test_file_SOURCES = file.c
test_file_LDADD = $(LIBS)

test_load_SOURCES = test_load.cpp
test_load_LDADD = $(LIBS)

test_load_2_SOURCES = test_load_2.cpp
test_load_2_LDADD = $(LIBS) -lz

test_save_SOURCES = test_save.cpp
test_save_LDADD = $(LIBS)

test_grab_SOURCES = test_grab.cpp
test_grab_LDADD = $(LIBS)

test_scale_SOURCES = test_scale.cpp
test_scale_LDADD = $(LIBS) -lz

test_rotate_SOURCES = test_rotate.cpp
test_rotate_LDADD = $(LIBS) -lz

 TESTS_RUN = $(addprefix run-, $(GTESTS))

 TEST_ENV = IMLIB2_LOADER_PATH=$(top_builddir)/src/modules/loaders/.libs

 VG_PROG = valgrind --leak-check=full

all-local: run

.PHONY: run $(TESTS_RUN)
run: $(TESTS_RUN)
$(TESTS_RUN): run-%: %
	$(TEST_ENV) ./$* $(RUN_OPTS)

 TESTS_RUN_VG = $(addprefix run-vg-, $(GTESTS))

.PHONY: run-vg $(TESTS_RUN_VG)
run-vg: $(TESTS_RUN_VG)
$(TESTS_RUN_VG): run-vg-%: %
	$(TEST_ENV) $(VG_PROG) ./$* $(RUN_OPTS)
