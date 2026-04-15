#!/bin/bash

flatc --cpp --scoped-enums -o generated/ misc/Common.fbs
flatc --cpp --scoped-enums -o generated/ misc/Mesh.fbs
flatc --cpp --scoped-enums -o generated/ misc/Material.fbs
flatc --cpp --scoped-enums -o generated/ misc/Audio.fbs
flatc --cpp --scoped-enums -o generated/ misc/AudioClip.fbs
flatc --cpp --scoped-enums -o generated/ misc/Component.fbs
flatc --cpp --scoped-enums -o generated/ misc/SceneTree.fbs
flatc --cpp --scoped-enums -o generated/ misc/ShaderComponent.fbs
flatc --cpp --scoped-enums -o generated/ misc/ShaderProgram.fbs
flatc --cpp --scoped-enums -o generated/ misc/UILocaleTable.fbs
flatc --cpp --scoped-enums -o generated/ misc/AnimationClip.fbs
flatc --cpp --scoped-enums -o generated/ misc/AnimationSkeleton.fbs
flatc --cpp --scoped-enums -o generated/ misc/AnimationGraph.fbs
