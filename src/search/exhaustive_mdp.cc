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
    for (int i = 0; i < states.size(); ++i) {
        if (states[i] == state) {
            return i;
        }
    }
    if (states.size() == maxStates) {
        cout << "Error: State limit reached! Aborting." << endl;
        exit(1);
    }
    int result = states.size();
    states.push_back(state);
    return result;
}

void ExhaustiveMDPGenerator::expandPDState(
    PDState const& state, double prob, int index, vector<int> &succStateIDs, vector<double> &probs) {
    while (index < State::numberOfProbabilisticStateFluents &&
           state.probabilisticStateFluentAsPD(index).isDeterministic()) {
        ++index;
    }
    if (index == State::numberOfProbabilisticStateFluents) {
        State succ(state);
        State::calcStateHashKey(succ);
        State::calcStateFluentHashKeys(succ);
        succStateIDs.push_back(getStateID(succ));
        probs.push_back(prob);
    } else {
        const DiscretePD &varVal = state.probabilisticStateFluentAsPD(index);
        for (size_t i = 0; i < varVal.values.size(); ++i) {
            PDState copy(state);
            copy.probabilisticStateFluent(index) = varVal.values[i];
            expandPDState(copy, prob*varVal.probabilities[i], index + 1, succStateIDs, probs);
        }
    }
}

void ExhaustiveMDPGenerator::initSession() {
    states.push_back(SearchEngine::initialState);
    int stateID = 0;
    while(stateID < states.size()) {
        cout << "expanding state with index " << stateID << endl;
        expandState(stateID);
        ++stateID;
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

    exit(1);
}

void ExhaustiveMDPGenerator::expandState(int const stateID) {
    vector<int> actionsToExpand = getApplicableActions(states[stateID]);
    for (int actionID = 0; actionID < actionsToExpand.size(); ++actionID) {
        if (actionsToExpand[actionID] == actionID) {
            // cout << "action " << actionStates[actionID].toCompactString() << " (" << actionID << ")" << endl;
            PDState next(SearchEngine::horizon);
            // cout << states[stateID].hashKey << endl;
            calcSuccessorState(states[stateID], actionID, next);
            // cout << "successor computed!" << endl;
            double reward = 0.0;
            calcReward(states[stateID], actionID, reward);
            // cout << "reward: " << reward << endl;
            vector<int> succStateIDs;
            vector<double> probs;
            expandPDState(next,1.0, 0, succStateIDs, probs);
            // cout << "num successors: " << succStateIDs.size() << endl;
            // cout << "total num states: " << states.size() << endl;
            transitions.emplace_back(stateID, actionID, move(succStateIDs), move(probs), reward);
        } else if (actionsToExpand[actionID] >= 0) {
            // TODO: Just copy this transition!
            PDState next(SearchEngine::horizon);
            // cout << states[stateID].hashKey << endl;
            calcSuccessorState(states[stateID], actionID, next);
            // cout << "successor computed!" << endl;
            double reward = 0.0;
            calcReward(states[stateID], actionID, reward);
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
