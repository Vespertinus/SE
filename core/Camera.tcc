
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
        zoom(1),
        target_length(0),
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

        /*
        //DEBUG
        float mat[16];
        float angle_x,
              angle_y,
              angle_z;
        glGetFloatv(GL_MODELVIEW_MATRIX, mat);

        printf("Camera::Adjust: current model matrix:\n");
        printf("m0(%f) m4(%f) m8 (%f) m12(%f)\n", mat[0], mat[4], mat[8],  mat[12]);
        printf("m1(%f) m5(%f) m9 (%f) m13(%f)\n", mat[1], mat[5], mat[9],  mat[13]);
        printf("m2(%f) m6(%f) m10(%f) m14(%f)\n", mat[2], mat[6], mat[10], mat[14]);
        printf("m3(%f) m7(%f) m11(%f) m15(%f)\n", mat[3], mat[7], mat[11], mat[15]);

        printf("real angles: x = %f, y = %f, z = %f\n", rot_x, rot_y, rot_z);

        glm::mat4 mModel        = glm::translate(glm::mat4(1.0), glm::vec3(-pos_x, -pos_y, -pos_z));
        mModel                  = glm::rotate(mModel, glm::radians(rot_x), glm::vec3(-1, 0, 0) );
        mModel                  = glm::rotate(mModel, glm::radians(rot_y), glm::vec3( 0, 1, 0) );
        mModel                  = glm::rotate(mModel, glm::radians(rot_z), glm::vec3( 0, 0, 1) );

        const float * new_mat = (const float *)glm::value_ptr(mModel);

        printf("Camera::Adjust: calc model matrix:\n");
        printf("m0(%f) m4(%f) m8 (%f) m12(%f)\n", new_mat[0], new_mat[4], new_mat[8],  new_mat[12]);
        printf("m1(%f) m5(%f) m9 (%f) m13(%f)\n", new_mat[1], new_mat[5], new_mat[9],  new_mat[13]);
        printf("m2(%f) m6(%f) m10(%f) m14(%f)\n", new_mat[2], new_mat[6], new_mat[10], new_mat[14]);
        printf("m3(%f) m7(%f) m11(%f) m15(%f)\n", new_mat[3], new_mat[7], new_mat[11], new_mat[15]);
        
        glm::extractEulerAngleXYZ(mModel, angle_x, angle_y, angle_z);        
        printf("calc angles: x = %f, y = %f, z = %f\n", - glm::degrees(angle_x), glm::degrees(angle_y), glm::degrees(angle_z));

        glm::mat4 mRealMat = glm::make_mat4(mat);

        const float * new_mat2 = (const float *)glm::value_ptr(mRealMat);

        printf("Camera::Adjust: real to glm matrix:\n");
        printf("m0(%f) m4(%f) m8 (%f) m12(%f)\n", new_mat2[0], new_mat2[4], new_mat2[8],  new_mat2[12]);
        printf("m1(%f) m5(%f) m9 (%f) m13(%f)\n", new_mat2[1], new_mat2[5], new_mat2[9],  new_mat2[13]);
        printf("m2(%f) m6(%f) m10(%f) m14(%f)\n", new_mat2[2], new_mat2[6], new_mat2[10], new_mat2[14]);
        printf("m3(%f) m7(%f) m11(%f) m15(%f)\n", new_mat2[3], new_mat2[7], new_mat2[11], new_mat2[15]);
        
        glm::extractEulerAngleXYZ(mRealMat, angle_x, angle_y, angle_z);
        printf("calc angles: x = %f, y = %f, z = %f\n", glm::degrees(angle_x), glm::degrees(angle_y), glm::degrees(angle_z));

        //CHECK: if rot_z < 0 --> rot_z = 360 + rot_z
        */
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

        UpdateZoom();
}



//int32_t Camera::GetWidth() const { return oSettings.width; }



//int32_t Camera::GetHeight() const { return oSettings.height; }



void Camera::SetPos(const float new_x, const float new_y, const float new_z) {

  pos_x = new_x; 
  pos_y = new_y;
  pos_z = new_z;
}



void Camera::LookAt(const float x, const float y, const float z) {

        LookAt(glm::vec3(x, y, z));
}


void Camera::LookAt(const glm::vec3 & center) {

        glm::mat4 mModel = glm::lookAt(
                        glm::vec3(pos_x, pos_y, pos_z),
                        center,
                        glm::vec3(oSettings.up[0], oSettings.up[1], oSettings.up[2]) );

        float angle_x,
              angle_y,
              angle_z;

        glm::extractEulerAngleXYZ(mModel, angle_x, angle_y, angle_z);
        
        rot_x = -glm::degrees(angle_x);
        rot_y = glm::degrees(angle_y);
        rot_z = glm::degrees(angle_z);
        if (rot_z < 0) {
                rot_z = 360 + rot_z;
        }

        printf("Camera::LookAt: new angles: rot_x = %f, rot_y = %f, rot_z = %f, center x = %f, y = %f, z = %f\n", 
                        rot_x, rot_y, rot_z, 
                        center.x, center.y, center.z);
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
                        //TODO calc zoom
                        //glOrtho(oVolume.left /* * scale */, oVolume.right /* * scale */, oVolume.bottom /* * scale*/, oVolume.top /* * scale */, oVolume.near_clip, oVolume.far_clip);
                        glOrtho ((- oSettings.width / 2) / zoom, 
                                 (oSettings.width / 2) / zoom, 
                                 (- oSettings.height / 2) / zoom, 
                                 (oSettings.height / 2) / zoom, 
                                 oVolume.near_clip, 
                                 oVolume.far_clip);
                        //glOrtho (0, oSettings.width, oSettings.height, 0, oVolume.near_clip, oVolume.far_clip);
                        break;

                default:
                        fprintf(stderr, "Camera::UpdateProjection: wrong projection = %u\n", oVolume.projection);

        }

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
}


void Camera::SetZoom(const float new_zoom) {
        zoom = new_zoom;
}


void Camera::Zoom(const float factor) {
        zoom *= factor;
}


void Camera::UpdateZoom() {

        if (target_length <= 0) { return; }

        float min_dim = std::min(oSettings.width, oSettings.height);
        zoom = min_dim / target_length;

        printf("Camera::UpdateZoom: min_dim = %f, target_len = %f, zoom = %f\n", min_dim, target_length, zoom);

}

void Camera::ZoomTo(const std::tuple<const glm::vec3 &, const glm::vec3 &> bbox) {

        glm::vec3 len = std::get<1>(bbox) - std::get<0>(bbox);
///*
        const glm::vec3 & min = std::get<0>(bbox);
        const glm::vec3 & max = std::get<1>(bbox);

        printf("Camera::ZoomTo: min x = %f, y = %f, z = %f, max x = %f, y = %f, z = %f\n",
                        min.x, min.y, min.z,
                        max.x, max.y, max.z);

        printf("Camera::ZoomTo: bbox len = %f\n", glm::distance(max, min) );
//*/
        
        target_length = std::max({ std::abs(len.x), std::abs(len.y), std::abs(len.z) });
        
        UpdateZoom();
}


} //namhespace SE

