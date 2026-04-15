
#include "SphereBuilder.h"
#include <TextureBuilder.h>
#include <MeshGen.h>

namespace SE {

Scene::Scene(const Settings & oSettings) :
        pSceneTree(CreateRawResource<TSceneTree>("pbr_demo", true)) {

        // --- Meshes ---
        auto hSphereMesh = CreateSphereMesh(16, 32);

        MeshBuilder oFloorBuilder(VertexLayout::PosNormTanUV());
        MeshGen::Quad(oFloorBuilder, glm::vec2(12.f, 12.f));
        auto hFloorMesh = oFloorBuilder.Upload("floor_plane");

        MeshBuilder oTransBuilder(VertexLayout::PosNormTanUV());
        MeshGen::Sphere(oTransBuilder, 1.0f, 16, 32);
        auto hTranslucentMesh = oTransBuilder.Upload("translucent_sphere");

        // --- 1×1 procedural textures ---
        auto hWhiteTex      = TextureBuilder(1, 1).Fill(255, 255, 255).Upload("pbr_white");
        auto hNormalTex     = TextureBuilder(1, 1).Fill(128, 128, 255).Upload("pbr_normal");
        auto hSpecularTex   = TextureBuilder(1, 1).Fill(  0, 255, 255).Upload("pbr_specular");
        auto hDarkGreyTex   = TextureBuilder(1, 1).Fill( 40,  40,  40).Upload("pbr_dark_grey");
        auto hFloorSpecTex  = TextureBuilder(1, 1).Fill(200,   0,   0).Upload("pbr_floor_spec");
        auto hTransBlueTex  = TextureBuilder(1, 1).Fill(100, 150, 255, 128).Upload("pbr_trans_blue");
        auto hTransSpecTex  = TextureBuilder(1, 1).Fill(100,   0,   0).Upload("pbr_trans_spec");

        // --- Floor ---
        auto hFloorMat = CreateResource<Material>("resource/material/pbr_rough_dielectric.semt");
        auto * pFloorMat = GetResource(hFloorMat);
        pFloorMat->SetTexture(TextureUnit::DIFFUSE,  hDarkGreyTex);
        pFloorMat->SetTexture(TextureUnit::NORMAL,   hNormalTex);
        pFloorMat->SetTexture(TextureUnit::SPECULAR, hFloorSpecTex);

        auto pFloorNode = pSceneTree->Create("Floor");
        pFloorNode->SetPos(glm::vec3(0.f, -1.f, 0.f));
        pFloorNode->CreateComponent<StaticModel>(hFloorMesh, hFloorMat);

        // --- Materials (5 PBR spheres) ---
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
                auto res = pNode->CreateComponent<StaticModel>(hSphereMesh, hMat);
                if (res != uSUCCESS) {
                        throw std::runtime_error("failed to create StaticModel");
                }
        }

        // --- Translucent glass sphere ---
        auto hTransMat = CreateResource<Material>("resource/material/pbr_smooth_dielectric.semt");
        auto * pTransMat = GetResource(hTransMat);
        pTransMat->SetTexture(TextureUnit::DIFFUSE,  hTransBlueTex);
        pTransMat->SetTexture(TextureUnit::NORMAL,   hNormalTex);
        pTransMat->SetTexture(TextureUnit::SPECULAR, hTransSpecTex);
        pTransMat->SetBlendMode(BlendMode::Translucent);

        auto pTransNode = pSceneTree->Create("TranslucentGlass");
        pTransNode->SetPos(glm::vec3(0.f, 1.5f, 3.5f));
        pTransNode->CreateComponent<StaticModel>(hTranslucentMesh, hTransMat);

        // --- IBL: default neutral irradiance is active from DeferredRenderer ctor ---
        // The engine provides a neutral solid-color irradiance cubemap (0.03*PI ambient).
        // To use a custom environment map, pre-bake 6 PNG faces from an HDR panorama
        // (e.g. using Filament's cmgen tool) and load:
        /*
        auto hEnv = CreateResource<TTexture>("env/outdoor",
                        TextureStock{},
                        StoreTextureCubeMap::Settings("resource/texture/env/outdoor/"));
        GetSystem<TRenderer>().SetIBL(hEnv);
        */

        // Multiple cubemaps can be loaded up-front and swapped by calling SetIBL again.

        // --- Camera ---
        {
                auto pCamNode = pSceneTree->Create("Camera");
                auto res = pCamNode->CreateComponent<Camera>(oSettings.oCamSettings);
                if (res != uSUCCESS) {
                        throw std::runtime_error("failed to create Camera");
                }
                pCamera = pCamNode->GetComponent<Camera>();
                pCamera->SetPos(0.f, 5.f, 16.f);
                pCamera->LookAt(0.f, 0.5f, 0.f);
                GetSystem<TRenderer>().SetCamera(pCamera);
                pCamNode->CreateComponent<BasicController>();
        }

        // --- Lights (clustered shading demo — 11 point lights) ---
#if SE_DEFERRED_RENDERER
        {

                DirLight oDir;
                oDir.direction = glm::normalize(glm::vec3(-1.f, -2.f, -1.f));
                oDir.intensity = 0.5f;
                GetSystem<TRenderer>().SetDirLight(oDir);

                // 3 main lights (warm, cool, fill)
                vLights.emplace_back(PointLight{
                                { -5.f, 5.f, 5.f },
                                16.f,
                                {  1.f, 0.85f, 0.7f },
                                3.f
                                });

                vLights.emplace_back(PointLight{
                                {  5.f, 5.f, 5.f },
                                16.f,
                                {  0.6f, 0.8f, 1.f },
                                3.f
                                });

                vLights.emplace_back(PointLight{
                                {  0.f, 8.f, 0.f },
                                20.f,
                                {  1.f, 1.f, 1.f },
                                2.f
                                });


                // 8 additional colored lights in a grid to demo clustered shading
                for (int ix = -1; ix <= 1; ix += 2) {
                        for (int iz = -1; iz <= 1; iz += 2) {

                                vLights.emplace_back(PointLight{
                                                { ix * 3.f, 4.f, iz * 3.f },
                                                10.f,
                                                { 0.8f + 0.2f * (ix > 0), 0.5f, 1.0f - 0.2f * (iz > 0) },
                                                1.5f
                                                });
                        }
                }

                for (auto & oLight : vLights) {
                        GetSystem<TRenderer>().AddPointLight(oLight);
                }

        }
#endif

        pSceneTree->Print();

        // --- Subscribe to keyboard events for debug ---
        auto & oEM = GetSystem<EventManager>();
        oEM.AddListener<EKeyDown, &Scene::OnKeyDown>(this);
}

Scene::~Scene() noexcept {
        auto & oEM = GetSystem<EventManager>();
        oEM.RemoveListener<EKeyDown, &Scene::OnKeyDown>(this);
}

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

        /*
        for (auto & oLight : vLights) {
                GetSystem<DebugRenderer>().DrawSphere(oLight.position, oLight.radius);
        }*/
}

void Scene::OnKeyDown(const Event & oEvent) {

        auto key = oEvent.Get<EKeyDown>().key;
        if (key == Keys::F9) {
#if SE_DEFERRED_RENDERER
                GetSystem<TRenderer>().WriteDebug();
#endif
        }
        else if (key == Keys::T) {
                pSceneTree->Print();
        }
}

} // namespace SE
