
#ifndef APP_STATE_CONDITION_H
#define APP_STATE_CONDITION_H 1

#include <StrID.h>
#include <hsm/ParameterStore.h>
#include <StateMachine_generated.h>

namespace SE {

// Evaluates one boolean expression against a ParameterStore.
// Multiple conditions on a transition are ANDed by the caller.
struct StateCondition {

        StrID             parameter;
        FlatBuffers::SMConditionOp op = FlatBuffers::SMConditionOp::IsTrue;
        float             threshold   = 0.f;

        bool Evaluate(ParameterStore& params) const;

        static StateCondition FromFB(const FlatBuffers::SMCondition& fb);
};

inline bool StateCondition::Evaluate(ParameterStore& params) const {

        using Op = FlatBuffers::SMConditionOp;
        switch (op) {
                case Op::IsTrue:       return params.GetBool(parameter);
                case Op::IsFalse:      return !params.GetBool(parameter);
                case Op::Triggered:    return params.ConsumeTrigger(parameter);
                case Op::Greater:      return params.GetNumeric(parameter) >  threshold;
                case Op::GreaterEqual: return params.GetNumeric(parameter) >= threshold;
                case Op::Less:         return params.GetNumeric(parameter) <  threshold;
                case Op::LessEqual:    return params.GetNumeric(parameter) <= threshold;
                case Op::Equal:        return params.GetNumeric(parameter) == threshold;
                case Op::NotEqual:     return params.GetNumeric(parameter) != threshold;
        }
        return false;
}

inline StateCondition StateCondition::FromFB(const FlatBuffers::SMCondition& fb) {
        StateCondition c;
        if (fb.parameter()) c.parameter = StrID(fb.parameter()->str());
        c.op        = fb.op();
        c.threshold = fb.threshold();
        return c;
}

} // namespace SE

#endif
