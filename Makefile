ECHO = echo
RM = rm -f

CFLAGS = -Wall -Werror -ggdb
CXXFLAGS = -Wall -Werror -ggdb

LDFLAGS = -lm -lncurses

BIN = pokemon
OBJS = main.o heap.o

all: $(BIN)

$(BIN): $(OBJS)
        gcc $^ -o $@ $(LDFLAGS)

-include $(OBJS:.o=.d)

%.o: %.c
        @$(ECHO) Compiling $<
        @$(CC) $(CFLAGS) -MMD -MF $*.d -c $<

%.o: %.cpp
	@$(ECHO) Compiling $<
        @$(CXX) $(CXXFLAGS) -MMD -MF $*.d -c $<

.PHONY: all clean clobber

clean:
      	@$(ECHO) Removing all generated files
        @$(RM) *.o $(BIN) *.d TAGS core vgcore.* gmon.out

clobber: clean
	@$(ECHO) Removing backup files
        @$(RM) *~ \#* *pgm