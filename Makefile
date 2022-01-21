.SUFFIXES: .cpp .o

CXX=g++

SRCDIR=src/
INC=include/
LIBS=lib/

# SRCS:=$(wildcard src/*.cpp)
# OBJS:=$(SRCS:.cpp=.o)

# main source file
TARGET_SRC:=$(SRCDIR)main.cpp
TARGET_OBJ:=$(SRCDIR)main.o
STATIC_LIB:=$(LIBS)libbpt.a

# Include more files if you write another source file.
SRCS_FOR_LIB:= \
	$(SRCDIR)bpt.cpp \
	$(SRCDIR)file.cpp \
	$(SRCDIR)db.cpp \
	$(SRCDIR)buffer.cpp \
	$(SRCDIR)lock_table.cpp \
	$(SRCDIR)trx.cpp \
	$(SRCDIR)recovery.cpp \
	$(SRCDIR)log_buffer.cpp \
	$(SRCDIR)log_file.cpp

OBJS_FOR_LIB:=$(SRCS_FOR_LIB:.cpp=.o)

CXXFLAGS+= -g -fPIC -I $(INC) -lpthread

TARGET=main

all: $(TARGET)

$(TARGET): $(TARGET_OBJ) $(STATIC_LIB)
	$(CXX) $(CXXFLAGS) $< -o $@ -L $(LIBS) -lbpt

%.o: %.c
	$(CXX) $(CXXFLAGS) $^ -c -o $@

clean:
	rm $(TARGET) $(TARGET_OBJ) $(OBJS_FOR_LIB) $(LIBS)*

library:
	gcc -shared -Wl,-soname,libbpt.so -o $(LIBS)libbpt.so $(OBJS_FOR_LIB)

$(STATIC_LIB): $(OBJS_FOR_LIB)
	ar cr $@ $^
