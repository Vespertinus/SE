
#ifndef __CAMERA_H__
#define __CAMERA_H__ 1

#include <BoundingBox.h>
#include <Frustum.h>

namespace SE {

/*
 TODO split on 3 parts:
 - Node
 - Camera component with frustum
 - Controller component
 */
class Camera {

        public:

        struct BaseData {
                int32_t     width,
                            height;
                uint8_t     up[3]; // x, y or z
        };

        struct CamSettings_t : BaseData {

                Frustum::Volume oVolume;
                /*
                   float       fov;
                   float       near_clip;
                   float	far_clip;
                   uint8_t     projection;
                   */

        };

        private:

	float	        pos_x,
                        pos_y,
                        pos_z,
                        rot_x,
                        rot_y,
                        rot_z;

        float           delta_x,
                        delta_y,
                        delta_z;
        float           zoom,
                        /** zooming target max size  */
                        target_length;

        //TODO move calculation to matrix
        glm::mat4       mModelView;
        glm::mat4       mModelViewProjection;

        BaseData        oSettings;
        Frustum         oFrustum;


        void UpdateZoom();

	public:

	template <class TTranspose> Camera(TTranspose & oTranspose, const CamSettings_t & oNewSettings);
	~Camera() throw();

	void UpdateSettings(const CamSettings_t & oNewSettings);
        const Frustum::Volume & GetVolume() const;
        const BaseData & GetSettings() const;
	void Adjust();
	template <class TTranspose> void UpdateTransposer(TTranspose & oTranspose);
	void UpdateDimension(const int32_t new_width, const int32_t new_height);
	//cam effects, zoom, aberation, etc, noise
//	int32_t GetWidth() const;
//	int32_t GetHeight() const;
	
        void SetPos(const float new_x, const float new_y, const float new_z);
        void SetRotation(const float new_x, const float new_y, const float new_z);
        void LookAt(const float x, const float y, const float z);
        void LookAt(const glm::vec3 & center);
        void SetZoom(const float new_zoom);
        void Zoom(const float factor);
        void ZoomTo(const BoundingBox & oBBox);
        void ZoomTo(const float width);
        const glm::mat4 & GetMVMatrix();
        const glm::mat4 & GetMVPMatrix();

};

} //namespace SE

#ifdef SE_IMPL
#include <Camera.tcc>
#endif

#endif
