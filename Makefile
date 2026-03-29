TARGET = me56ps2

SRC_DIR = src
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=%.o)

CXXFLAGS = -Wall -Wextra
LDFLAGS = -pthread

.SUFFIXES: .cpp .o

$(TARGET): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	$(RM) $(TARGET) $(OBJS)
