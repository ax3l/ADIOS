INCLUDES = $(all_includes)
INCLUDES += -I$(top_srcdir)/src -I$(top_srcdir)/src/public

AUTOMAKE_OPTIONS = no-dependencies

all-local:
	test "$(srcdir)" = "$(builddir)" || cp $(srcdir)/*.xml $(builddir)

noinst_PROGRAMS = arrays_write arrays_read

arrays_write_SOURCES = arrays_write.c
arrays_write_LDADD = $(top_builddir)/src/libadios.a $(ADIOSLIB_LDADD)
arrays_write_LDFLAGS = $(ADIOSLIB_LDFLAGS)
arrays_read_SOURCES = arrays_read.c
arrays_read_LDADD = $(top_builddir)/src/libadios.a $(ADIOSLIB_LDADD)
arrays_read_LDFLAGS = $(ADIOSLIB_LDFLAGS)

CLEANFILES = *.bp
CC=$(MPICC)
EXTRA_DIST = arrays.xml README
