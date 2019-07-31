
#ifndef __EVENT_MANAGER_H__
#define __EVENT_MANAGER_H__ 1

#include <typeindex>
#include <Delegate.h>

namespace SE {

#define SE_EVENT_HEADER_SIZE (sizeof(std::type_index) + sizeof(uint16_t))

class Event {

        std::type_index oKey;
        uint16_t        len;
        const void    * pData;

        public:
        Event(std::type_index oNewKey, const uint16_t data_len, void * ptr);
        template <class TEventData> Event(TEventData && oEventData);
        template <class TEventData> const TEventData & Get() const;
};

/**
 data layout:
 |type_index|len |data |
 |8b        |2b  |x    |
*/
class EventManager {

        using TListenerDelegate = SA::delegate<void (const Event &)>;
        using TListenersArr = std::vector<TListenerDelegate>;
        using TListenersMap = std::unordered_map<std::type_index, TListenersArr >;

        TListenersMap                           mListeners;
        std::array<std::vector<uint8_t>, 2>     vEventBuffers;
        uint8_t                                 cur_buf{0};
        //THINK store queued events cnt for each queue and check in Process

        template <class TDelegate> bool Contain(const std::type_index oKey, TDelegate & oDelegate);
        void Append(const void * pData, const size_t data_len);

        public:
        EventManager() = default;//max event queue buffer size?
        template <class TEventData, auto TMethod, class TListener> bool AddListener(
                        TListener * pListener);
        template <class TEventData, auto TMethod, class TListener> bool RemoveListener(
                        TListener * pListener);
        template <class TEventData> void TriggerEvent(const TEventData & oEvent);
        template <class TEventData> void QueueEvent(const TEventData & oEvent);
        template <class TEventData> void AbortEvent(const bool all = false);
        void Process();
};

}

#endif
