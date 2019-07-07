
#include <math.h>

namespace SE {

Camera::Volume::Volume(
                const Projection proj,
                const float near,
                const float far,
                const float new_aspect,
                const float new_fov
                ) :
        near_clip(near),
        far_clip(far),
        aspect(new_aspect),
        fov(new_fov),
        projection(proj) {

}

Camera::Camera(TSceneTree::TSceneNodeExact * pNewNode, bool enabled) : pNode(pNewNode) {

        if (enabled) {
                Enable();
        }
}

Camera::Camera(TSceneTree::TSceneNodeExact * pNewNode, bool enabled, const Settings & oSettings) :
        pNode(pNewNode),
        //view_size(oSettings.view_size),
        //up(oSettings.up),
        oVolume(
                        oSettings.projection,
                        oSettings.near_clip,
                        oSettings.far_clip,
                        (float)view_size.x / (float)view_size.y,
                        oSettings.fov
               ) {

        if (enabled) {
                Enable();
        }
}

Camera::~Camera() noexcept {

        Disable();
}

void Camera::RecalcVolume() {

        if (!(flags & Dirty::VOLUME)) { return; }

        switch (oVolume.projection) {
                case Projection::PERSPECTIVE:

                        oVolume.top    = oVolume.near_clip  * tanf(oVolume.fov * M_PI / 360.0); //TODO rewrite to internal math
                        oVolume.bottom = - oVolume.top;
                        oVolume.right  = oVolume.top * oVolume.aspect;
                        oVolume.left   = oVolume.bottom * oVolume.aspect;
                        break;

                case Projection::ORTHO:
                        oVolume.left   = -view_size.x / 2;
                        oVolume.right  =  view_size.x / 2;
                        oVolume.bottom = -view_size.y / 2;
                        oVolume.top    =  view_size.y / 2;
                        break;
                default:
                        log_e("wrong projection = {}", static_cast<uint8_t>(oVolume.projection));
                        abort();
        }

        flags ^= Dirty::VOLUME;
        flags |= Dirty::PROJECTION;
}

void Camera::RecalcProjection() {

        RecalcVolume();

        if (!(flags & Dirty::PROJECTION)) { return; }

        switch (oVolume.projection) {

                case Projection::PERSPECTIVE:

                        mProjection = glm::frustum(
                                        oVolume.left,
                                        oVolume.right,
                                        oVolume.bottom,
                                        oVolume.top,
                                        oVolume.near_clip,
                                        oVolume.far_clip);
                        break;

                case Projection::ORTHO:
                        UpdateZoom();
                        mProjection = glm::ortho (
                                        oVolume.left / zoom,
                                        oVolume.right / zoom,
                                        oVolume.bottom / zoom,
                                        oVolume.top / zoom,
                                        oVolume.near_clip,
                                        oVolume.far_clip);
                        break;

                default:
                        log_e("wrong projection = {}", static_cast<uint8_t>(oVolume.projection));
                        abort();
        }

        flags ^= Dirty::PROJECTION;
}

const glm::mat4 & Camera::GetWorldMVP() {

        RecalcProjection();

        //View matrix -> inverse world
        //THINK always reset scale to 1
        mModelViewProjection = mProjection * glm::inverse(pNode->GetTransform().GetWorld());
        return mModelViewProjection;
}

void Camera::UpdateZoom() {

        if (!(flags & Dirty::ZOOM) || target_length <= 0) { return; }

        float min_dim = std::min(view_size.x, view_size.y);
        zoom = min_dim / target_length;

        log_d("min_dim = {}, target_len = {}, zoom = {}", min_dim, target_length, zoom);

        flags ^= Dirty::ZOOM;
        flags |= Dirty::PROJECTION;
}

void Camera::SetZoom(const float new_zoom) {

        zoom = new_zoom;
        float min_dim = std::min(view_size.x, view_size.y);
        target_length = min_dim / zoom;

        flags ^= Dirty::ZOOM;
        flags |= Dirty::PROJECTION;
}


void Camera::Zoom(const float factor) {
        target_length /= factor;
        flags |= Dirty::ZOOM;
}

void Camera::ZoomTo(const BoundingBox & oBBox) {

        glm::vec3 len = oBBox.Size();
/*
        const glm::vec3 & min = oBBox.Min();
        const glm::vec3 & max = oBBox.Max();

        log_d("min x = {}, y = {}, z = {}, max x = {}, y = {}, z = {}",
                        min.x, min.y, min.z,
                        max.x, max.y, max.z);

        log_d("bbox len = {}", glm::distance(max, min) );
*/

        target_length = std::max({ std::abs(len.x), std::abs(len.y), std::abs(len.z) });

        flags |= Dirty::ZOOM;
}

void Camera::ZoomTo(const float width) {

        target_length = width;
        flags |= Dirty::ZOOM;
}

void Camera::UpdateDimension(const glm::uvec2 new_view_size) {

        view_size       = new_view_size;
        oVolume.aspect  = (float)view_size.x / (float)view_size.y;

        flags |= Dirty::ZOOM | Dirty::VOLUME;
}

void Camera::UpdateDimension(const uint32_t new_width, const uint32_t new_height) {

        UpdateDimension(glm::uvec2(new_width, new_height));
}

void Camera::LookAt(const float x, const float y, const float z) {
        LookAt(glm::vec3(x, y, z));
}

void Camera::LookAt(const glm::vec3 & center) {

        pNode->WorldLookAt(center);
        //pNode->LookAt(center);
}

void Camera::SetPos(const float new_x, const float new_y, const float new_z) {

        pNode->SetWorldPos(glm::vec3(new_x, new_y, new_z));
}

void Camera::SetRotation(const float new_x, const float new_y, const float new_z) {

        pNode->SetWorldRotation(glm::vec3(new_x, new_y, new_z));
}

void Camera::SetFOV(const float new_fov) {
        oVolume.fov = new_fov;
        flags |= Dirty::VOLUME;
}

std::string Camera::Str() const {

        return fmt::format("Camera: projection: {}, fov, {}, aspect: {}, zoom: {}, target_length: {}, near: {}, far: {}",
                        static_cast<uint8_t>(oVolume.projection),
                        oVolume.fov,
                        oVolume.aspect,
                        zoom,
                        target_length,
                        oVolume.near_clip,
                        oVolume.far_clip
                        );
}

void Camera::SetProjection(const Projection proj) {

        if (oVolume.projection == proj) { return; }
        if (proj > Projection::ORTHO) {
                log_e("unknown projection: {}", static_cast<uint8_t>(proj));
                return;
        }

        oVolume.projection = proj;
        flags |= Dirty::PROJECTION;
}

//THINK
void Camera::Enable() { ;; }
void Camera::Disable() { ;; }

} //namhespace SE

