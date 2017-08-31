
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
        oSettings((const BaseData &)oNewSettings),
        oFrustum(oNewSettings.oVolume) {

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
	glRotated ( rot_y, 0.0, 1.0, 0.0);
	glRotated ( rot_z, 0.0, 0.0, 1.0);

	glTranslatef (-pos_x, -pos_y, -pos_z);
}



void Camera::UpdateSettings(const CamSettings_t & oNewSettings) { 
        oSettings = (const BaseData &)oNewSettings;
        oFrustum.SetVolume(oNewSettings.oVolume);
}



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
        oFrustum.SetAspect((float)new_width / (float)new_height);
}



//int32_t Camera::GetWidth() const { return oSettings.width; }



//int32_t Camera::GetHeight() const { return oSettings.height; }



void Camera::SetPos(const float new_x, const float new_y, const float new_z) {

  pos_x = new_x; 
  pos_y = new_x;
  pos_z = new_z;
}



void Camera::LookAt(const float x, const float y, const float z) {

        gluLookAt(pos_x, pos_y, pos_z,
                  x, y, z,
                  oSettings.up[0], oSettings.up[1], oSettings.up[2]);
}


void Camera::UpdateProjection() const {

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        const Frustum::Volume & oVolume = oFrustum.GetVolume();

        switch (oVolume.projection) {

                case Frustum::uPERSPECTIVE:
                        /*
                        gluPerspective(oVolume.fov, 
                                       //(GLfloat)oSettings.width / (GLfloat) oSettings.height,
                                       oVolume.aspect,
                                       oVolume.near_clip,
                                       oVolume.far_clip);
                        */                                        

                        glFrustum(oVolume.left, oVolume.right, oVolume.bottom, oVolume.top, oVolume.near_clip, oVolume.far_clip);
                        break;

                case Frustum::uORTHO:
                        
                        glOrtho(oVolume.left, oVolume.right, oVolume.bottom, oVolume.top, oVolume.near_clip, oVolume.far_clip);
                        //glOrtho (0, oSettings.width, oSettings.height, 0, oVolume.near_clip, oVolume.far_clip);
                        break;

                default:
                        fprintf(stderr, "Camera::UpdateProjection: wrong projection = %u\n", oVolume.projection);

        }

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
}



} //namhespace SE

