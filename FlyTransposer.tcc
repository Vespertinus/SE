
#include <math.h>



namespace SE {

FlyTransposer::FlyTransposer() : pos_x(0), pos_y(0), pos_z(0), 
                               rot_x(0), rot_y(0), rot_z(0), 
                               delta_x(0), delta_y(0), delta_z(0),
															 cursor_x(0), cursor_y(0), 
															 speed(0.001), speed_mul(1),
															 width(0), height (0),
                               w_x_delta(0), w_y_delta(0),
                               s_x_delta(0), s_y_delta(0),
                               a_x_delta(0), a_y_delta(0),
                               d_x_delta(0), d_y_delta(0) { ;; }

FlyTransposer::~FlyTransposer() throw() { ;; }



void FlyTransposer::ReInit(float   * new_pos_x, float   * new_pos_y, float * new_pos_z, 
													 float   * new_rot_x, float   * new_rot_y, float * new_rot_z,
                           float   * new_delta_x, float * new_delta_y, float * new_dela_z,
													 int32_t * new_width, int32_t * new_height) {

	pos_x = new_pos_x;
	pos_y = new_pos_y;
	pos_z = new_pos_z;

	rot_x = new_rot_x;
	rot_y = new_rot_y;
	rot_z = new_rot_z;

  delta_x = new_delta_x;
  delta_y = new_delta_y;
  delta_z = new_dela_z;

	width 	= new_width;
	height 	= new_height;
}



bool FlyTransposer::keyPressed( const OIS::KeyEvent &ev) {

	switch (ev.key) {

    case OIS::KC_W:
			w_x_delta = speed * 1.35 * sin (*rot_z / 180.0 * M_PI);
			w_y_delta = speed * 1.35 * cos (*rot_z / 180.0 * M_PI);

      *delta_x += w_x_delta;
      *delta_y += w_y_delta;
			break;
    case OIS::KC_S:	
			s_x_delta = speed * 1.35 * sin (*rot_z / 180.0 * M_PI);
			s_y_delta = speed * 1.35 * cos (*rot_z / 180.0 * M_PI);

      *delta_x -= s_x_delta;
      *delta_y -= s_y_delta;
			break;
    case OIS::KC_D:	
			d_x_delta = speed * 1.35 * sin ( (*rot_z + 90.0) / 180.0 * M_PI);
			d_y_delta = speed * 1.35 * cos ( (*rot_z + 90.0) / 180.0 * M_PI);

      *delta_x += d_x_delta;
      *delta_y += d_y_delta;
			break;
    case OIS::KC_A:
			a_x_delta = speed * 1.35 * sin ( (*rot_z - 90.0) / 180.0 * M_PI);
			a_y_delta = speed * 1.35 * cos ( (*rot_z - 90.0) / 180.0 * M_PI);

      *delta_x += a_x_delta;
      *delta_y += a_y_delta;
			break;
    case OIS::KC_R:
			*delta_z	+=	speed;
			break;
    case OIS::KC_F:
			*delta_z	-=	speed;
			break;
    case OIS::KC_ESCAPE:
    case OIS::KC_Q:
      fprintf(stderr, "FlyTransposer::keyPressed: stop program\n");
      exit(0);
      //return false;
      //TODO write correct exit

    case OIS::KC_C:
             {
             
               fprintf(stderr, "checkpoint:\n"
                   "pos_x = %f, pos_y = %f, pos_z = %f\n"
                   "rot_x = %f, rot_y = %f, rot_z = %f\n"
                   "last x = %u, last y = %u\n\n",
                   *pos_x,
                   *pos_y,
                   *pos_z,
                   *rot_x,
                   *rot_y,
                   *rot_z,
                   cursor_x,
                   cursor_y);
             }
             break;
    default:
      break;
      
	}
  return true;
}



bool FlyTransposer::keyReleased( const OIS::KeyEvent &ev) {

	switch (ev.key) {

    case OIS::KC_W:
			*delta_x -= w_x_delta; 
			*delta_y -= w_y_delta;
			break;
    case OIS::KC_S:	
			*delta_x += s_x_delta;
			*delta_y += s_y_delta;
			break;
    case OIS::KC_D:	
			*delta_x -= d_x_delta;
			*delta_y -= d_y_delta;
			break;
    case OIS::KC_A:
			*delta_x -= a_x_delta;
			*delta_y -= a_y_delta;
			break;
    case OIS::KC_R:
			*delta_z	-=	speed;
			break;
    case OIS::KC_F:
			*delta_z	+=	speed;
			break;

    default:
      break;
      
	}

  return true;
}



bool FlyTransposer::mouseMoved( const OIS::MouseEvent &ev) {
/*
  fprintf(stderr, "FlyTransposer::mouseMoved:\n"
                  "rot_x = %f, rot_y = %f, rot_z = %f\n"
                  "mouse x = %u, mouse y = %u\n"
                  "last x = %u, last y = %u\n\n",
      *rot_x,
      *rot_y,
      *rot_z,
      ev.state.X.abs,
      ev.state.Y.abs,
      cursor_x,
      cursor_y);
*/
	*rot_x += (cursor_y - ev.state.Y.abs) * 0.05;
	*rot_z -= (cursor_x - ev.state.X.abs) * 0.05;

	if (*rot_x < 0.0) 	*rot_x  = 0.0;
	if (*rot_x > 180.0) *rot_x	= 180.0;

	if (*rot_z < 0.0) 	*rot_z 	= 359.0;
	if (*rot_z > 359.0) *rot_z 	= 0.0;

	cursor_x = ev.state.X.abs;
	cursor_y = ev.state.Y.abs;

  if (ev.state.X.abs >= *width - 1.0) {

    //ev.state.X.abs = 1.0;
    //SetCursorPos(ev.state.X.abs,ev.state.Y.abs);
    OIS::MouseState &MutableMouseState = const_cast<OIS::MouseState &>(TInputManager::Instance().GetMouse()->getMouseState());
    MutableMouseState.X.abs = 1;
    cursor_x                = 1;
    //fprintf(stderr, "right\n");
  }

  if (ev.state.Y.abs >= *height - 1.0) {
    //ev.state.Y.abs = 1.0;
    //SetCursorPos (ev.state.X.abs, ev.state.Y.abs);
    OIS::MouseState &MutableMouseState = const_cast<OIS::MouseState &>(TInputManager::Instance().GetMouse()->getMouseState());
    MutableMouseState.Y.abs = 1;
    cursor_y                = 1;
    //fprintf(stderr, "down\n");
  }
  if (ev.state.X.abs == 0.0) {
    //ev.state.X.abs = *width;
    //SetCursorPos (ev.state.X.abs, ev.state.Y.abs);
    OIS::MouseState &MutableMouseState = const_cast<OIS::MouseState &>(TInputManager::Instance().GetMouse()->getMouseState());
    MutableMouseState.X.abs = *width;
    cursor_x                = *width;
    //fprintf(stderr, "left\n");
  }

  if (ev.state.Y.abs == 0.0) {
    //ev.state.Y.abs = *height;
    //SetCursorPos(ev.state.X.abs,ev.state.Y.abs);
    OIS::MouseState &MutableMouseState = const_cast<OIS::MouseState &>(TInputManager::Instance().GetMouse()->getMouseState());
    MutableMouseState.Y.abs = *height;
    cursor_y                = *height;
    //fprintf(stderr, "up\n");
  }

  return true;
}



bool FlyTransposer::mousePressed( const OIS::MouseEvent &ev, OIS::MouseButtonID id) {

  return true;
}



bool FlyTransposer::mouseReleased( const OIS::MouseEvent &ev, OIS::MouseButtonID id) {
  
  return true;
}

} // namespace SE
