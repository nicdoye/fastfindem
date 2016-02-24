# (GNU)Makefile for tidy.
# Super Simple

SHELL = /bin/sh

prefix = /usr/local
bindir = bin
installdir = $(prefix)/bin
xcodebindir = build/Release

sources = tidy/main.c

# Xcode won't do tabs (or I haven't discovered how to set it per file)
# GNU Make allows you to replace \t with another char (I've used ':')')'
.RECIPEPREFIX := :

# Should be a sensible default on most machines.
# DEFAULT: CC = cc
GCC = gcc
CLANG = clang
# Don't want this? Pass it through on the command line
CFLAGS = -Ofast
# For Xcode
OTHER_CFLAGS = -O3

# Xcode
xcodebuild = xcodebuild
xcodeproj = fastfindem.xcodeproj

# Valgrind http://valgrind.org/
VALGRIND = valgrind --leak-check=full --track-origins=yes

# Facebook Open Source Infer http://fbinfer.com/
INFER = infer -- $(GCC)

# DEFAULT: RM = rm -f
CP = cp -f
INSTALL = install

legacy_binary = tidy
binary = fastfindem
target = $(bindir)/$(binary)

########################################################################

all: default

default: $(sources)
: $(CC) -o $(target) $(sources) $(CFLAGS)

gcc: $(sources)
: $(GCC) -o $(target) $(sources) $(CFLAGS)

clang: $(sources)
: $(CLANG) -o $(target) $(sources) $(OTHER_CFLAGS)

xcode: $(sources) $(xcodeproj)
: $(xcodebuild) -project $(xcodeproj) OTHER_CFLAGS=$(OTHER_CFLAGS)
: $(CP) $(xcodebindir)/$(legacy_binary) $(target)

infer: $(sources)
: $(INFER) $(sources)

valgrind: gcc
: $(VALGRIND) ./$(target) $(ARGV)

clean:
: $(RM) $(target)
: $(xcodebuild) clean 

install: $(target)
: $(INSTALL) -m 0755 $(target) $(installdir)