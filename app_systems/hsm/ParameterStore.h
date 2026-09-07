
#ifndef APP_PARAMETER_STORE_H
#define APP_PARAMETER_STORE_H 1

#include <variant>
#include <unordered_map>
#include <StrID.h>

namespace SE {

struct SMTrigger { bool fired = true; };
using TParamValue = std::variant<float, bool, int, SMTrigger>;

// Stores named float / bool / int / trigger values.
// Triggers are write-once and consumed exactly once via ConsumeTrigger.
class ParameterStore {

public:
        void  SetFloat(StrID name, float v);
        void  SetBool(StrID name, bool v);
        void  SetInt(StrID name, int v);
        void  SetTrigger(StrID name);

        float GetFloat(StrID name, float default_val = 0.f) const;
        bool  GetBool(StrID name,  bool  default_val = false) const;
        int   GetInt(StrID name,   int   default_val = 0) const;
        // Returns float for both Float and Int parameter types (for condition evaluation).
        float GetNumeric(StrID name, float default_val = 0.f) const;
        // Returns true and resets to false on first call; subsequent calls return false.
        bool  ConsumeTrigger(StrID name);

        void  Clear();

private:
        std::unordered_map<StrID, TParamValue> mParams;
};

inline void ParameterStore::SetFloat(StrID name, float v) {
        mParams[name] = v;
}

inline void ParameterStore::SetBool(StrID name, bool v) {
        mParams[name] = v;
}

inline void ParameterStore::SetInt(StrID name, int v) {
        mParams[name] = v;
}

inline void ParameterStore::SetTrigger(StrID name) {
        mParams[name] = SMTrigger{true};
}

inline float ParameterStore::GetFloat(StrID name, float default_val) const {
        auto it = mParams.find(name);
        if (it == mParams.end()) return default_val;
        const float* pVal = std::get_if<float>(&it->second);
        return pVal ? *pVal : default_val;
}

inline bool ParameterStore::GetBool(StrID name, bool default_val) const {
        auto it = mParams.find(name);
        if (it == mParams.end()) return default_val;
        const bool* pVal = std::get_if<bool>(&it->second);
        return pVal ? *pVal : default_val;
}

inline int ParameterStore::GetInt(StrID name, int default_val) const {
        auto it = mParams.find(name);
        if (it == mParams.end()) return default_val;
        const int* pVal = std::get_if<int>(&it->second);
        return pVal ? *pVal : default_val;
}

inline float ParameterStore::GetNumeric(StrID name, float default_val) const {
        auto it = mParams.find(name);
        if (it == mParams.end()) return default_val;
        if (const float* pf = std::get_if<float>(&it->second)) return *pf;
        if (const int*   pi = std::get_if<int>  (&it->second)) return static_cast<float>(*pi);
        return default_val;
}

inline bool ParameterStore::ConsumeTrigger(StrID name) {
        auto it = mParams.find(name);
        if (it == mParams.end()) return false;
        SMTrigger* pTrigger = std::get_if<SMTrigger>(&it->second);
        if (!pTrigger || !pTrigger->fired) return false;
        pTrigger->fired = false;
        return true;
}

inline void ParameterStore::Clear() {
        mParams.clear();
}

} // namespace SE

#endif
