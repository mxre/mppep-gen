# Windows build with VC2017 build tools using Clang compiler 
# Requires boost headers to be in a searchable path

BUILD_DIR = build

OBJS =\
	$(BUILD_DIR)/PhylogeneticLoader.obj\
	$(BUILD_DIR)/Taxon.obj\
	$(BUILD_DIR)/ThreadPool.obj\
	$(BUILD_DIR)/Timer.obj\
	$(BUILD_DIR)/CPUTime.obj

PROGRAM  = phylogeny.exe

INCLUDES = -Isrc/
CXX      = clang
DEFINES  = -D_MT -D_DLL -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0601

all: $(BUILD_DIR)/$(PROGRAM)

{src}.cpp{$(BUILD_DIR)}.obj:
	$(CXX) $(INCLUDES) -Wall -fexceptions $(DEFINES) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/$(PROGRAM): $(OBJS)
	link -nologo -subsystem:console $(LDFLAGS) $(OBJS) -out:$@

clean:
	-del $(BUILD_DIR)\*.obj $(BUILD_DIR)\$(PROGRAM)
