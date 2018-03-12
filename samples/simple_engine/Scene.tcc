


namespace SE {

Scene::Scene(const Settings & oSettings, Camera & oCurCamera) :
        oSmallElipse(0, 0, 2, 10, 36),
        oBigElipse(0, 0, 2, 100, 36),
        pTex01(TResourceManager::Instance().Create<TTexture>("resource/texture/tst_01.tga")),
        pSceneTree(CreateResource<TSceneTree>("resource/scene/test-01.sesc")) {

        pSceneTree->Print();
}



Scene::~Scene() throw() { ;; }



void Scene::Process() {

        HELPERS::DrawAxes(10);
        oSmallElipse.Draw();
        oBigElipse.Draw();

        glEnable(GL_TEXTURE_2D);

        HELPERS::DrawPlane(4, 4,
                          -1, 2, 0.25,
                           1, 1, 1,
                           pTex01->GetID());

        pSceneTree->Draw();

        glDisable(GL_TEXTURE_2D);

        glColor3f(1, 1, 1);
        glPointSize(15);
        glBegin(GL_POINTS);
        glVertex3f(0, 1, 1);
        glEnd();
}






} //namespace SE
