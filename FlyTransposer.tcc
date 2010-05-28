
#include <math.h>



namespace SE {

FlyTransposer::FlyTransposer() : pos_x(0), pos_y(0), pos_z(0), rot_x(0), rot_y(0), rot_z(0), 
															 cursor_x(0), cursor_y(0), 
															 speed(0.5), speed_mul(1),
															 width(0), height (0) { ;; }

FlyTransposer::~FlyTransposer() throw() { ;; }



void FlyTransposer::ReInit(float   * new_pos_x, float   * new_pos_y, float * new_pos_z, 
													 float   * new_rot_x, float   * new_rot_y, float * new_rot_z,
													 int32_t * new_width, int32_t * new_height) {

	pos_x = new_pos_x;
	pos_y = new_pos_y;
	pos_z = new_pos_z;

	rot_x = new_rot_x;
	rot_y = new_rot_y;
	rot_z = new_rot_z;

	width 	= new_width;
	height 	= new_height;
}


	
void FlyTransposer::Operate(unsigned char key, const int x, const int y) {

	*rot_x += (cursor_y - y) * 0.05;
	*rot_z -= (cursor_x - x) * 0.05;

	if (*rot_x < 0.0) 	*rot_x  = 0.0;
	if (*rot_x > 180.0) *rot_x	= 180.0;

	if (*rot_z < 0.0) 	*rot_z 	= 359.0;
	if (*rot_z > 359.0) *rot_z 	= 0.0;


	switch (key) {

		case 'W':
			*pos_x += speed * 1.35 * sin (*rot_z / 180.0 * M_PI);
			*pos_y += speed * 1.35 * cos (*rot_z / 180.0 * M_PI);
			break;
		case 'S':	
			*pos_x -= speed * 1.35 * sin (*rot_z / 180.0 * M_PI);
			*pos_y -= speed * 1.35 * cos (*rot_z / 180.0 * M_PI);
			break;
		case 'D':	
			*pos_x += speed * 1.35 * sin ( (*rot_z + 90.0) / 180.0 * M_PI);
			*pos_y += speed * 1.35 * cos ( (*rot_z + 90.0) / 180.0 * M_PI);
			break;
		case 'A':
			*pos_x += speed * 1.35 * sin ( (*rot_z - 90.0) / 180.0 * M_PI);
			*pos_y += speed * 1.35 * cos ( (*rot_z - 90.0) / 180.0 * M_PI);
			break;
		case 'R':
			*pos_z	+=	speed;
			break;
		case 'F':
			*pos_z	-=	speed;
			break;
	}
/*
	if (x == ViewWidth - 1.0)
	{
		ViewPoint.x = 1.0;
		SetCursorPos(ViewPoint.x,ViewPoint.y);
	}
	if (ViewPoint.y == ViewHeight - 1.0)
	{
		ViewPoint.y = 1.0;
		SetCursorPos (ViewPoint.x, ViewPoint.y);
	}
	if (ViewPoint.x == 0.0)
	{
		ViewPoint.x = ViewWidth;
		SetCursorPos (ViewPoint.x, ViewPoint.y);
	}

	if (ViewPoint.y == 0.0)
	{
		ViewPoint.y = ViewHeight;
		SetCursorPos(ViewPoint.x,ViewPoint.y);
	}
*/

	cursor_x = x;
	cursor_y = y;
}



bool FlyTransposer::keyPressed( const OIS::KeyEvent &ev) {

	switch (ev.key) {

    case OIS::KC_W:
			*pos_x += speed * 1.35 * sin (*rot_z / 180.0 * M_PI);
			*pos_y += speed * 1.35 * cos (*rot_z / 180.0 * M_PI);
			break;
    case OIS::KC_S:	
			*pos_x -= speed * 1.35 * sin (*rot_z / 180.0 * M_PI);
			*pos_y -= speed * 1.35 * cos (*rot_z / 180.0 * M_PI);
			break;
    case OIS::KC_D:	
			*pos_x += speed * 1.35 * sin ( (*rot_z + 90.0) / 180.0 * M_PI);
			*pos_y += speed * 1.35 * cos ( (*rot_z + 90.0) / 180.0 * M_PI);
			break;
    case OIS::KC_A:
			*pos_x += speed * 1.35 * sin ( (*rot_z - 90.0) / 180.0 * M_PI);
			*pos_y += speed * 1.35 * cos ( (*rot_z - 90.0) / 180.0 * M_PI);
			break;
    case OIS::KC_R:
			*pos_z	+=	speed;
			break;
    case OIS::KC_F:
			*pos_z	-=	speed;
			break;
    case OIS::KC_ESCAPE:
    case OIS::KC_Q:
      fprintf(stderr, "FlyTransposer::keyPressed: stop program\n");
      exit(0);
      //return false;
      //TODO write correct exit

    default:
      break;
      
	}
  return true;
}



bool FlyTransposer::keyReleased( const OIS::KeyEvent &ev) {


  return true;
}



bool FlyTransposer::mouseMoved( const OIS::MouseEvent &ev) {

	*rot_x += (cursor_y - ev.state.Y.abs) * 0.05;
	*rot_z -= (cursor_x - ev.state.X.abs) * 0.05;

	if (*rot_x < 0.0) 	*rot_x  = 0.0;
	if (*rot_x > 180.0) *rot_x	= 180.0;

	if (*rot_z < 0.0) 	*rot_z 	= 359.0;
	if (*rot_z > 359.0) *rot_z 	= 0.0;

	cursor_x = ev.state.X.abs;
	cursor_y = ev.state.Y.abs;

  return true;
}



bool FlyTransposer::mousePressed( const OIS::MouseEvent &ev, OIS::MouseButtonID id) {

  return true;
}



bool FlyTransposer::mouseReleased( const OIS::MouseEvent &ev, OIS::MouseButtonID id) {
  
  return true;
}

} // namespace SE
