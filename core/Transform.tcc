
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace SE  {

Transform::Transform() :
        Transform(glm::vec3(0), glm::vec3(0), glm::vec3(1)) {
}

Transform::Transform(const glm::vec3 & pos, const glm::vec3 & rotation, const glm::vec3 & new_scale) :
        pParent(nullptr),
        vTranslation(pos),
        vRotation(rotation),
        vScale(new_scale),
        local_dirty(1),
        world_dirty(0) {

                Recalc();

}

void Transform::Recalc() const {

        if (!local_dirty) { return; }

        //TODO rewrite

        /*
        mTransform  = glm::translate(glm::mat4(1.0), vTranslation);
        mTransform  = glm::scale (mTransform, vScale);
        mTransform  = glm::rotate(mTransform, glm::radians(vRotation.x), glm::vec3( 1, 0, 0) );
        mTransform  = glm::rotate(mTransform, glm::radians(vRotation.y), glm::vec3( 0, 1, 0) );
        mTransform  = glm::rotate(mTransform, glm::radians(vRotation.z), glm::vec3( 0, 0, 1) );
        */

        glm::mat4 mTranslate    = glm::translate(glm::mat4(1.0), vTranslation);
        glm::mat4 mScale        = glm::scale (glm::mat4(1.0), vScale);
        glm::quat oQuat(glm::vec3(glm::radians(vRotation.x), glm::radians(vRotation.y), glm::radians(vRotation.z)));
        glm::mat4 mRotation     = glm::toMat4(oQuat);

        mTransform  = mTranslate * mRotation * mScale;

        local_dirty = 0;
}

void Transform::RecalcWorld() const {

        if (!local_dirty && !world_dirty) { return; }

        Recalc();

        if (!pParent) {
                mWorldTransform = mTransform;
        }
        else {
                mWorldTransform = pParent->GetWorld() * mTransform;
        }
        world_dirty = 0;
}

void Transform::Set(const glm::vec3 & pos, const glm::vec3 & rotation, const glm::vec3 & new_scale) {

        vTranslation    = pos;
        vRotation       = rotation;
        vScale          = new_scale;
        local_dirty     = 1;
}

void Transform::SetPos(const glm::vec3 & vPos) {
        vTranslation    = vPos;
        local_dirty     = 1;
}

void Transform::SetRotation(const glm::vec3 & vDegreeAngles) {

        vRotation = vDegreeAngles;
        local_dirty = 1;
}

void Transform::SetScale(const glm::vec3 & new_scale) {

        vScale = new_scale;
        local_dirty = 1;
}

const glm::mat4 & Transform::Get() const {

        Recalc();
        /*
        log_d("pos x = {}, y = {}, z = {}, rot x = {}, y = {}, z = {}, scale = {}",
                        vTranslation.x,
                        vTranslation.y,
                        vTranslation.z,
                        vRotation.x,
                        vRotation.y,
                        vRotation.z,
                        scale);
        */
        return mTransform;
}

const glm::mat4 & Transform::GetWorld() const {

        RecalcWorld();
        return mWorldTransform;
}

void Transform::SetParent(Transform * pNode) {

        pParent         = pNode;
        world_dirty     = 1;
}

void Transform::Invalidate() {
        world_dirty = 1;
}

void Transform::Print(const size_t indent) {

        log_d("{:>{}} pos ({}, {}, {}), rot ({}, {}, {}), scale ({}, {}, {}), parent = {:p}",
                        ">",
                        indent,
                        vTranslation.x,
                        vTranslation.y,
                        vTranslation.z,
                        vRotation.x,
                        vRotation.y,
                        vRotation.z,
                        vScale.x,
                        vScale.y,
                        vScale.z,
                        (void *)pParent);
}


}
