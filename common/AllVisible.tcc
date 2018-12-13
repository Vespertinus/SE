
namespace SE {

template <class ... TRenderableComponents> AllVisible<TRenderableComponents ...>::AllVisible() : changed (false) {

        mActiveRenderables.reserve(1000);
        vVisibleRenderables.reserve(1000);

}

template <class ... TRenderableComponents>
        template <class TRenderable >
                void AllVisible<TRenderableComponents ...>::AddRenderable(TRenderable * pComponent) {

        mActiveRenderables.emplace(reinterpret_cast<uintptr_t>(pComponent), TVariant(pComponent)/*test*/);
        changed = true;
}

template <class ... TRenderableComponents>
        template <class TRenderable >
                void AllVisible<TRenderableComponents ...>::RemoveRenderable(TRenderable * pComponent) {

        mActiveRenderables.erase(reinterpret_cast<uintptr_t>(pComponent));
        changed = true;
}


template <class ... TRenderableComponents>
        const std::vector<typename std::variant<TRenderableComponents * ...> *> & AllVisible<TRenderableComponents...>::
                GetVisible(bool & data_changed) {

        data_changed = changed;

        if (changed) {

                vVisibleRenderables.clear();

                for (auto & item : mActiveRenderables) {

                        vVisibleRenderables.emplace_back(&item.second);
                }

                changed = false;
        }

        return vVisibleRenderables;
}

}
