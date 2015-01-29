SOURCES:=$(shell ls *.tl)
TARGETS:=$(SOURCES:.tl=.tlb)

all: $(TARGETS)

%.tlb: %.tl ../../tlcompiler Makefile $(SOURCES)
	TL_MODULE_PATH=../../modules ../../tl ../../tlcompiler $<

