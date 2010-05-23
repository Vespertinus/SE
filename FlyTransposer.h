
#ifndef __FLY_TRANSPOSER_H__
#define __FLY_TRANSPOSER_H__ 1

namespace SE {

class FlyTransposer : public OIS::KeyListener, public OIS::MouseListener {

	float   * pos_x,
			 		* pos_y,
			 		* pos_z,
			 		* rot_x,
			 		* rot_y,
			 		* rot_z;
	
	int32_t		cursor_x,
						cursor_y;

	float 		speed;
	float			speed_mul;

	int32_t	* width;
	int32_t	* height;


	public:

	FlyTransposer();
	~FlyTransposer() throw();

	void ReInit(float * new_pos_x, float * new_pos_y, float * new_poz_z, 
							float * new_rot_x, float * new_rot_y, float * new_rot_z,
							int32_t * new_width, int32_t * new_height);

	//TODO rewrite input. At this time process only one button by the frame!!!
	void Operate(unsigned char key, const int x, const int y); //FIXME remove old code..


  bool keyPressed( const OIS::KeyEvent &ev);
  bool keyReleased( const OIS::KeyEvent &ev);

  bool mouseMoved( const OIS::MouseEvent &ev);
  bool mousePressed( const OIS::MouseEvent &ev, OIS::MouseButtonID id);
  bool mouseReleased( const OIS::MouseEvent &ev, OIS::MouseButtonID id);


};

} // namespace SE

#include <FlyTransposer.tcc>

#endif
