#!/bin/sh

flatc --cpp --scoped-enums -o generated/ misc/Mesh.fbs
flatc --cpp --scoped-enums -o generated/ misc/SceneTree.fbs
