
BUILD_DIR = build

$(shell mkdir $(BUILD_DIR) 2>/dev/null)

PROGRAM = phylogeny

CXX      = g++
DEPFLAGS = -MT $@ -MMD -MP -MF $(BUILD_DIR)/dependencies.d

$(BUILD_DIR)/$(PROGRAM): src/unified.cpp
	$(CXX) $(DEPFLAGS) -Wall -std=c++17 -pthread $(CXXFLAGS) $< -lstdc++fs -o $@

clean:
	-rm $(BUILD_DIR)/$(PROGRAM)

$(BUILD_DIR)/dependencies.d: ;

include $(BUILD_DIR)/dependencies.d
