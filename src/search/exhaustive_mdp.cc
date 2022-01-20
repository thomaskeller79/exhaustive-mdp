#include "exhaustive_mdp.h"

#include "utils/system_utils.h"

#include <sstream>

using namespace std;

bool ExhaustiveMDPGenerator::setValueFromString(string& param, string& value) {
    if (param == "-ms") {
        setNumMaxStates(atoi(value.c_str()));
        return true;
    } else if (param == "-file") {
        setFileName(value.c_str());
        return true;
    }

    return ProbabilisticSearchEngine::setValueFromString(param, value);
}

int ExhaustiveMDPGenerator::getStateID(State const& state) {
    if (states.count(state)) {
        return states[state];
    }
    int result = states.size();
    if (result == maxStates) {
        cout << "Error: State limit reached! Aborting." << endl;
        exit(1);
    }
    open.push_back(state);
    states[state] = result;
    return result;
}

void ExhaustiveMDPGenerator::expandPDState(
    PDState state, double prob, int index, vector<int> &succStateIDs, vector<double> &probs) {
    while (index < State::numberOfProbabilisticStateFluents &&
           state.probabilisticStateFluentAsPD(index).isDeterministic()) {
       state.probabilisticStateFluent(index) = state.probabilisticStateFluentAsPD(index).values[0];
        ++index;
    }
    if (index == State::numberOfProbabilisticStateFluents) {
        State::calcStateHashKey(state);
        State::calcStateFluentHashKeys(state);
        succStateIDs.push_back(getStateID(state));
        probs.push_back(prob);
    } else {
        const DiscretePD &varVal = state.probabilisticStateFluentAsPD(index);
        for (size_t i = 0; i < varVal.values.size(); ++i) {
            state.probabilisticStateFluent(index) = varVal.values[i];
            expandPDState(state, prob*varVal.probabilities[i], index + 1, succStateIDs, probs);
        }
    }
}

void ExhaustiveMDPGenerator::initSession() {
    applicableActionCounter = vector<int>(SearchEngine::actionStates.size(), 0);

    getStateID(SearchEngine::initialState);
    while(!open.empty()) {
        State state = open.back();
        open.pop_back();
        expandState(state);
    }

    stringstream ss;
    ss << states.size() << endl << SearchEngine::actionStates.size() << endl;
    for (Transition const& t : transitions) {
        ss << t.fromID << " " << t.actionID << " ";
        for (int i = 0; i < t.toIDs.size(); ++i) {
            ss << "( " << t.toIDs[i] << " " << t.probs[i] << " ) ";
        }
        ss << t.reward << endl;
    }
    SystemUtils::writeFile(fileName, ss.str());

    cout << "Actions that are never applicable: " << endl;
    for (int i = 0; i < applicableActionCounter.size(); ++i) {
        if (applicableActionCounter[i] == 0) {
            cout << SearchEngine::actionStates[i].toCompactString() << endl;
        }
    }

    exit(1);
}

void ExhaustiveMDPGenerator::expandState(State const& state) {
    assert(states.count(state));
    int stateID = states[state];
    vector<int> actionsToExpand = getApplicableActions(state);
    for (int actionID = 0; actionID < actionsToExpand.size(); ++actionID) {
        if (actionsToExpand[actionID] == actionID) {
            ++applicableActionCounter[actionID];
            // cout << "action " << actionStates[actionID].toCompactString() << " (" << actionID << ")" << endl;
            PDState next(SearchEngine::horizon);
            // cout << state.hashKey << endl;
            calcSuccessorState(state, actionID, next);
            // cout << "successor computed!" << endl;
            double reward = 0.0;
            calcReward(state, actionID, reward);
            // cout << "reward: " << reward << endl;
            vector<int> succStateIDs;
            vector<double> probs;
            expandPDState(next,1.0, 0, succStateIDs, probs);
            // cout << "num successors: " << succStateIDs.size() << endl;
            // cout << "total num states: " << states.size() << endl;
            transitions.emplace_back(stateID, actionID, move(succStateIDs), move(probs), reward);
        } else if (actionsToExpand[actionID] >= 0) {
            ++applicableActionCounter[actionID];
            // TODO: Just copy this transition!
            PDState next(SearchEngine::horizon);
            // cout << state.hashKey << endl;
            calcSuccessorState(state, actionID, next);
            // cout << "successor computed!" << endl;
            double reward = 0.0;
            calcReward(state, actionID, reward);
            // cout << "reward: " << reward << endl;
            vector<int> succStateIDs;
            vector<double> probs;
            expandPDState(next, 1.0, 0, succStateIDs, probs);
            // cout << "num successors: " << succStateIDs.size() << endl;
            // cout << "total num states: " << states.size() << endl;
            transitions.emplace_back(stateID, actionID, move(succStateIDs), move(probs), reward);
        }
    }
}
