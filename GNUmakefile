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
CFLAGS = -g

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

INSTALLbinary = $(INSTALL) -m 0755 $(target) $(installdir)

########################################################################

all: default

default: $(sources)
: $(CC) -o $(target) $(sources) $(CFLAGS)

gcc: $(sources)
: $(GCC) -o $(target) $(sources) $(CFLAGS)

clang: $(sources)
: $(CLANG) -o $(target) $(sources) $(CFLAGS)

xcode: $(sources) $(xcodeproj)
: $(xcodebuild) -project $(xcodeproj)
: $(CP) $(xcodebindir)/$(legacy_binary) $(target)

infer: $(sources)
: $(INFER) $(sources)

valgrind: gcc
: $(VALGRIND) ./$(target) $(ARGV)

clean:
: $(RM) $(target)
: $(xcodebuild) clean 

install: $(target)
: $(INSTALLbinary)