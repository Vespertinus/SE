
#ifndef __FRUSTUM_H_
#define __FRUSTUM_H_ 1

namespace SE {

class Frustum {

  public:
  
  static const uint8_t uPERSPECTIVE   = 1;
  static const uint8_t uORTHO         = 2;

  struct Volume {
    float     near_clip,
              far_clip,
              left,
              right,
              top,
              bottom;
  };

  struct Settings {
    union {
      Volume oVolume;
      struct {
        float aspect;
        float fov;
        float near_clip;
        float far_clip;
      };
    }
    uint8_t projection;
    Settings() : projection(uPERSPECTIVE) { ;; }
  };

  protected:

  Volume    oVolume;

  float     aspect;
  float     fov;

  uint8_t   projection;


  void RecalcVolume();
  void RecalcPersp();


  //void            UpdateProjection();

  public:

  //Frustum();
  Frustum(const Settings & oNewSettings);
  ~Frustum() throw();


  void            SetFOV(const float new_fov);
  void            SetFOV(const float new_fov, const float new_aspect);
  float           GetFOV() const;
  void            SetAspect(const float new_aspect);
  float           GetAspect() const;
  void            SetVolume(const & Volume oNewVolume);
  void            SetVolume(const & Volume oNewVolume, const uint8_t new_projection);
  const & Volume  GetVolume();

};

} //namespace SE

#include <Frustum.tcc>

#endif
