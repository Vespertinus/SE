

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


#endif
