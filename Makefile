# Compiler
CC		= gcc
CXX		= g++

# Flags
CFLAGS		= -Wall -m32 -g -D__HOST_LE__ -D__TARGET_BE__
CXXFLAGS	= $(CFLAGS)
LDFLAGS		= -m32

# Target
TARGET		= armemu

# Objects
OBJS		=		\
		arm.o		\
		memory.o	\
		main.o		\
		utils.o


all: $(TARGET)

$(TARGET): $(OBJS)
	@echo -e "  LD\t$@"
	@$(CXX) $(LDFLAGS) $(OBJS) -o $(TARGET)

%.o: %.c
	@echo -e "  CC\t$<"
	@$(CC) $(CFLAGS) $< -c -o $@

%.o: %.cpp
	@echo -e "  CXX\t$<"
	@$(CXX) $(CXXFLAGS) $< -c -o $@

clean:
	@echo -e "Cleaning..."
	@rm -f $(OBJS) $(TARGET) *~
