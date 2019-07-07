
namespace SE {

Frustum::Frustum(const Volume & oNewVolume) : oVolume(oNewVolume) {

        RecalcVolume();
}
 
Frustum::~Frustum() throw() { ;; }


void Frustum::SetVolume(const Volume & oNewVolume) { 

        oVolume  = oNewVolume; 
        RecalcVolume();
}



void Frustum::RecalcVolume() {

        //if (oVolume.projection != uPERSPECTIVE || (oVolume.fov == 0)) { return; }

        if (oVolume.projection == Projection::PERSPECTIVE) {

                oVolume.top    = oVolume.near_clip  * tan(oVolume.fov * M_PI / 360.0); //TODO rewrite to internal math
                oVolume.bottom = - oVolume.top;
                oVolume.right  = oVolume.top * oVolume.aspect;
                oVolume.left   = oVolume.bottom * oVolume.aspect;
        }
        else {
                oVolume.top     =;


        
        }

 /* reverse:
 * new_fov     = math.atan(top / near_clip) / (math.pi / 360.0)
 * new_aspect  = left / top
 * */
        
}


/*
void Frustum::RecalcPersp() {


}
*/

const Frustum::Volume & Frustum::GetVolume() const { 
        return oVolume;
}

void Frustum::SetFOV(const float new_fov) {
        oVolume.fov = new_fov;
        RecalcVolume();
}

void Frustum::SetAspect(const float new_aspect) {
        oVolume.aspect = new_aspect;
        RecalcVolume();
}

} // namespace SE

