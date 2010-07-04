
#include <math.h>

namespace SE {


template <class TTranspose> Camera::Camera(TTranspose & oTranspose, const CamSettings_t & oNewSettings) :
	pos_x(0),
	pos_y(0),
	pos_z(0),
	rot_x(100),
	rot_y(0),
	rot_z(270),
  delta_x(0),
  delta_y(0),
  delta_z(0),
	oSettings(oNewSettings) {

	oTranspose.ReInit(&pos_x, &pos_y, &pos_z, 
                    &rot_x, &rot_y, &rot_z, 
                    &delta_x, &delta_y, &delta_z, 
                    &oSettings.width, 
                    &oSettings.height);
}



Camera::~Camera() throw() { ;; }



void Camera::Adjust() {
  
  pos_x += delta_x;
  pos_y += delta_y;
  pos_z += delta_z;
		
	glRotated (-rot_x, 1.0, 0.0, 0.0);
	glRotated ( rot_z, 0.0, 0.0, 1.0);

	glTranslatef (-pos_x, -pos_y, -pos_z);
}



void Camera::UpdateSettings(const CamSettings_t & oNewSettings) { oSettings = oNewSettings; }



template <class TTranspose> void Camera::UpdateTransposer(TTranspose & oTranspose) {

	oTranspose.ReInit(&pos_x, &pos_y, &pos_z, 
                    &rot_x, &rot_y, &rot_z, 
                    &delta_x, &delta_y, &delta_z, 
                    &oSettings.width, 
                    &oSettings.height);
}



void Camera::UpdateDimension(const int32_t new_width, const int32_t new_height) {

	//oSettings.height	= (new_height) ? new_height : 1;
	
	oSettings.width 	= new_width;
	oSettings.height 	= new_height;
}

int32_t Camera::GetWidth() const { return oSettings.width; }

int32_t Camera::GetHeight() const { return oSettings.height; }


void Camera::SetPos(const float new_x, const float new_y, const float new_z) {

  pos_x = new_x; 
  pos_y = new_x;
  pos_z = new_z;
}

void Camera::LookAt(const float x, const float y, const float z) {
/*
  float sx = sinf(rot_x);
  float cx = cosf(rot_x);
  float sy = sinf(rot_y);
  float cy = cosf(rot_y);
  
  float sz = sinf(rot_z);
  float cz = cosf(rot_z);

  float res_x =  sy*cx;
  float res_y = -sx;
  float res_z =  cy*cx;
*/
  //directionVector.Set(x,y,z);

  float res_x,
        res_y,
        res_z;

  res_x = x - pos_x;
  res_y = y - pos_y;
  res_z = z - pos_z;
  
/*  
  res_x = pos_x - x;
  res_y = pos_y - y;
  res_z = pos_z - z;
*/  
  fprintf(stderr, "Camera::LookAt: res_x = %f, res_y = %f, res_z = %f\n", res_x, res_y, res_z);

  //rot_x = (float)atan2(-res_y, sqrt(res_x * res_x + res_z * res_z));
  //rot_z = (float)atan2(res_x, res_z);
  
  rot_x += (float)atan2(-res_y, sqrt(res_x * res_x + res_z * res_z)) * 180 / M_PI;
  rot_z += (float)atan2(res_x, res_z) * 180 / M_PI;
  //rot_z = 0;
}

} //namespace SE

