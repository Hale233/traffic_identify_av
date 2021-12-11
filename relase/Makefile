#vpath %.a ../lib
#vpath %.h ./inc

PAPP_PATH=/home/xuminchao/sapp_run

#CFLAGS          =  -g3 -Wall -fPIC -Werror -O
#CFLAGS          =  -g3 -Wall -fPIC -O
CFLAGS          =  -g3 -Wall -fPIC             
CFLAGS	       	+= $(INCLUDES)
CC              = g++
CCC             = g++
INCLUDES        = -I.
INCLUDES        += -I/opt/MESA/include/MESA/
#INCLUDES        += -I/root/anconda3/include/python3.6m/
#INCLUDES        += -I/usr/include/python3.6m/

#LIB		= -L/root/anconda3/lib/
LIB		= -L/usr/lib/
LIB		+= -lMESA_handle_logger
LIB		+= -lMESA_prof_load
#LIB     += -lpython3.6m
LIB		+= -lrdkafka
LIB		+= -lMESA_field_stat2
LIB		+= -lmaatframe

LIB_FILE	= $(wildcard ../lib/*.a)
SOURCES		= $(wildcard *.c)
SOURCESCPP	= $(wildcard *.cpp)   
OBJECTS 	= $(SOURCES:.c=.o)    
OBJECTSCPP 	= $(SOURCESCPP:.cpp=.o)
DEPS		= $(SOURCES:.c=.d)
DEPSCPP		= $(SOURCESCPP:.cpp=.d)

TARGET		= traffic_identify_av.so

.PHONY:clean all 

all:$(TARGET)

$(TARGET):$(OBJECTSCPP) $(OBJECTS) $(LIB_FILE)
	$(CCC) -shared $(CFLAGS) $(OBJECTSCPP) $(OBJECTS) $(LIB)  -o $@
	
.c.o:
%.d:%.c
	$(CCC) $< -MM $(INCLUDES) > $@
	
.cpp.o:
#%.dpp:%.cpp
	$(CCC) $(CFLAGS) -c $<
clean :
	rm -rf xmc_test.o xmc_test.so xmc_test.cpp
