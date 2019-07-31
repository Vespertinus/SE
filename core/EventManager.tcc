
#include <MPTraits.h>

namespace SE {

struct DeletedEvent {};

//FIXME
#ifdef SE_IMPL
Event::Event(std::type_index oNewKey, const uint16_t data_len, void * ptr) :
        oKey(oNewKey),
        len(data_len),
        pData(ptr) {
}
#endif
template <class TEventData> Event::Event(TEventData && oEventData) :
        oKey(typeid(TEventData)),
        len(sizeof(TEventData)),
        pData(&oEventData) {
}

template <class TEventData> const TEventData & Event::Get() const {

        std::type_index oReqKey{typeid(TEventData)};
        se_assert(oKey == oReqKey);//THINK may be only error

        return *reinterpret_cast<const TEventData *>(pData);
}


template <class TDelegate> bool EventManager::Contain(const std::type_index oKey, TDelegate & oDelegate) {

        bool ret = false;

        if (auto it = mListeners.find(oKey); it != mListeners.end()) {

                auto & vListeners = it->second;
                auto res = std::find(vListeners.begin(), vListeners.end(), oDelegate);
                if (res != vListeners.end()) {
                        ret = true;
                }
        }

        return ret;
}

template <class TEventData, auto TMethod, class TListener> bool
        EventManager::AddListener(TListener * pListener) {

        auto oDelegate = TListenerDelegate::create<TListener, TMethod>(pListener);
        std::type_index oKey{typeid(TEventData)};

        if (Contain(oKey, oDelegate)) {
                log_w("obj: '{}' delegate '{}' already added for event: '{}'",
                                MP::GetName(*pListener),
                                typeid(TMethod).name(),
                                typeid(TEventData).name() );
                return false;
        }

        log_d("obj: '{}' delegate '{}' subscribe to event: '{}'",
                        MP::GetName(*pListener),
                        typeid(TMethod).name(),
                        typeid(TEventData).name() );

        mListeners[oKey].emplace_back(oDelegate);
        return true;
}

template <class TEventData, auto TMethod, class TListener> bool
        EventManager::RemoveListener(TListener * pListener) {

        auto oDelegate = TListenerDelegate::create<TListener, TMethod>(pListener);
        std::type_index oKey{typeid(TEventData)};

        if (auto it = mListeners.find(oKey); it != mListeners.end()) {
                auto & vListeners = it->second;
                se_assert (!vListeners.empty()); //THINK
                auto it2 = std::find(vListeners.begin(), vListeners.end(), oDelegate);
                if (it2 != vListeners.end()) {
                        if (*it2 != vListeners.back()) {
                                *it2 = vListeners.back();
                        }
                        vListeners.pop_back();

                        if (vListeners.size() == 0) {
                                mListeners.erase(it);
                        }
                        return true;
                }
        }
        return false;
}

template <class TEventData> void EventManager::TriggerEvent(const TEventData & oEventData) {

        TListenersMap::iterator it;

        if (it = mListeners.find(typeid(TEventData)); it == mListeners.end()) {
                return;
        }

        Event oEvent(oEventData);

        for (auto & oListener : it->second) {
                oListener(oEvent);
        }

}

template <class TEventData> void EventManager::QueueEvent(const TEventData & oEventData) {

        static_assert(sizeof(TEventData) < std::numeric_limits<uint16_t>::max(), "too big event size");

        std::type_index oKey = typeid(TEventData);
        uint16_t        len  = sizeof(TEventData);

        if (auto it = mListeners.find(oKey); it == mListeners.end()) {
                return;
        }

        auto & vQueueBuffer = vEventBuffers[cur_buf];
        vQueueBuffer.reserve(vQueueBuffer.size() + SE_EVENT_HEADER_SIZE + sizeof(TEventData) );

        Append(&oKey,       sizeof(oKey));
        Append(&len,        sizeof(len));
        Append(&oEventData, sizeof(TEventData));

}

template <class TEventData> void EventManager::AbortEvent(const bool all) {

        auto & vProcBuffer      {vEventBuffers[cur_buf]};
        size_t cur_pos          {0};
        std::type_index oKey    {typeid(TEventData)};
        std::type_index * pKey;
        uint16_t        * pLen;

        while((cur_pos + SE_EVENT_HEADER_SIZE) < vProcBuffer.size()) {

                pKey = reinterpret_cast<std::type_index *>(&vProcBuffer[cur_pos]);
                pLen = reinterpret_cast<uint16_t *>(&vProcBuffer[cur_pos + sizeof(std::type_index)]);
                se_assert((cur_pos + SE_EVENT_HEADER_SIZE + *pLen) <= vProcBuffer.size());

                if (*pKey == oKey) {//mark as removed
                        *pKey = typeid(DeletedEvent);
                        if (!all) {
                                break;
                        }
                }
                cur_pos += SE_EVENT_HEADER_SIZE + *pLen;
        }
}

//FIXME
#ifdef SE_IMPL
void EventManager::Append(const void * pData, const size_t data_len) {

        vEventBuffers[cur_buf].insert(
                        vEventBuffers[cur_buf].end(),
                        reinterpret_cast<const uint8_t *>(pData),
                        reinterpret_cast<const uint8_t *>(pData) + data_len);
}

void EventManager::Process() {

        auto              proc_buf      {cur_buf};
        cur_buf                         = 1 - cur_buf;

        auto            & vProcBuffer   {vEventBuffers[proc_buf]};
        size_t            cur_pos       {0};
        std::type_index * pKey;
        uint16_t        * pLen;

        TListenersMap::iterator it;
        std::type_index oDeleted   {typeid(DeletedEvent)};

        while((cur_pos + SE_EVENT_HEADER_SIZE) < vProcBuffer.size()) {

                pKey = reinterpret_cast<std::type_index *>(&vProcBuffer[cur_pos]);
                pLen = reinterpret_cast<uint16_t *>(&vProcBuffer[cur_pos + sizeof(std::type_index)]);
                se_assert((cur_pos + SE_EVENT_HEADER_SIZE + *pLen) <= vProcBuffer.size());
                uint8_t * pData = &vProcBuffer[cur_pos + SE_EVENT_HEADER_SIZE];

                cur_pos += SE_EVENT_HEADER_SIZE + *pLen;

                if (*pKey == oDeleted) {
                        continue;
                }

                if (it = mListeners.find(*pKey); it == mListeners.end()) {
                        log_w("skip event '{}' without listeners", pKey->name());
                        continue;
                }

                Event oEvent(*pKey, *pLen, pData);

                for (auto & oListener : it->second) {
                        oListener(oEvent);
                }
        }
        vProcBuffer.clear();
}
#endif
};

