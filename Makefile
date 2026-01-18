PLUGIN_NAME = hyprland_settings
SOURCE = main.cpp
OUTPUT = $(PLUGIN_NAME).so

CXX = g++
# Use pkg-config to get the correct include paths
CXXFLAGS = -g -fPIC --no-gnu-unique -std=c++23 $(shell pkg-config --cflags hyprland)
LDFLAGS = -shared -pthread

all: $(OUTPUT)

$(OUTPUT): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(OUTPUT)
