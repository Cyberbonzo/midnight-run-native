CC = gcc
CXX = g++
CPPFLAGS += -Icpu -Isound -Iinclude -Ivideo/sharc/build
CFLAGS += -O1 -fno-strict-aliasing -MMD -MP
CXXFLAGS += -O2 -std=c++20 -fno-strict-aliasing -MMD -MP
CSRC := $(wildcard cpu/wide/page_*.c) cpu/wide/dispatch.c board/zr107.c video/texel_bus.c
CXXSRC := $(wildcard sound/gen/*.cpp) sound/sound_cpu.cpp board/zr_raster.cpp video/zr107_video.cpp video/sharc/dsp.cpp host/midnight.cpp
OBJ := $(addprefix build/,$(CSRC:.c=.o)) $(addprefix build/,$(CXXSRC:.cpp=.o))
.DEFAULT_GOAL := midnight
midnight: $(OBJ)
	$(CXX) $^ -l:libSDL3.so.0 -lm -pthread -o $@
build/sound/gen/%.o: CXXFLAGS += -Og -w
build/video/sharc/dsp.o: CXXFLAGS += -O2 -w
build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
-include $(OBJ:.o=.d)

