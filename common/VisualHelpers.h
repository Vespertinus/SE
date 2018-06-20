

#ifndef __VISUAL_HELPERS_H__
#define __VISUAL_HELPERS_H__ 1

namespace SE {

namespace HELPERS {

void DrawAxes(const float size);

void DrawPlane(const float width, const float height,
               const float x_pos, const float y_pos, const float z_pos,
               const float x_dir, const float y_dir, const float z_dir,
               const uint32_t texture_id);

void DrawBox(const float x_size,
             const float y_size,
             const float z_size,
             const float x_pos,
             const float y_pos,
             const float z_pos,
             const uint32_t texture_id);

class Elipse {

  typedef std::vector < std::pair<float, float> > TVertArray;

  TVertArray  oVertArray;
  TVertArray  oColorArray;
  float       pos[3];
  float       radius;
  uint32_t    edge_count;
  float       delta;

  public:

  Elipse(const float x_pos, const float y_pos, const float z_pos, const float new_radius, const uint32_t new_edge_count);
  ~Elipse() throw();

  void Draw() const;
  void Resize(const float new_radius, const uint32_t new_edge_count);
  void ChangePos(const float x_pos, const float y_pos, const float z_pos);
};


void DrawBBox(const glm::vec3 & cur_min, const glm::vec3 & cur_max);


class VisualHelpers {

        uint32_t        local_axes_vao;
        ShaderProgram * pShader;

        public:
        VisualHelpers();
        //DrawAxes(const float size)
        void DrawLocalAxes();
        //Draw ...
};


} // HELPERS

using TVisualHelpers = Loki::SingletonHolder < HELPERS::VisualHelpers >;

} // SE

#ifdef SE_IMPL
#include <VisualHelpers.tcc>
#endif

#endif
