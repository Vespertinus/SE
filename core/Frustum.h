
#ifndef __FRUSTUM_H_
#define __FRUSTUM_H_ 1

namespace SE {

class Frustum {

        public:
  
        static const uint8_t uPERSPECTIVE   = 1;
        static const uint8_t uORTHO         = 2;

        struct Volume {
                //options for each type of camera
                float   near_clip,
                        far_clip;

                float   left,
                        right,
                        top,
                        bottom;

                //perspective only
                float   aspect,
                        fov;
                
                uint8_t projection;
        };
/*
        struct Settings {
                Volume oVolume;
                uint8_t projection;
                Settings() : projection(uPERSPECTIVE) { ;; }
        };
*/
        protected:

        Volume    oVolume;
/*
        float     aspect;
        float     fov;
*/
//        uint8_t   projection;

        void RecalcVolume();
//        void RecalcPersp();


        //void            UpdateProjection();

        public:

        //Frustum();
//        Frustum(const Settings & oNewSettings);
        Frustum(const Volume & oNewVolume);
        ~Frustum() throw();

        void            SetFOV(const float new_fov);
//        void            SetFOV(const float new_fov, const float new_aspect);
//        float           GetFOV() const;
        void            SetAspect(const float new_aspect);
//        float           GetAspect() const;
        void            SetVolume(const Volume & oNewVolume);
//        void            SetVolume(const & Volume oNewVolume, const uint8_t new_projection);
        const Volume & GetVolume() const;

};

} //namespace SE

#ifdef SE_IMPL
#include <Frustum.tcc>
#endif

#endif
