# Compiler
CXX := g++

# Compiler flags
CXXFLAGS := -std=c++11 -O2 -Wall -I/usr/include -I/usr/include/opencv4

# Libraries
LIBS := -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs -lpthread -lboost_system -lboost_filesystem

# Source and target
SRC := app.cpp
TARGET := app

# Default target
all: $(TARGET)

# Link the target executable
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LIBS)

# Clean up build files
clean:
	rm -f $(TARGET)

# Phony targets
.PHONY: all clean
