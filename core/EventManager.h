
#ifndef __RENDERER_H__
#define __RENDERER_H__ 1


namespace SE {

#ifndef SE_MAX_EVENT_SIZE
#define SE_MAX_EVENT_SIZE 128
#endif

class Event {

        std::array<uint8_t, SE_MAX_EVENT_SIZE>  vData;
        uint32_t                                used_size;//???

        public:

        Event(const & Event);
        //template <class Args..> Event(Args.. && oArgs);
        template <class TEventData> Event(TEventData && oEventData);
        template <class TEvenData> & TEvenData Get() const;
};

class EventManager {

/*
queue 2x vector buf
max event size
event buf

TriggerEvent
QueueEvent
SendEvent macro?

how to store delegate handlers
*/

};

};

#endif
