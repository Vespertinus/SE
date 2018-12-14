
namespace FUNNY_TEX {


OrthoScene::OrthoScene(const Settings & oSettings, SE::Camera & oCurCamera) :
        pTex01(SE::CreateResource<SE::TTexture>(
                                "resource/texture/_MG_1252.png",
                                SE::StoreTexture2D::Settings(false),
                                SE::OpenCVImgLoader::Settings()
                                )),
        oCamera(oCurCamera) {

        oCamera.SetPos(0, -20, 0);
        oCamera.LookAt(0, 0, 0);

        pTestMesh = SE::CreateResource<SE::TMesh>("resource/mesh/tests/test_ship01.sems");

        oCamera.ZoomTo(pTestMesh->GetBBox());

        pShaderTex = SE::CreateResource<SE::TTexture>(
                                "resource/mesh/tests/checker02.png",
                                SE::StoreTexture2D::Settings(false),
                                SE::OpenCVImgLoader::Settings()
                                );



        pSimpleShader = SE::CreateResource<SE::ShaderProgram>("resource/shader_program/simple.sesp");
        //pBrickShader  = SE::CreateResource<SE::ShaderProgram>("resource/shader_program/brick.sesp");
        pBrickShader  = SE::CreateResource<SE::ShaderProgram>("resource/shader_program/simple_tex.sesp");

        pBrickShader->Use();
/*
        pBrickShader->SetVariable("BrickColor",    glm::vec3(1, 0.3, 0.2));
        pBrickShader->SetVariable("MortarColor",   glm::vec3(0.85, 0.86, 0.84));
        pBrickShader->SetVariable("BrickSize",     glm::vec2(0.6, 0.3));
        pBrickShader->SetVariable("BrickPct",      glm::vec2(0.9, 0.85));
        pBrickShader->SetVariable("LightPosition", glm::vec3(5, -5, 10));
*/
        SE::TRenderState::Instance().SetTexture(SE::TextureUnit::DIFFUSE, pShaderTex);
        //pBrickShader->SetTexture(SE::TextureUnit::DIFFUSE, pShaderTex);
        //glUseProgram(0);
}

OrthoScene::~OrthoScene() throw() { ;; }


void OrthoScene::Process() {

        glUseProgram(0);

        SE::HELPERS::DrawAxes(10);

        glEnable(GL_TEXTURE_2D);

        /*
        auto & oCamSettings = oCamera.GetSettings();

        SE::HELPERS::DrawPlane(oCamSettings.width, oCamSettings.height,
                        -oCamSettings.width / 2, 0, -oCamSettings.height / 2,
                        1, 0, 1,
                        pTex01->GetID());
        */
        /*
        SE::HELPERS::DrawPlane(1024, 1024,
                        -512, 0, -512,
                        1, 0, 1,
                        pTex01->GetID());
        */

        pBrickShader->Use();
        auto & mModelView = oCamera.GetMVMatrix();
        glm::mat3 mNormal = glm::inverseTranspose(glm::mat3(mModelView));
        auto & mModelViewProjection = oCamera.GetMVPMatrix();
        SE::TRenderState::Instance().SetViewProjection(oCamera.GetMVPMatrix());


/*        pBrickShader->SetVariable("NormalMatrix", mNormal);
        pBrickShader->SetVariable("MVMatrix", mModelView);*/
        pBrickShader->SetVariable("MVPMatrix", mModelViewProjection);
        //pBrickShader->SetTexture(SE::TextureUnit::DIFFUSE, pShaderTex);

        //pTestMesh->Draw();

        glDisable(GL_TEXTURE_2D);


        pSimpleShader->Use();
        pSimpleShader->SetVariable("MVPMatrix", mModelViewProjection);


        glColor3f(1, 1, 1);
        glBindTexture (GL_TEXTURE_2D, 0);
        glBegin(GL_POLYGON);

        float x_pos = 0, y_pos = 1, z_pos = 0,
              x_dir = 1, z_dir = 1,
              width = 25.6, height = 25.6;

        glVertex3f(x_pos, y_pos, z_pos);
        glNormal3f(0.1, 0.1, 0.1);
        glVertex3f(x_dir * width + x_pos, y_pos, z_pos);
        glNormal3f(0.3, 0.3, 0.3);
        glVertex3f(x_dir * width + x_pos, y_pos, z_dir * height + z_pos);
        glNormal3f(0.7, 0.7, 0.7);
        //glVertex3f(x_pos, y_pos, z_dir * height + z_pos);

        glEnd();

}

void OrthoScene::PostRender() {}


} //namespace FUNNY_TEX


