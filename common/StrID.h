#ifndef __STR_ID_H__
#define __STR_ID_H__ 1

namespace SE {

//THINK std::hash calculate always in size_t... so, arch dependent
class StrID {

        uint64_t        hash;

        public:
        StrID();
        StrID(const std::string_view sVal);
        StrID(const std::string & sVal);
        StrID(const char * data, const uint32_t size);
        StrID(const char * data);

        bool operator == (const StrID rhs) const noexcept;
        operator uint64_t() const noexcept;
        void operator = (const StrID rhs) noexcept;

        friend std::ostream & operator<< (std::ostream& stream, const StrID & obj);
};

inline StrID::StrID() : hash(0xDEADBEEF) { ;; }

inline StrID::StrID(const std::string_view sVal) {

        hash = std::hash<std::string_view>{}(sVal);
}

inline StrID::StrID(const std::string & sVal) {

        hash = std::hash<std::string>{}(sVal);
}

inline StrID::StrID(char const * data, const uint32_t size) {
        hash = std::hash<std::string_view>{}(std::string_view(data, size));
}

inline StrID::StrID(const char * data) : StrID(std::string_view(data)) {
}

inline bool StrID::operator == (const StrID rhs) const noexcept {
        return hash == rhs.hash;
}

inline StrID::operator uint64_t() const noexcept {
        return hash;
}

inline std::ostream & operator<< (std::ostream& stream, const StrID & obj) {
        stream << obj.hash;
        return stream;
}

inline void StrID::operator = (const StrID rhs) noexcept {
        hash = rhs.hash;
}

} //namespace SE

namespace std {
        template<> struct hash<SE::StrID> {
                typedef SE::StrID argument_type;
                typedef uint64_t result_type;
                result_type operator()(argument_type const strid) const noexcept {
                        return strid;
                }
        };
}

#endif
