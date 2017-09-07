
echo "build offscreen rendering:"
g++ -Wall -pedantic ./samples/offscreen_rendering/main.cpp -I./core -I./common -I./X11 -I./units -I./samples/offscreen_rendering -I../../open_source/tinyobjloader -I../../usr/include -I../../usr/include/loki -L/usr/local/lib  -o offscreen_demo -I/usr/include/loki -std=c++0x -L/usr/X11R6/lib -L../../usr/lib  -lopencv_core -lopencv_highgui -lglapi -lOSMesa -lGLU -lm -ggdb3 -O0
