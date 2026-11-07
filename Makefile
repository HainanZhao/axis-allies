CXX      := clang++
CXXFLAGS := -std=c++20 -O3 -march=native -mavx2 -mfma -ffast-math -fno-math-errno -MMD -MP -Wall -Wextra -Wno-unused-parameter -Iinclude -isystem vendor/raylib/src
LDFLAGS  := vendor/raylib/src/libraylib.a -framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo -framework AudioToolbox -lpthread

SRCS := src/main.cpp \
        src/data/game_data.cpp \
        src/audio/sound_engine.cpp \
        src/rts/rts_engine.cpp \
        src/rts/rts_models.cpp \
        src/rts/rts_renderer.cpp \
        src/ui/ui_theme.cpp \
        src/ui/app_ui.cpp

OBJS := $(SRCS:.cpp=.o)
TARGET := bin/axis_allies_2004

.PHONY: all clean test app run install uninstall

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TARGET)
	./$(TARGET) --self-test

app: $(TARGET)
	./build_mac_app.sh

run: $(TARGET)
	./$(TARGET)

install: app
	@echo "Installing Axis & Allies 2004 to /Applications..."
	rm -rf "/Applications/Axis & Allies 2004.app"
	cp -R "Axis & Allies 2004.app" "/Applications/"
	@mkdir -p /usr/local/bin
	@printf '#!/bin/sh\nexec "/Applications/Axis & Allies 2004.app/Contents/MacOS/axis_allies_2004" "$$@"\n' > /usr/local/bin/axis_allies_2004
	@chmod +x /usr/local/bin/axis_allies_2004
	@ln -sf /usr/local/bin/axis_allies_2004 /usr/local/bin/axis-allies
	@/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f "/Applications/Axis & Allies 2004.app" 2>/dev/null || true
	@echo "Installation complete!"
	@echo "  -> App Bundle: /Applications/Axis & Allies 2004.app"
	@echo "  -> CLI Command: axis_allies_2004 (or axis-allies)"

uninstall:
	rm -rf "/Applications/Axis & Allies 2004.app"
	rm -f /usr/local/bin/axis_allies_2004 /usr/local/bin/axis-allies
	@echo "Uninstalled Axis & Allies 2004."

clean:
	rm -f $(OBJS) $(OBJS:.o=.d) $(TARGET)
	rm -rf "Axis & Allies 2004.app"

-include $(OBJS:.o=.d)
