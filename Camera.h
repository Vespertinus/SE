

#ifndef __CAMERA_H__
#define __CAMERA_H__ 1

namespace SE {

struct CamSettings_t {

	int32_t width,
					height;
	float 	fov;

};

class Camera {

	float		pos_x,
					pos_y,
					pos_z,
					rot_x,
					rot_y,
					rot_z;


	CamSettings_t oSettings;


	public:
	template <class TTranspose> Camera(TTranspose & oTranspose, const CamSettings_t & oNewSettings);
	~Camera() throw();

	void UpdateSettings(const CamSettings_t & oNewSettings);
	void Adjust();
	template <class TTranspose> void UpdateTransposer(TTranspose & oTranspose);
	void UpdateDimension(const int32_t new_width, const int32_t new_height);
	//cam effects, zoom, aberation, etc, noise
	int32_t GetWidth() const;
	int32_t GetHeight() const;
	

};

} //namespace SE

#include <Camera.tcc>

#endif
