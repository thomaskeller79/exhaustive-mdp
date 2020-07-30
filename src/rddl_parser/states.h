#ifndef STATES_H
#define STATES_H

#include <cassert>
#include <set>
#include <string>
#include <vector>

#include "probability_distribution.h"
#include "utils/math_utils.h"

class ActionFluent;
class ActionPrecondition;
class ConditionalProbabilityFunction;
struct RDDLTask;

/*****************************************************************
                               State
*****************************************************************/

struct State {
    State(std::vector<ConditionalProbabilityFunction*> const& cpfs);
    State(State const& other) = default;
    State(int stateSize) : state(stateSize, 0.0) {}

    // Create a copy of other except that the value of variable var is
    // set to val
    State(State const& other, int var, int val)
        : state(other.state) {
        assert(var < state.size());
        state[var] = val;
    }


    double& operator[](int const& index) {
        assert(index < state.size());
        return state[index];
    }

    double const& operator[](int const& index) const {
        assert(index < state.size());
        return state[index];
    }

    void print(std::ostream& out) const;

    struct StateSort {
        bool operator()(State const& lhs, State const& rhs) const {
            assert(lhs.state.size() == rhs.state.size());

            for (int i = lhs.state.size() - 1; i >= 0; --i) {
                if (MathUtils::doubleIsSmaller(lhs.state[i], rhs.state[i])) {
                    return true;
                } else if (MathUtils::doubleIsSmaller(rhs.state[i],
                                                      lhs.state[i])) {
                    return false;
                }
            }
            return false;
        }
    };

    std::vector<double> state;
};

/*****************************************************************
                            PDState
*****************************************************************/

struct PDState {
    PDState(int stateSize) : state(stateSize, DiscretePD()) {}

    DiscretePD& operator[](int const& index) {
        assert(index < state.size());
        return state[index];
    }

    DiscretePD const& operator[](int const& index) const {
        assert(index < state.size());
        return state[index];
    }

    struct PDStateSort {
        bool operator()(PDState const& lhs, PDState const& rhs) const {
            for (unsigned int i = 0; i < lhs.state.size(); ++i) {
                if (rhs.state[i] < lhs.state[i]) {
                    return false;
                } else if (lhs.state[i] < rhs.state[i]) {
                    return true;
                }
            }
            return false;
        }
    };

    std::vector<DiscretePD> state;
};

/*****************************************************************
                          KleeneState
*****************************************************************/

class KleeneState {
public:
    KleeneState(int stateSize) : state(stateSize) {}

    KleeneState(State const& origin) : state(origin.state.size()) {
        for (unsigned int index = 0; index < state.size(); ++index) {
            state[index].insert(origin[index]);
        }
    }

    std::set<double>& operator[](int const& index) {
        assert(index < state.size());
        return state[index];
    }

    std::set<double> const& operator[](int const& index) const {
        assert(index < state.size());
        return state[index];
    }

    bool operator==(KleeneState const& other) const {
        assert(state.size() == other.state.size());

        for (unsigned int index = 0; index < state.size(); ++index) {
            if (!std::equal(state[index].begin(), state[index].end(),
                            other.state[index].begin())) {
                return false;
            }
        }
        return true;
    }

    // This is used to merge two KleeneStates
    KleeneState operator|=(KleeneState const& other) {
        assert(state.size() == other.state.size());

        for (unsigned int i = 0; i < state.size(); ++i) {
            state[i].insert(other.state[i].begin(), other.state[i].end());
        }
        return *this;
    }

    KleeneState const operator||(KleeneState const& other) {
        assert(state.size() == other.state.size());
        return KleeneState(*this) |= other;
    }

    void print(std::ostream& out) const;

protected:
    std::vector<std::set<double>> state;

private:
    KleeneState(KleeneState const& other) : state(other.state) {}
};

/*****************************************************************
                            ActionState
*****************************************************************/

class ActionState {
public:
    ActionState(int size) : state(size, 0), index(-1) {}
    ActionState(std::vector<int> _state) : state(_state), index(-1) {}

    ActionState(ActionState const& other)
        : state(other.state), index(other.index) {}

    // Create a copy of other except that the value of variable var is
    // set to val
    ActionState(ActionState const& other, int var, int val)
        : state(other.state), index(other.index) {
        state[var] = val;
    }

    int& operator[](int const& index) {
        return state[index];
    }

    const int& operator[](int const& index) const {
        return state[index];
    }

    bool operator<(ActionState const& other) const {
        if (state.size() < other.state.size()) {
            return true;
        } else if (state.size() > other.state.size()) {
            return false;
        }

        for (unsigned int i = 0; i < state.size(); ++i) {
            if (state[i] < other.state[i]) {
                return true;
            } else if (state[i] > other.state[i]) {
                return false;
            }
        }

        return false;
    }

    bool isNOOP(RDDLTask* task) const;

    void print(std::ostream& out) const;

    std::vector<int> state;
    std::vector<ActionPrecondition*> relevantSACs;
    int index;
};

#endif
