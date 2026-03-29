TARGET = me56ps2

SRC_DIR = src
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=%.o)

CXXFLAGS = -Wall -Wextra
LDFLAGS = -pthread

.SUFFIXES: .cpp .o

$(TARGET): $(OBJS)
	$(CXX) -o $(TARGET) $(LDFLAGS) $^

%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: rpi4
rpi4: $(TARGET)

.PHONY: rpi-zero
rpi-zero:
	$(MAKE) CXXFLAGS="$(CXXFLAGS) -DHW_RPI_ZERO" $(TARGET)

.PHONY: rpi-zero2
rpi-zero2:
	$(MAKE) CXXFLAGS="$(CXXFLAGS) -DHW_RPI_ZERO2" $(TARGET)

.PHONY: nanopi-neo2
nanopi-neo2:
	$(MAKE) CXXFLAGS="$(CXXFLAGS) -DHW_NANOPI_NEO2" $(TARGET)

.PHONY: clean
clean:
	$(RM) $(TARGET) $(OBJS)
