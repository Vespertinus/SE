
namespace SE {

Skeleton::Skeleton(
                const std::string & sName,
                const rid_t new_rid,
                const SE::FlatBuffers::Skeleton * pSkeleton) :
        ResourceHolder(new_rid, sName) {

        uint32_t joints_cnt = pSkeleton->joints()->Length();
        se_assert(joints_cnt <= 255);
        vJoints.reserve(joints_cnt);

        for (uint32_t i = 0; i < joints_cnt; ++i) {
                auto pCurJoint = pSkeleton->joints()->Get(i);
                vJoints.emplace_back(Joint{
                                pCurJoint->name()->c_str(),
                                *reinterpret_cast<const glm::quat *>(pCurJoint->bind_rot()),
                                *reinterpret_cast<const glm::vec3 *>(pCurJoint->bind_pos()),
                                *reinterpret_cast<const glm::vec3 *>(pCurJoint->bind_scale()),
                                pCurJoint->parent_index()
                                }
                                );
        }

        log_d("name: '{}', joints cnt: {}", sName, vJoints.size());
}

Skeleton::Skeleton(const std::string & sName, const rid_t new_rid) : ResourceHolder(new_rid, sName) {

        //TODO load from file
        log_e("unimplemented yet");
        abort();
}

uint8_t Skeleton::GetJointsCnt() const {
        return vJoints.size();
}

const std::vector<Joint> & Skeleton::Joints() const {
        return vJoints;
}

}
