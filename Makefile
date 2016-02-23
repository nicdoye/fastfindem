# (GNU)Makefile for tidy.
# Super Simple

SHELL = /bin/sh

prefix = /usr/local
srcdir = tidy
bindir = bin
installdir = ${prefix}/bin
xcodebindir = build/Release

SRCFILES = ${srcdir}/main.c

# Xcode won't do tabs (or I haven't discovered how to set it per file)
# GNU Make allows you to replace \t with another char (I've used ':')')'
.RECIPEPREFIX := :

# Should be a sensible default on most machines.
CC = cc
GCC = gcc
CLANG = clang
CFLAGS = -g

# Xcode
XCODEBUILD = xcodebuild -project
XCODEPROJ = tidy.xcodeproj

# Valgrind http://valgrind.org/
VALGRIND = valgrind --leak-check=full --track-origins=yes

# Facebook Open Source Infer http://fbinfer.com/
INFER = infer -- ${GCC}

RM = rm -f
CP = cp -f
INSTALL = install

BINARY = tidy
TARGET = ${bindir}/${BINARY}

INSTALLBINARY = ${INSTALL} -m 0755 ${TARGET} ${installdir}

########################################################################

all: default

default: ${SRCFILES}
: $(CC) -o ${TARGET} ${SRCFILES}

gcc-debug: ${SRCFILES}
: ${GCC} -o ${TARGET} ${SRCFILES} ${CFLAGS}

clang-debug:
: clang -o ${TARGET} -g ${SRCFILES} ${CFLAGS}

gcc: ${SRCFILES}
: ${GCC} -o ${TARGET} ${SRCFILES}

clang: ${SRCFILES}
: clang -o ${TARGET} ${SRCFILES}

xcode: ${SRCFILES} ${XCODEPROJ}
: ${XCODEBUILD} ${XCODEPROJ}
: ${CP} ${xcodebindir}/${BINARY} ${TARGET}

infer: ${SRCFILES}
: ${INFER} ${SRCFILES}

valgrind: gcc-debug
: ${VALGRIND} ./${TARGET} ${ARGV}

clean:
: ${RM} ${TARGET}

install: ${TARGET}
: ${INSTALLBINARY}