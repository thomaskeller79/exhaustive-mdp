#ifndef EVALUATABLE_H
#define EVALUATABLE_H

#include "logical_expressions.h"

class Evaluatable {
public:
    //TODO: This is very very ugly, but cpfs, planning tasks and
    //states are very tightly coupled. Nevertheless, there must be a
    //way to get rid of this, even if it takes some work!
    friend class PlanningTask;

    // Evaluate
    void evaluate(double& res, State const& current, ActionState const& actions) {
        switch(cachingType) {
        case NONE:
            formula->evaluate(res, current, actions);
            break;
        case MAP:
            stateHashKey = current.stateFluentHashKey(hashIndex) + actionHashKeyMap[actions.index];
            if(!((current.stateFluentHashKey(hashIndex) >= 0) && (actionHashKeyMap[actions.index] >= 0) && (stateHashKey >= 0))) {
                std::cout << name << std::endl;
                std::cout << hashIndex << " / " << actions.index << " / " << stateHashKey << std::endl;
                std::cout << current.stateFluentHashKey(hashIndex) << std::endl;
                std::cout << actionHashKeyMap[actions.index] << std::endl;
            }
            assert((current.stateFluentHashKey(hashIndex) >= 0) && (actionHashKeyMap[actions.index] >= 0) && (stateHashKey >= 0));

            if(evaluationCacheMap.find(stateHashKey) != evaluationCacheMap.end()) {
                res = evaluationCacheMap[stateHashKey];
            } else {
                formula->evaluate(res, current, actions);
                evaluationCacheMap[stateHashKey] = res;
            }
            break;
        case DISABLED_MAP:
            stateHashKey = current.stateFluentHashKey(hashIndex) + actionHashKeyMap[actions.index];
            assert((current.stateFluentHashKey(hashIndex) >= 0) && (actionHashKeyMap[actions.index] >= 0) && (stateHashKey >= 0));

            if(evaluationCacheMap.find(stateHashKey) != evaluationCacheMap.end()) {
                res = evaluationCacheMap[stateHashKey];
            } else {
                formula->evaluate(res, current, actions);
            }

            break;
        case VECTOR:
            stateHashKey = current.stateFluentHashKey(hashIndex) + actionHashKeyMap[actions.index];
            assert((current.stateFluentHashKey(hashIndex) >= 0) && (actionHashKeyMap[actions.index] >= 0) && (stateHashKey >= 0));

            if(MathUtils::doubleIsMinusInfinity(evaluationCacheVector[stateHashKey])) {
                formula->evaluate(res, current, actions);
                evaluationCacheVector[stateHashKey] = res;
            } else {
                res = evaluationCacheVector[stateHashKey];
            }
            break;
        }
    }

    void evaluateToKleeneOutcome(double& res, State const& current, ActionState const& actions) {
        switch(kleeneCachingType) {
        case NONE:
            formula->evaluateToKleeneOutcome(res, current, actions);
            break;
        case MAP:
            stateHashKey = current.stateFluentHashKey(hashIndex) + actionHashKeyMap[actions.index];
            assert((current.stateFluentHashKey(hashIndex) >= 0) && (actionHashKeyMap[actions.index] >= 0) && (stateHashKey >= 0));

            if(kleeneEvaluationCacheMap.find(stateHashKey) != kleeneEvaluationCacheMap.end()) {
                res = kleeneEvaluationCacheMap[stateHashKey];
            } else {
                formula->evaluateToKleeneOutcome(res, current, actions);
                kleeneEvaluationCacheMap[stateHashKey] = res;
            }
            break;
        case DISABLED_MAP:
            stateHashKey = current.stateFluentHashKey(hashIndex) + actionHashKeyMap[actions.index];
            assert((current.stateFluentHashKey(hashIndex) >= 0) && (actionHashKeyMap[actions.index] >= 0) && (stateHashKey >= 0));

            if(kleeneEvaluationCacheMap.find(stateHashKey) != kleeneEvaluationCacheMap.end()) {
                res = kleeneEvaluationCacheMap[stateHashKey];
            } else {
                formula->evaluateToKleeneOutcome(res, current, actions);
            }

            break;
        case VECTOR:
            stateHashKey = current.stateFluentHashKey(hashIndex) + actionHashKeyMap[actions.index];
            assert((current.stateFluentHashKey(hashIndex) >= 0) && (actionHashKeyMap[actions.index] >= 0) && (stateHashKey >= 0));

            if(MathUtils::doubleIsMinusInfinity(kleeneEvaluationCacheVector[stateHashKey])) {
                formula->evaluateToKleeneOutcome(res, current, actions);
                kleeneEvaluationCacheVector[stateHashKey] = res;
            } else {
                res = kleeneEvaluationCacheVector[stateHashKey];
            }
            break;
        }
    }

    // Initialization
    void initialize();
    void initializeHashKeys(int _hashIndex, std::vector<ActionState> const& actionStates,
                            std::vector<ConditionalProbabilityFunction*> const& CPFs,
                            std::vector<std::vector<std::pair<int,long> > >& indexToStateFluentHashKeyMap,
                            std::vector<std::vector<std::pair<int,long> > >& indexToKleeneStateFluentHashKeyMap);

    // Disable caching
    void disableCaching();

    // Properties
    bool const& isProbabilistic() const {
        return isProb;
    }

    bool hasPositiveActionDependencies() const {
        return !positiveActionDependencies.empty();
    }

    bool isActionIndependent() const {
        return (positiveActionDependencies.empty() && negativeActionDependencies.empty());
    }

    bool const& containsArithmeticFunction() const {
        return hasArithmeticFunction;
    }

protected:
    Evaluatable(std::string _name, LogicalExpression* _formula) :
        name(_name),
        formula(_formula),
        isProb(false),
        hasArithmeticFunction(false),
        hashIndex(-1),
        cachingType(VECTOR),
        kleeneCachingType(VECTOR) {}

    Evaluatable(Evaluatable const& other, LogicalExpression* _formula) :
        name(other.name),
        formula(_formula),
        isProb(other.isProb),
        hasArithmeticFunction(other.hasArithmeticFunction),
        hashIndex(other.hashIndex),
        cachingType(other.cachingType),
        evaluationCacheVector(other.evaluationCacheVector.size(), -std::numeric_limits<double>::max()),
        kleeneCachingType(other.kleeneCachingType),
        kleeneEvaluationCacheVector(other.kleeneEvaluationCacheVector.size(), -std::numeric_limits<double>::max()),
        actionHashKeyMap(other.actionHashKeyMap) {}

    // A unique string that describes this, only used for print()
    std::string name;

    // The formula that is evaluatable
    LogicalExpression* formula;

    // Some properties of formula
    std::set<StateFluent*> dependentStateFluents;
    std::set<ActionFluent*> positiveActionDependencies;
    std::set<ActionFluent*> negativeActionDependencies;
    bool isProb;
    bool hasArithmeticFunction;

    // hashIndex gives the position in the stateFluentHashKey vector
    // of a state where the state fluent hash key of this Evaluatable
    // is stored
    int hashIndex;

    enum CachingType {
        NONE, // too many variables influence formula
        MAP, // many variables influence formula, but it's still possible with a map
        DISABLED_MAP, // as MAP, but after disableCaching() has been called
        VECTOR // only few variables influence formula, so we use a vector for caching
    };

    // CachingType describes which of the two (if any) datastructures
    // is used to cache computed values
    CachingType cachingType;
    std::map<long, double> evaluationCacheMap;
    std::vector<double> evaluationCacheVector;

    // KleeneCachingType describes which of the two (if any)
    // datastructures is used to cache computed values on Kleene
    // states
    CachingType kleeneCachingType;
    std::map<long, double> kleeneEvaluationCacheMap;
    std::vector<double> kleeneEvaluationCacheVector;

    // ActionHashKeyMap contains the hash keys of the actions that
    // influence this Evaluatable
    std::vector<long> actionHashKeyMap;

private:
    // These function are used to calculate the two parts of state
    // fluent hash keys: the action part (that is stored in the
    // actionHashKeyMap of Evaluatable), and the state fluent part
    // (that is stored in PlanningTask).
    void initializeActionHashKeys(std::vector<ActionState> const& actionStates);
    void calculateActionHashKey(std::vector<ActionState> const& actionStates, ActionState const& action, long& nextKey);
    long getActionHashKey(std::vector<ActionState> const& actionStates, std::vector<ActionFluent*>& scheduledActions);

    void initializeStateFluentHashKeys(std::vector<ConditionalProbabilityFunction*> const& CPFs,
                                       std::vector<std::vector<std::pair<int,long> > >& indexToStateFluentHashKeyMap);
    void initializeKleeneStateFluentHashKeys(std::vector<ConditionalProbabilityFunction*> const& CPFs,
                                             std::vector<std::vector<std::pair<int,long> > >& indexToKleeneStateFluentHashKeyMap);
    bool dependsOnActionFluent(ActionFluent* fluent) {
        return (positiveActionDependencies.find(fluent) != positiveActionDependencies.end() ||
                negativeActionDependencies.find(fluent) != negativeActionDependencies.end());
    }

    // This is a temporary variable used in evaluate. It is a member
    // var as initializing variables in case statements leads to
    // strange syntactical rules (we'd need parens)
    long stateHashKey;

    // This is the hash key base of the first
    // state fluent, which depends on the number of dependent actions
    // (and is the same for states and Kleene states and thus saved)
    long firstStateFluentHashKeyBase;
};

#endif