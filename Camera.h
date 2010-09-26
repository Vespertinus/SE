

#ifndef __CAMERA_H__
#define __CAMERA_H__ 1

namespace SE {


class Camera {

  public:

  static const uint8_t uPERSPECTIVE   = 1;
  static const uint8_t uORTHO         = 2;

  struct CamSettings_t {

    int32_t   width,
              height;
    float 	  fov;
    float 		near_clip;
    float			far_clip;
    uint8_t   projection;

  };

  private:

	float		pos_x,
					pos_y,
					pos_z,
					rot_x,
					rot_y,
					rot_z;
  float   delta_x,
          delta_y,
          delta_z;


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
	
  void SetPos(const float new_x, const float new_y, const float new_z);
  void LookAt(const float x, const float y, const float z);

  void UpdateProjection() const;

};

} //namespace SE

#include <Camera.tcc>

#endif
