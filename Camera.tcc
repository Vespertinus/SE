

namespace SE {


template <class TTranspose> Camera::Camera(TTranspose & oTranspose, const CamSettings_t & oNewSettings) :
	pos_x(0),
	pos_y(0),
	pos_z(0),
	rot_x(0),
	rot_y(0),
	rot_z(0),
	oSettings(oNewSettings) {

	oTranspose.ReInit(&pos_x, &pos_y, &pos_z, &rot_x, &rot_y, &rot_z, &oSettings.width, &oSettings.height);
}



Camera::~Camera() throw() { ;; }



void Camera::Adjust() {
		
	glRotated (-rot_x, 1.0, 0.0, 0.0);
	glRotated ( rot_z, 0.0, 0.0, 1.0);

	glTranslatef (-pos_x, -pos_y, -pos_z);
}



void Camera::UpdateSettings(const CamSettings_t & oNewSettings) { oSettings = oNewSettings; }



template <class TTranspose> void Camera::UpdateTransposer(TTranspose & oTranspose) {

	oTranspose.ReInit(&pos_x, &pos_y, &pos_z, &rot_x, &rot_y, &rot_z, &oSettings.width, &oSettings.height);
}



void Camera::UpdateDimension(const int32_t new_width, const int32_t new_height) {

	//oSettings.height	= (new_height) ? new_height : 1;
	
	oSettings.width 	= new_width;
	oSettings.height 	= new_height;
}

int32_t Camera::GetWidth() const { return oSettings.width; }

int32_t Camera::GetHeight() const { return oSettings.height; }

} //namespace SE

