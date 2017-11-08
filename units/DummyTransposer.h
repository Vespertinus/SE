
#ifndef __DUMMY_TRANSPOSER_H__
#define __DUMMY_TRANSPOSER_H__ 1

namespace SE {

class DummyTransposer {

        public:

        DummyTransposer();
        ~DummyTransposer() throw();

        void ReInit(float * new_pos_x, float * new_pos_y, float * new_poz_z, 
                    float * new_rot_x, float * new_rot_y, float * new_rot_z,
                    float * new_delta_x, float * new_delta_y, float * new_dela_z,
                    int32_t * new_width, int32_t * new_height);

};

} // namespace SE

#ifdef SE_IMPL
#include <DummyTransposer.tcc>
#endif

#endif

