
namespace SE {

Frustum::Frustum(const Settings & oNewSettings) {

  projection = oNewSettings.projection;
  switch (projection) {
    
    case uORTHO:


    case uPERSPECTIVE:


      break

    default:
        throw("unknown perspective type"); //TODO write project exception 
  }



}
 


void Frustum::SetVolume(const & Volume oNewVolume) { 

  oVolume  = oNewVolume; 
  UpdateProjection();
}



void Frustum::RecalcVolume() {

  top    = near_clip  * tan(fov * M_PI / 360.0); //TODO rewrite to internal math
  bottom = -top;
  left   = top * aspect;
  right  = bottom * aspect;
}



void Frustum::RecalcPersp() {


}


} // namespace SE

