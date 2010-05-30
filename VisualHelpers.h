

#ifndef __VISUAL_HELPERS_H__
#define __VISUAL_HELPERS_H__ 1

void DrawAxes(const uint32_t size) {

  glColor3f(1, 0, 0);
  glBegin(GL_LINES);
  glVertex3f(0, 0, 0);
  glVertex3f(size, 0, 0);
  glEnd();

  glColor3f(0, 1, 0);
  glBegin(GL_LINES);
  glVertex3f(0, 0, 0);
  glVertex3f(size, 0, 0);
  glEnd();

  glColor3f(0, 0, 1);
  glBegin(GL_LINES);
  glVertex3f(0, 0, 0);
  glVertex3f(size, 0, 0);
  glEnd();

}


#endif
