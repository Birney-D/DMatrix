CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

TARGET = dmatrix
SOURCE = dmatrix.cpp

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) $(SOURCE) -o $(TARGET)

clean:
	rm -f $(TARGET)
