
#include "SphereBuilder.h"
#include <TextureBuilder.h>

namespace SE {

Scene::Scene(const Settings & oSettings) :
        pSceneTree(CreateRawResource<TSceneTree>("pbr_demo", true)) {

        // --- Sphere mesh ---
        auto hMesh = CreateSphereMesh(16, 32);

        // --- 1×1 procedural textures ---
        auto hWhiteTex    = TextureBuilder(1, 1).Fill(255, 255, 255).Upload("pbr_white");
        auto hNormalTex   = TextureBuilder(1, 1).Fill(128, 128, 255).Upload("pbr_normal");
        auto hSpecularTex = TextureBuilder(1, 1).Fill(  0, 255, 255).Upload("pbr_specular");

        // --- Materials (5 variations) ---
        const char * matPaths[] = {
                "resource/material/pbr_rough_dielectric.semt",
                "resource/material/pbr_smooth_dielectric.semt",
                "resource/material/pbr_demo.semt",
                "resource/material/pbr_rough_metal.semt",
                "resource/material/pbr_emissive.semt",
        };
        const char * nodeNames[] = {
                "RoughDielectric", "SmoothDielectric", "SmoothMetal", "RoughMetal", "Emissive"
        };
        const float xpos[] = { -4.f, -2.f, 0.f, 2.f, 4.f };

        for (int i = 0; i < 5; ++i) {
                auto hMat = CreateResource<Material>(matPaths[i]);
                auto * pMat = GetResource(hMat);
                pMat->SetTexture(TextureUnit::DIFFUSE,  hWhiteTex);
                pMat->SetTexture(TextureUnit::NORMAL,   hNormalTex);
                pMat->SetTexture(TextureUnit::SPECULAR, hSpecularTex);

                auto pNode = pSceneTree->Create(nodeNames[i]);
                pNode->SetPos(glm::vec3(xpos[i], 0.f, 0.f));
                auto res = pNode->CreateComponent<StaticModel>(hMesh, hMat);
                if (res != uSUCCESS) {
                        throw std::runtime_error("failed to create StaticModel");
                }
        }

        // --- Camera ---
        {
                auto pCamNode = pSceneTree->Create("Camera");
                auto res = pCamNode->CreateComponent<Camera>(oSettings.oCamSettings);
                if (res != uSUCCESS) {
                        throw std::runtime_error("failed to create Camera");
                }
                pCamera = pCamNode->GetComponent<Camera>();
                pCamera->SetPos(0.f, 4.f, 14.f);
                pCamera->LookAt(0.f, 0.f, 0.f);
                GetSystem<TRenderer>().SetCamera(pCamera);
                pCamNode->CreateComponent<BasicController>();
        }

        // --- Lights ---
        {
                DirLight oDir;
                oDir.direction = glm::normalize(glm::vec3(-1.f, -2.f, -1.f));
                oDir.intensity = 0.6f;
                GetSystem<TRenderer>().SetDirLight(oDir);

                PointLight oP1;
                oP1.position  = { -5.f, 4.f, 4.f };
                oP1.color     = {  1.f, 0.85f, 0.7f };
                oP1.intensity = 3.f;
                oP1.radius    = 14.f;

                PointLight oP2;
                oP2.position  = {  5.f, 4.f, 4.f };
                oP2.color     = {  0.6f, 0.8f, 1.f };
                oP2.intensity = 3.f;
                oP2.radius    = 14.f;

                PointLight oP3;
                oP3.position  = {  0.f, 8.f, 0.f };
                oP3.color     = {  1.f, 1.f, 1.f };
                oP3.intensity = 2.f;
                oP3.radius    = 18.f;

                GetSystem<TRenderer>().AddPointLight(oP1);
                GetSystem<TRenderer>().AddPointLight(oP2);
                GetSystem<TRenderer>().AddPointLight(oP3);
        }

        pSceneTree->Print();
}

Scene::~Scene() noexcept {}

void Scene::Process() {

        //HELPERS::DrawAxes(5);

        /*
        auto & oDebugRenderer = GetSystem<DebugRenderer>();
        oDebugRenderer.DrawGrid(pSceneTree->GetRoot()->GetTransform());

        pSceneTree->GetRoot()->DepthFirstWalk([](SE::TSceneTree::TSceneNodeExact & oNode) {

                oNode.DrawDebug();
                return true;
        });
        */
}

} // namespace SE
