CXX := g++
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -Wno-unused-variable -Wno-unused-parameter -pedantic -Isrc -I../SDL/include
LDFLAGS := -L../sdl/build -lSDL3

SRCDIR := src
OBJDIR := build
TARGET := Utah_Raytracer.exe

SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(SRCS:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
DEPS := $(OBJS:.o=.d)

.PHONY: all run clean
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LDFLAGS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	if not exist $(OBJDIR) mkdir $(OBJDIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# automatically rebuild when headers change
-include $(DEPS)

run: $(TARGET)
	.\$(TARGET)

clean:
	if exist $(OBJDIR) rmdir /S /Q $(OBJDIR)
	if exist $(TARGET) del /Q $(TARGET)





#g++ -std=c++20 -O2 -Wall -Wextra -Wno-unused-variable -Wno-unused-parameter -pedantic -Isrc -I../SDL/include src/main.cpp src/xml.cpp src/ecs.cpp -L../sdl/build -lSDL3 -o Utah_Raytracer.exe; .\Utah_Raytracer.exe
#
#cd /C:/Users/Joe/Desktop/Projects/2025_Winter/Utah_Renderer/utah_raytracer
#make        # builds Utah_Raytracer.exe (default target)
#make run    # builds then runs the executable (uses the run target)
#make clean  # removes objects and the exe
