
#ifndef __CAMERA_H__
#define __CAMERA_H__ 1

#include <BoundingBox.h>
#include <Component_generated.h>

namespace SE {

class Camera {

        public:

        using TSerialized = FlatBuffers::StaticModel; //FIXME

        enum class Projection : uint8_t {
                PERSPECTIVE     = 1,
                ORTHO           = 2
        };

        private:

        enum Dirty : uint8_t {
                PROJECTION      = 0x1,
                VOLUME          = 0x2,
                ZOOM            = 0x4
        };

        struct Volume {
                //options for each type of camera
                float   near_clip       {0.1},
                        far_clip        {100.0f};

                //perspective only
                float   aspect          {1.333f},
                        fov             {60};

                //calculated
                float   left,
                        right,
                        top,
                        bottom;

                Projection projection{Projection::PERSPECTIVE};
                Volume() = default;
                Volume(const Projection proj, const float near, const float far, const float aspect, const float new_fov);
        };

        TSceneTree::TSceneNodeExact   * pNode;
        glm::uvec2                      view_size       {1024, 768};
        //std::array<uint8_t, 3>          up              { 0, 0, 1 }; // x, y or z
        //glm::vec3                       up              {0, 0, 1.0f };
        float                           zoom            {1},
                                        /** zooming target max size  */
                                        target_length   {20};
        Volume                          oVolume;
        glm::mat4                       mModelViewProjection;
        glm::mat4                       mProjection;
        uint8_t                         flags           {Dirty::PROJECTION | Dirty::VOLUME};

        void UpdateZoom();
        void RecalcVolume();
        void RecalcProjection();

        public:

        //using TSerialized = FlatBuffers::Camera;

        struct Settings {
                //glm::uvec2              view_size;
                //glm::vec3               up;
                float                   near_clip,
                                        far_clip;
                float                   fov;
                Projection              projection;
        };
	
        Camera(TSceneTree::TSceneNodeExact * pNewNode, bool enabled);
        Camera(TSceneTree::TSceneNodeExact * pNewNode, bool enabled, const Settings & oSettings);
        //Camera(TSceneTree::TSceneNodeExact * pNewNode, const SE::FlatBuffers::Camera * pCamera);
	~Camera() noexcept;

	void                    UpdateDimension(const uint32_t new_width, const uint32_t new_height);
	void                    UpdateDimension(const glm::uvec2 new_view_size);
        void                    SetZoom(const float new_zoom);
        void                    Zoom(const float factor);
        void                    ZoomTo(const BoundingBox & oBBox);
        void                    ZoomTo(const float width);
        const       glm::mat4 & GetWorldMVP();
        void                    LookAt(const float x, const float y, const float z);
        void                    LookAt(const glm::vec3 & center);
        void                    SetPos(const float new_x, const float new_y, const float new_z);
        void                    SetRotation(const float new_x, const float new_y, const float new_z);
        void                    SetFOV(const float new_fov);
        void                    SetProjection(const Projection proj);

        //Rotate
        //Translate

        //TSceneTree::TSceneNode  Node() const;
        void                    Enable();
        void                    Disable();
        void                    Print(const size_t indent);
        std::string             Str() const;
};

/*
 TODO split on 3 parts:
 - Node
 - Camera component with frustum
 - Controller component
 */

} //namespace SE

//#ifdef SE_IMPL
//#include <Camera.tcc>
//#endif

#endif
