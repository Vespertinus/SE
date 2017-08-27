


namespace SE {

Scene::Scene(const Settings & oSettings) : 
  oSmallElipse(0, 0, 2, 10, 36), 
  oBigElipse(0, 0, 2, 100, 36),
  //pTex01(TResourceManager::Instance().Create<TTexture>("resource/texture/checker_01.tga")) {
  pTex01(TResourceManager::Instance().Create<TTexture>("resource/texture/tst_01.tga")) {


  /* Test Resource manager compilation
  TGALoader::Settings       oLoadStrategySettings;
  StoreTexture2D::Settings  oStoreStrategySettings;
  //change preference

  //create resource

  TTexture * pTex01 = TResourceManager::Instance().Create<TTexture>("", oStoreStrategySettings, oLoadStrategySettings);
  
  TTexture * pTex02 = TResourceManager::Instance().Create<TTexture>("");
  */

}



Scene::~Scene() throw() { ;; }



void Scene::Process() {

  HELPERS::DrawAxes(10);
  //DrawElipse(0, 0, 2, 10, 18);
  oSmallElipse.Draw();
  oBigElipse.Draw();

  HELPERS::DrawPlane(4, 4, 
                     -1, 2, 0.25,
                     1, 1, 1,
                     pTex01->GetID());
  //glPushMatrix();
	//glTranslatef(-1.5f,0.0f,-6.0f);   // Move Left 1.5 Units And Into The Screen 6.0

	glColor3f(1, 1, 1);
  glPointSize(15);
  glBegin(GL_POINTS);
  glVertex3f(0, 1, 1);
  glEnd();
/*
	glColor4f(1.0f, 0.0f, 0.0f, 0.0f);
	// draw a triangle
	glBegin(GL_POLYGON);        // start drawing a polygon
	glVertex3f( 0.0f, 1.0f, 0.0f);    // Top
	glVertex3f( 1.0f,-1.0f, 0.0f);    // Bottom Right
	glVertex3f(-1.0f,-1.0f, 0.0f);    // Bottom Left
	glEnd();          // we're done with the polygon

	glTranslatef(3.0f,0.0f,0.0f);           // Move Right 3 Units


	glColor4f(0.0f, 1.0f, 0.0f, 0.0f);
	// draw a square (quadrilateral)
	glBegin(GL_QUADS);        // start drawing a polygon (4 sided)
	glVertex3f(-1.0f, 1.0f, 0.0f);    // Top Left
	glVertex3f( 1.0f, 1.0f, 0.0f);    // Top Right
	glVertex3f( 1.0f,-1.0f, 0.0f);    // Bottom Right
	glVertex3f(-1.0f,-1.0f, 0.0f);    // Bottom Left
	glEnd();          // done with the polygon
  //glPopMatrix();
*/
}






} //namespace SE
