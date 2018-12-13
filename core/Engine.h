
#ifndef __ENGINE_H__
#define __ENGINE_H__ 1

namespace SE {

template <class ... TSystems> class Engine {

        std::tuple<TSystems * ...>      oSystems{};

        public:
        ~Engine() noexcept;
        /** init not inited systems */
        void Init();
        template <class TSystem, class ... TArgs> ret_code_t    Init(const TArgs && ... oArgs);
        template <class TSystem> TSystem &                      Get();
        //Update; TODO

        /** TODO:
          - Engine configuration management
          -- load \ save
          -- loop support
          -- json?
          -- GetParam...
        */
};


} //namespace SE

#include <Engine.tcc>

#endif



