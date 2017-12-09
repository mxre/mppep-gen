# Windows build with VC2017 build tools using Clang compiler 
# Requires boost headers to be in a searchable path

BUILD_DIR = build

OBJS = \
	$(BUILD_DIR)/PhylogeneticLoader.obj \
	$(BUILD_DIR)/Taxon.obj \
	$(BUILD_DIR)/ThreadPool.obj \
	$(BUILD_DIR)/Timer.obj \
	$(BUILD_DIR)/Time.obj

PROGRAM=phylogeny.exe

INCLUDES = -Isrc/
CXX = clang
CXXFLAGS = -O3 -fexceptions -D_CONSOLE -D_MT -D_DLL -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0601
LDFLAGS= -nologo -subsystem:console
LIBS = msvcrt.lib vcruntime.lib ucrt.lib msvcprt.lib kernel32.lib

all: $(BUILD_DIR)/$(PROGRAM)

{src}.cpp{$(BUILD_DIR)}.obj:
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/$(PROGRAM): $(OBJS)
	link $(LDFLAGS) $(OBJS) $(LIBS) -out:$@

clean:
	-del $(BUILD_DIR)\*.obj $(BUILD_DIR)\$(PROGRAM)
