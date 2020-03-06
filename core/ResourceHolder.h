
#ifndef __RESOURCE_HOLDER_H_
#define __RESOURCE_HOLDER_H_ 1

namespace SE {

/** resource id type */
typedef uint64_t rid_t;

class ResourceHolder {

        protected:

        uint32_t      size;
        rid_t         rid;
        std::string   sName;

        private:
        template <class T> struct Holder : public T {};

        public:
        ResourceHolder(const rid_t new_rid, const std::string_view sNewName) : rid(new_rid), sName(sNewName) { ;; }

        uint32_t Size() const { return size ; }
        rid_t RID() const { return rid; }
        const std::string & Name() const { return sName; }
};


} //namespace SE


#endif
