# ============================================================================
#  Rural Market - GNU Makefile (no-CMake fallback)
#
#  Voxel rural village market scene, OpenGL 1.x fixed-function + GLUT.
#
#  Usage
#  -----
#      make            # build ./rural_market  (default target: all)
#      make run        # build, then launch it
#      make clean      # remove obj/ and the binary
#      make CXX=clang++          # override the compiler
#      make CXXFLAGS="-O0 -g"    # override the flags
#
#  Sources live in src/; object files are written to obj/ so the project root
#  stays free of build artifacts.
#
#  Dependencies (Debian / Ubuntu):
#      sudo apt install build-essential freeglut3-dev libglu1-mesa-dev
#  Dependencies (MSYS2 / MinGW-w64):
#      pacman -S mingw-w64-x86_64-freeglut
#
#  Platform link flags are auto-detected from `uname -s`.
#  A CMakeLists.txt is also provided if you prefer CMake.
# ============================================================================

CXX      ?= g++
CXXFLAGS  = -O2 -Wall -Wextra -std=c++11
TARGET    = rural_market

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
    # freeglut + Mesa GLU + legacy GL + libm
    LDLIBS = -lglut -lGLU -lGL -lm
endif

ifeq ($(UNAME_S),Darwin)
    # macOS ships GLUT/OpenGL as frameworks; both are deprecated but present.
    LDLIBS    = -framework OpenGL -framework GLUT
    CXXFLAGS += -Wno-deprecated-declarations
endif

# MSYS2 / MinGW / Cygwin report things like MINGW64_NT-10.0, MSYS_NT-10.0,
# CYGWIN_NT-10.0 - match on a substring rather than an exact value.
ifneq (,$(findstring MINGW,$(UNAME_S)))
    LDLIBS = -lfreeglut -lglu32 -lopengl32 -lwinmm
    TARGET = rural_market.exe
endif
ifneq (,$(findstring MSYS,$(UNAME_S)))
    LDLIBS = -lfreeglut -lglu32 -lopengl32 -lwinmm
    TARGET = rural_market.exe
endif
ifneq (,$(findstring CYGWIN,$(UNAME_S)))
    LDLIBS = -lfreeglut -lglu32 -lopengl32 -lwinmm
    TARGET = rural_market.exe
endif

# Last-resort default if uname reported something unexpected.
LDLIBS ?= -lglut -lGLU -lGL -lm

SRCDIR = src
OBJDIR = obj

SRCS = $(SRCDIR)/main.cpp $(SRCDIR)/Scene.cpp $(SRCDIR)/Stall.cpp \
       $(SRCDIR)/Customer.cpp $(SRCDIR)/Carriage.cpp \
       $(SRCDIR)/GraphicsHelpers.cpp
OBJS = $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
HDRS = $(wildcard $(SRCDIR)/*.h)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDLIBS)

# Every object depends on every header - coarse but always correct here.
$(OBJS): $(HDRS) | $(OBJDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(OBJDIR) rural_market rural_market.exe

.PHONY: all clean run
