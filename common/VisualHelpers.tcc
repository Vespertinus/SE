
namespace SE {

namespace HELPERS {

void DrawAxes(const float size) {

  glLineWidth(4);

  glColor3f(1, 0, 0);
  glBegin(GL_LINES);
  glVertex3f(0, 0, 0);
  glVertex3f(size, 0, 0);
  glEnd();

  glColor3f(0, 1, 0);
  glBegin(GL_LINES);
  glVertex3f(0, 0, 0);
  glVertex3f(0, size, 0);
  glEnd();

  glColor3f(0, 0, 1);
  glBegin(GL_LINES);
  glVertex3f(0, 0, 0);
  glVertex3f(0, 0, size);
  glEnd();

  glLineWidth(1);

  glColor3f(1, 0, 0);
  glBegin(GL_LINES);
  for (float i = 0; i < size; ++i) {
    glVertex3f(0, i, 0);
    glVertex3f(size, i, 0);
  }
  glEnd();
  
  glColor3f(0, 1, 0);
  glBegin(GL_LINES);
  for (float i = 0; i < size; ++i) {
    glVertex3f(0, 0, i);
    glVertex3f(0, size, i);
  }
  glEnd();

  glColor3f(0, 0, 1);
  glBegin(GL_LINES);
  for (float i = 0; i < size; ++i) {
    glVertex3f(i, 0, 0);
    glVertex3f(i, 0, size);
  }
  glEnd();

}




Elipse::Elipse(const float x_pos, const float y_pos, const float z_pos, const float new_radius, const uint32_t new_edge_count) : radius(new_radius), edge_count(new_edge_count) {

  delta = (360 * (M_PI / 180.0)) / edge_count;
  
  ChangePos(x_pos, y_pos, z_pos); 
  Resize(new_radius, new_edge_count);
}

Elipse::~Elipse() throw() { ;; }

void Elipse::Resize(const float new_radius, const uint32_t new_edge_count) {

  radius      = new_radius;
  edge_count  = new_edge_count;
  
  oVertArray.resize(edge_count);
  oColorArray.resize(edge_count);

  float alpha;

  for (uint32_t i = 0; i < edge_count; ++i) {
    alpha = delta * i;
    oVertArray[i].first     = radius * cos(alpha);  //x
    oVertArray[i].second    = radius * sin(alpha);  //y

    oColorArray[i].first    = (oVertArray[i].first  > 0) ? oVertArray[i].first  / radius : fabs(oVertArray[i].first) / (radius *  2);
    oColorArray[i].second   = (oVertArray[i].second > 0) ? oVertArray[i].second / radius : fabs(oVertArray[i].second) / (radius * 2);

    //fprintf(stderr, "oColorArray: x = %f, y = %f\n", oColorArray[i].first, oColorArray[i].second);
  }
  
}

void Elipse::ChangePos(const float x_pos, const float y_pos, const float z_pos) {

  pos[0] = x_pos;
  pos[1] = y_pos;
  pos[2] = z_pos;
}

void Elipse::Draw() const {

  glBegin(GL_LINES);
  for (uint32_t i = 1; i < edge_count; ++i) {
    
    glColor3f(oColorArray[i - 1].first, oColorArray[i - 1].second, 0);
    glVertex3f(oVertArray[i - 1].first + pos[0], oVertArray[i - 1].second + pos[1], pos[2]);

    glColor3f(oColorArray[i].first, oColorArray[i].second, 0);
    glVertex3f(oVertArray[i].first + pos[0], oVertArray[i].second + pos[1], pos[2]);
  }
  
  glColor3f(oColorArray[edge_count - 1].first, oColorArray[edge_count - 1].second, 0);
  glVertex3f(oVertArray[edge_count - 1].first + pos[0], oVertArray[edge_count - 1].second + pos[1], pos[2]);
  
  glColor3f(oColorArray[0].first, oColorArray[0].second, 0);
  glVertex3f(oVertArray[0].first + pos[0], oVertArray[0].second + pos[1], pos[2]);

  glEnd();

}


void DrawPlane(const float width, const float height, 
               const float x_pos, const float y_pos, const float z_pos,
               const float x_dir, const float y_dir, const float z_dir,
               const uint32_t texture_id) {

	
  
  //fprintf(stderr, "DrawPlane: texture_id = %u\n", texture_id);

  glColor3f(1, 1, 1);
  glBindTexture (GL_TEXTURE_2D, texture_id);
  glBegin(GL_QUADS);
  
  glTexCoord2f (0, 0);
  //glNormal3f(0, 1, 0);
  glVertex3f(x_pos, y_pos, z_pos);
  
  glTexCoord2f (1, 0);
  //glNormal3f(0, 1, 0);
  glVertex3f(x_dir * width + x_pos, y_pos, z_dir * height + z_pos);
  
  glTexCoord2f (1, 1);
  //glNormal3f(0, 1, 0);
  glVertex3f(x_dir * width + x_pos, y_dir * height + y_pos, z_dir * height + z_pos);
  
  glTexCoord2f (0, 1);
  //glNormal3f(0, 1, 0);
  glVertex3f(x_pos, y_dir * height + y_pos, z_pos);

  glEnd();
  //glFlush();
  

}

} //namespace HELPERS

} //namespace SE

