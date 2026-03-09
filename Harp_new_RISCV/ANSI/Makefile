
# List all of the directories where there are source files, including
# '.' which is the current directory.  This could be replaced with a
# recursive search of the entire directory tree.

 subdirs = .. . ../pdcurses drivers drivers/Linux

# Define the compiler once and use the definition later.  Makes it
# easier to do cross-compilation with a different compiler.
CC = gcc
AR = ar rcv

# Seach all listed subdirs for C files and add them to the list of
# sources
SOURCES = $(foreach dir,$(subdirs),$(wildcard $(dir)/*.c))

# Seach all listed subdirs for .h files and add them to the list of
# headers
HEADERS = $(foreach dir,$(subdirs),$(wildcard $(dir)/*.h))

# for each source file, create a corresponding object file.
OBJECTS = $(patsubst %.c, %.o, $(SOURCES))

# We may want the compiler to include some extra directories in the
# default include search path. We use a foreach to add each subdir
# (including '.' to the include search path.
INCLUDE = $(foreach dir,$(subdirs),-I$(dir)) 

CFLAGS = -g -c

# We may want some extra flags on the final link.
LDFLAGS =

# A flag for the compiler to tell it to only run the preprocessor and
# output dependencies.
DEPENDFLAGS=-M

# Extra defs for debugging, etc
DEFINE += -DPDCDEBUG


# The default target is libpdcurses.a, because it is the first
# target. That is just the way make works. Commented lines inside a
# target (recipe) will be echoed when make is run.  This is a helpful
# tool for debugging makefiles.

# before building the target, you must build all the object files
# adding depend to the dependency list makes sure that "make depend"
# is run and that all dependencies are updated.
libpdcurses.a: $(OBJECTS) 
	# All of the objects are built. Creating $@
	$(AR) $(LDFLAGS) -o $@ $(OBJECTS) 

# "make clean" will remove all .o files from the directory tree, and
# remove the default target 'sortstrings' if it exists.
clean:
	rm -f $(OBJECTS)
	rm -f sortstrings

# "make realclean" will make clean, then remove all files ending with
# the '~" character, then remove the .depend file, if it exists
realclean:  clean
	find . -name \*~ -exec rm \{\} \;
	rm -f .depend
	rm -f libpdcurses.a

# now, a recipe for converting a .c file to a .o file and put the .o
# file in the same directory as the .c file.
.c.o:
	$(CC) $(CFLAGS) $(DEFINE) $(INCLUDE) $< -o $(patsubst %.c, %.o, $<)

# The previous recipe will never fire, unless we add some
# dependencies.  Running "make depend" will create a list of
# dependencies in a file called ".depend" that we will include into
# this file.
depend: $(SOURCES) $(HEADERS)
	$(CC) $(DEFINE) $(INCLUDE) $(DEPENDFLAGS) $(SOURCES) > .depend
	# Dependencies updated


# if we have a .depend file, include it
ifeq (.depend,$(wildcard .depend))
include .depend
endif
