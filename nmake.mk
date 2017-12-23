
BUILD_DIR = build

!IF [ MKDIR $(BUILD_DIR) 2> NUL ]
!ENDIF

PROGRAM  = phylogeny.exe

CXX      = clang
DEFINES  = -D_DLL
DEPFLAGS = -MT $@ -MV -MMD -MP -MF $(BUILD_DIR)/dependencies.d

all: $(BUILD_DIR)/$(PROGRAM)

{src}.cpp{$(BUILD_DIR)}.obj:
	$(CXX) $(DEPFLAGS) $(INCLUDES) -Wall -fexceptions $(DEFINES) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/$(PROGRAM): $(BUILD_DIR)/unified.obj
	link -nologo -subsystem:console $(LDFLAGS) $(BUILD_DIR)/unified.obj -out:$@

clean:
	-del $(BUILD_DIR)\unified.obj $(BUILD_DIR)\$(PROGRAM)

!IF EXIST($(BUILD_DIR)/dependencies.d)
!INCLUDE $(BUILD_DIR)/dependencies.d
!ENDIF
