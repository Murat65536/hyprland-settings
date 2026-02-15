CXX = g++
CXXFLAGS = -std=c++17 -Wall $(shell pkg-config --cflags gtkmm-4.0 json-glib-1.0)
LDFLAGS = $(shell pkg-config --libs gtkmm-4.0 json-glib-1.0)
TARGET = hyprland-settings-gui
SOURCE = src/main.cpp src/config_io.cpp

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) $(SOURCE) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
