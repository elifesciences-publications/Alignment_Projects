
include $(ALN_LOCAL_MAKE_PATH)/aln_makefile_std_defs

# If Valgrind (run as valgrind --leak-check=yes <mycmdline>)
#CFLAGS = -g -O0
#LFLAGS =

appname = lsq

files =\
 lsq.cpp\
 lsq_Layers.cpp

objs = ${files:.cpp=.o}

all : $(appname)

clean :
	rm -f *.o

$(appname) : .CHECK_GENLIB ${objs}
	$(CC) $(LFLAGS) ${objs} $(LINKS_STD) $(OUTPUT)

