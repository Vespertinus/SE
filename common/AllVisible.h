
#ifndef __ALL_VISIBLE_H__
#define __ALL_VISIBLE_H__ 1


namespace SE {

template <class ... TRenderableComponents > class AllVisible {

        public:

        using TVariant = std::variant<TRenderableComponents * ...>;

        private:

        std::unordered_map <std::uintptr_t, TVariant>   mActiveRenderables;
        /** same elements in case of rendering all */
        std::vector<TVariant *>                         vVisibleRenderables;
        bool                                            changed;

        public:

        AllVisible();
        template <class TRenderable > void AddRenderable(TRenderable * pComponent);
        template <class TRenderable > void RemoveRenderable(TRenderable * pComponent);
        const std::vector<TVariant *> & GetVisible(bool & data_changed);
};


} //namespace SE

#include <AllVisible.tcc>

#endif



