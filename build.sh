echo "build simple_engine:"
g++ -Wall -pedantic main.cpp InputManager.cpp -I./ -L/usr/local/lib  -o simple_engine -I/usr/include/OIS -I/usr/include/loki  -std=c++0x -L/usr/lib/x86_64-linux-gnu/ -lX11 -lXxf86vm -ggdb3 -lGL -lGLU -lOIS -O0 -lpthread

echo "build funny_tex:"
g++ -Wall -pedantic FunnyTex.cpp -I./ -lGL -lGLU -lOIS -L/usr/local/lib  -o funny_tex -I/usr/include/OIS -I/usr/include/loki InputManager.cpp -std=c++0x -L/usr/X11R6/lib -lX11 -lXxf86vm -ggdb3 -O0 -lpthread
