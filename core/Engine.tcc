namespace SE {


template <class ... TSystems> Engine<TSystems...>::~Engine() noexcept {

        MP::TupleForEachRev(oSystems, [](auto * pSystem) {
                delete pSystem;
        });
}

template <class ... TSystems> void Engine<TSystems...>::Init() {

        MP::TupleForEach(oSystems, [this](auto * pSystem) {
                using TSystem = typename std::remove_pointer<decltype(pSystem)>::type;

                if (pSystem != nullptr) { return; }

                //FIXME ...
                TSystem ** pSystemStorage = &std::get<TSystem *>(oSystems);
                *pSystemStorage = new TSystem();
        });

}

template <class ... TSystems>
template <class TSystem, class ... TArgs>
        ret_code_t Engine<TSystems...>::Init(const TArgs && ... oArgs) {

        TSystem ** pSystem = &std::get<TSystem *>(oSystems);
        if (*pSystem != nullptr) {

                log_e("system: '{}' already inited", typeid(TSystem).name());
                return uLOGIC_ERROR;
        }

        try {
                *pSystem = new TSystem(oArgs...);
        }
        catch(std::exception & ex) {
                log_e("got exception, description = '{}', system: '{}'", ex.what(), typeid(TSystem).name());
                return uWRONG_INPUT_DATA;
        }
        catch(...) {
                log_e("got unknown exception, system: '{}'", typeid(TSystem).name());
                return uWRONG_INPUT_DATA;
        }
        return uSUCCESS;
}

template <class ... TSystems>
        template <class TSystem>
        TSystem &  Engine<TSystems...>::Get() {

        return *std::get<TSystem *>(oSystems);
}

}
