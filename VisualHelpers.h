

#ifndef __VISUAL_HELPERS_H__
#define __VISUAL_HELPERS_H__ 1

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



void DrawElipse(const float x_pos, const float y_pos, const float z_pos, const float radius, const uint32_t edge_count) {
  float alpha = 0;
  float delta = 360 / edge_count;
  float x, 
        y; 

	glColor3f(1, 0.5, 0.5);
  //glPointSize(15);
  glBegin(GL_LINES);
  for (uint32_t i = 0; i < edge_count; ++i) {
    alpha = delta * i;
    x = radius * cos(alpha);
    y = radius * sin(alpha);
    glVertex3f(x + x_pos, y + y_pos, z_pos);
/*
    fprintf(stderr, "alpha = %f, delta = %f, radius = %f, x = %f, y = %f\n",
           alpha,
           delta,
           radius,
           x, y);
*/        
  }

  glEnd();

}

#endif
