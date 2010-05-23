
namespace SE {

Scene::Scene() { ;; }



Scene::~Scene() throw() { ;; }



void Scene::Process() {

	glTranslatef(-1.5f,0.0f,-6.0f);   // Move Left 1.5 Units And Into The Screen 6.0

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

}






} //namespace SE
