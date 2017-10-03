
echo "build offscreen rendering:"
g++-6 -Wall -D_GLIBCXX_USE_CXX11_ABI=0 ./samples/offscreen_rendering/main.cpp -I./core -I./common -I./X11 -I./units -I./samples/offscreen_rendering -I../../open_source/tinyobjloader -I../../usr/include -I../../usr/include/loki -I/usr/include/boost160/ -L/usr/local/lib  -o offscreen_demo -I/usr/include/loki -std=c++14 -L/usr/X11R6/lib -L../../usr/lib  -lopencv_core -lopencv_highgui -lglapi -lOSMesa -lm -ggdb3 -O0 -lboost_system -lboost_filesystem -lpthread
