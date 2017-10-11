
# SimpleEngine

simple rendering engine for inhouse usage.
support rendering using OpenGL on GPU and on CPU with OSMesa

C++14 required (at least gcc6)

Dependency:
 * General:
   * Loki - library written by Andrei Alexandrescu as part of his book Modern C++ Design (TODO - need to switch on boost hana or spirit)
     * http://loki-lib.sourceforge.net/
   * Boost - widely known C++ libraries pack
     * http://www.boost.org/
   * glm  - OpenGL Mathematics
     * https://glm.g-truc.net
   * tinyobjloader - wavefront obj (and mtl ) format parser
     * https://github.com/syoyo/tinyobjloader
   * spdlog - fast C++ logging library
     * https://github.com/gabime/spdlog
   * OpenCV - Open Source Computer Vision Library. 
     * https://opencv.org/
   * OpenGL - usually shiped with graphics drivers 
     * or with open source realisation - mesa
 * GPU enabled build only:
   * OIS - Object Oriented Input System
     * http://sourceforge.net/projects/wgois/develop
   * X11 - only basic library for window creation and input manipulation
     * part of any Linux distribution
 * CPU enabled build only:
   * OSMesa - part of Mesa 3D Graphics Library
     * https://www.mesa3d.org/


