#ifndef __STR_ID_H__
#define __STR_ID_H__ 1

namespace SE {

//THINK std::hash calculate always in size_t... so, arch dependent
class StrID {

        uint64_t        hash;

        public:
        StrID(const std::string_view sVal);
        StrID(const std::string & sVal);
        StrID(char const * data, const uint32_t size);

        bool operator == (const StrID rhs) const noexcept;
        operator uint64_t() const noexcept;

};

inline StrID::StrID(const std::string_view sVal) {

        hash = std::hash<std::string_view>{}(sVal);
}

inline StrID::StrID(const std::string & sVal) {

        hash = std::hash<std::string>{}(sVal);
}

inline StrID::StrID(char const * data, const uint32_t size) {
        hash = std::hash<std::string_view>{}(std::string_view(data, size));
}

inline bool StrID::operator == (const StrID rhs) const noexcept {
        return hash == rhs.hash;
}
        
inline StrID::operator uint64_t() const noexcept {
        return hash;
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
