echo "build simple_engine:"
g++ -Wall -pedantic main.cpp -I./ -lGL -lGLU -lOIS -L/usr/local/lib  -o simple_engine -I/usr/include/OIS -I/usr/include/loki InputManager.cpp -std=c++0x -L/usr/X11R6/lib -lXxf86vm -ggdb3 -O0

echo "build funny_tex:"
g++ -Wall -pedantic FunnyTex.cpp -I./ -lGL -lGLU -lOIS -L/usr/local/lib  -o funny_tex -I/usr/include/OIS -I/usr/include/loki InputManager.cpp -std=c++0x -L/usr/X11R6/lib -lXxf86vm -ggdb3 -O0
