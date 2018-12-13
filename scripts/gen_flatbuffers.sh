#!/bin/bash

flatc --cpp --scoped-enums -o generated/ misc/Common.fbs
flatc --cpp --scoped-enums -o generated/ misc/Mesh.fbs
flatc --cpp --scoped-enums -o generated/ misc/Material.fbs
flatc --cpp --scoped-enums -o generated/ misc/Component.fbs
flatc --cpp --scoped-enums -o generated/ misc/SceneTree.fbs
flatc --cpp --scoped-enums -o generated/ misc/ShaderComponent.fbs
flatc --cpp --scoped-enums -o generated/ misc/ShaderProgram.fbs
