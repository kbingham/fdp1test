
CFLAGS = -I./include
CFLAGS += -g -Wall


dist_bin_SCRIPTS = \
        fdp1-gst-tests \
        fdp1-gst-transcode-file \
        fdp1-gst-width-tests \
        fdp1-v4l2-compliance

fdp1-unit-test_SOURCES = \
        fdp1-unit-tests.c \
        fdp1-v4l2-helpers.c \
        fdp1-buffer.c \
        01-fdp1-open.c \
        02-fdp1-allocation.c \
        03-fdp1-streamon.c \
        04-fdp1-progressive.c \
        05-fdp1-deinterlace.c


fdp1-test_SOURCES = \
	crc.c \
	fdp1-test.c

process-vmalloc_SOURCES = \
	process-vmalloc.c

all:

define build-target

$(1)_OBJECTS = $$(addprefix $(2),$$($(1)_SOURCES))

$(1): $$($(1)_OBJECTS)
	$$(CC) -o $$@ $(CFLAGS) $$^

all: $(1)

endef

$(eval $(call build-target,fdp1-unit-test,src/fdp1-unit-test/))
$(eval $(call build-target,fdp1-test,src/))
$(eval $(call build-target,process-vmalloc,src/))

