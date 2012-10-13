###############################################################################
#
# Installation DIRECTORIES
#
# Change these if you want to install somewhere else.

INSTALL_PLUGINS_DIR	=	/usr/lib/ladspa/
INSTALL_INCLUDE_DIR	=	/usr/include/
INSTALL_BINARY_DIR	=	/usr/bin/

###############################################################################
#
# GENERAL
#

INCLUDES	=	-I.
LIBRARIES	=	-ldl -lm
CFLAGS		=	$(INCLUDES) -Wall -Werror -O3 -fPIC
CXXFLAGS	=	$(CFLAGS)
PLUGINS		=	../plugins/kicktrigger.so				
PROGRAMS	=	../bin/analyseplugin				\
			../bin/applyplugin 				\
			../bin/listplugins
CC		=	cc
CPP		=	c++

###############################################################################
#
# RULES TO BUILD PLUGINS FROM C OR C++ CODE
#

../plugins/%.so:	plugins/%.c ladspa.h
	$(CC) $(CFLAGS) -o plugins/$*.o -c plugins/$*.c
	$(LD) -o ../plugins/$*.so plugins/$*.o -shared

../plugins/%.so:	plugins/%.cpp ladspa.h
	$(CPP) $(CXXFLAGS) -o plugins/$*.o -c plugins/$*.cpp
	$(CPP) -o ../plugins/$*.so plugins/$*.o -shared

###############################################################################
#
# TARGETS
#

test:	/tmp/test.wav ../snd/noise.wav always
	@echo ---------------------------------------------
	@echo First listen to the white noise input signal:
	@echo ---------------------------------------------
	-sndfile-play ../snd/noise.wav
	@echo -------------------------
	@echo Compare to plugin output.
	@echo -------------------------
	@echo Should be a noise band around 6000Hz, repeated quietly after 1s.
	-sndfile-play /tmp/test.wav
	@echo Test complete.

install:	targets
	-mkdirhier $(INSTALL_PLUGINS_DIR)
	-mkdirhier $(INSTALL_INCLUDE_DIR)
	-mkdirhier $(INSTALL_BINARY_DIR)
	cp ../plugins/* $(INSTALL_PLUGINS_DIR)
	cp ladspa.h $(INSTALL_INCLUDE_DIR)
	cp ../bin/* $(INSTALL_BINARY_DIR)

/tmp/test.wav:	targets ../snd/noise.wav
	../bin/listplugins
	../bin/analyseplugin ../plugins/kicktrigger.so
	

targets:	$(PLUGINS) $(PROGRAMS)

###############################################################################
#
# PROGRAMS
#

../bin/applyplugin:	applyplugin.o load.o default.o
	$(CC) $(CFLAGS) $(LIBRARIES)					\
		-o ../bin/applyplugin					\
		applyplugin.o load.o default.o

../bin/analyseplugin:	analyseplugin.o load.o default.o
	$(CC) $(CFLAGS) $(LIBRARIES)					\
		-o ../bin/analyseplugin 				\
		analyseplugin.o load.o default.o

../bin/listplugins:	listplugins.o search.o
	$(CC) $(CFLAGS) $(LIBRARIES)					\
		-o ../bin/listplugins	 				\
		listplugins.o search.o

###############################################################################
#
# UTILITIES
#

always:	

clean:
	-rm -f `find . -name "*.o"` ../bin/* ../plugins/*
	-rm -f `find .. -name "*~"`
	-rm -f *.bak core score.srt
	-rm -f *.bb *.bbg *.da *-ann gmon.out bb.out
	-rm -f `find .. -name "*.class"`

backup:		clean
	(cd ../../;							\
	tar czf `date '+../backup/ladspa_sdk.%Y%m%d%H%M.tgz'` ladspa_sdk/)

###############################################################################

