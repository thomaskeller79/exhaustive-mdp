#ifndef EXHAUSTIVE_MDP_H
#define EXHAUSTIVE_MDP_H

#include "search_engine.h"

struct Transition {
    Transition(int _fromID, int _actionID, std::vector<int> &&_toIDs, std::vector<double> &&_probs, double _reward) :
        fromID(_fromID), actionID(_actionID), toIDs(std::move(_toIDs)), probs(std::move(_probs)), reward(_reward) {}

    int fromID;
    int actionID;
    std::vector<int> toIDs;
    std::vector<double> probs;
    double reward;
};

class ExhaustiveMDP {
    std::vector<State> states;
    // states[0] is the initial state
    std::vector<Transition> transitions;
};

class ExhaustiveMDPGenerator : public ProbabilisticSearchEngine {
public:
    ExhaustiveMDPGenerator() :
        ProbabilisticSearchEngine("ExhaustiveMDPGenerator"),
        maxStates(1000000), fileName("states_" + SearchEngine::taskName) {}

    bool setValueFromString(std::string& param, std::string& value) override;


    void initSession() override;

    // Start the search engine to estimate the Q-value of a single action
    void estimateQValue(State const& /*state*/, int /*actionIndex*/,
                        double& /*qValue*/) override {
        assert(false);
    }

    // Start the search engine to estimate the Q-values of all applicable
    // actions
    void estimateQValues(State const& /*_rootState*/,
                         std::vector<int> const& /*actionsToExpand*/,
                         std::vector<double>& /*qValues*/) override {
        assert(false);
    }

    void printRoundStatistics(std::string /*indent*/) const override {}
    void printStepStatistics(std::string /*indent*/) const override {}

private:
    void expandState(int const stateID);
    void expandPDState(
        PDState const& state, double prob, int index,
        std::vector<int> &succStateIDs, std::vector<double> &probs);
    int getStateID(State const& state);

    void setNumMaxStates(int _maxStates) {
        maxStates = _maxStates;
    }

    void setFileName(std::string _fileName) {
        fileName = _fileName;
    }

    std::vector<State> states;
    std::vector<Transition> transitions;

    int maxStates;
    std::string fileName;
};

#endif
