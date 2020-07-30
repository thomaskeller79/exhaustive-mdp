#include "simplifier.h"

#include "csp.h"
#include "evaluatables.h"
#include "fdr_generation.h"
#include "mutex_detection.h"
#include "rddl.h"
#include "reachability_analysis.h"

#include "utils/system_utils.h"
#include "utils/timer.h"

#include <algorithm>
#include <iostream>

using namespace std;

Simplifier::Simplifier(RDDLTask* _task)
    : task(_task), numGeneratedFDRActionFluents(0) {
    fdrGen = make_shared<GreedyFDRGenerator>(task);
}

void Simplifier::simplify(bool generateFDRActionFluents, bool output) {
    Timer t;
    Simplifications replacements;
    bool continueSimplification = true;
    int iteration = 0;
    while (continueSimplification) {
        ++iteration;

        if (output) {
            cout << "    Simplify formulas (" << iteration << ")..." << endl;
        }
        simplifyFormulas(replacements);
        if (output) {
            cout << "    ...finished (" << t() << ")" << endl;
        }
        t.reset();

        if (output) {
            cout << "    Compute inapplicable action fluents (" << iteration
                 << ")..." << endl;
        }
        continueSimplification = computeInapplicableActionFluents(replacements);
        if (output) {
            cout << "    ...finished (" << t() << ")" << endl;
        }
        t.reset();
        if (continueSimplification) {
            continue;
        }

        if (output) {
            cout << "    Compute relevant action fluents (" << iteration
                 << ")..." << endl;
        }
        continueSimplification = computeRelevantActionFluents(replacements);
        if (output) {
            cout << "    ...finished (" << t() << ")" << endl;
        }
        t.reset();
        if (continueSimplification) {
            continue;
        }

        if (generateFDRActionFluents) {
            if (output) {
                cout << "    Determine FDR action fluents (" << iteration
                     << ")..." << endl;
            }
            continueSimplification =
                determineFiniteDomainActionFluents(replacements);
            if (output) {
                cout << "    ...finished (" << t() << ")" << endl;
            }
            t.reset();
            if (continueSimplification) {
                continue;
            }
        }

        if (output) {
            cout << "    Compute actions (" << iteration << ")..." << endl;
        }
        continueSimplification = computeActions(replacements);
        if (output) {
            cout << "    ...finished (" << t() << ")" << endl;
        }
        t.reset();
        if (continueSimplification) {
            continue;
        }

        if (output) {
            cout << "    Approximate domains (" << iteration << ")..." << endl;
        }
        continueSimplification = approximateDomains(replacements);
        if (output) {
            cout << "    ...finished (" << t() << ")" << endl;
        }
        t.reset();
    }

    if (output) {
        cout << "    Initialize action states..." << endl;
    }
    initializeActionStates();
    if (output) {
        cout << "    ...finished (" << t() << ")" << endl;
    }
}

void Simplifier::simplifyFormulas(Simplifications& replacements) {
    simplifyCPFs(replacements);
    task->rewardCPF->simplify(replacements);
    simplifyPreconditions(replacements);
}

void Simplifier::simplifyCPFs(Simplifications& replacements) {
    bool continueSimplification = true;
    while (continueSimplification) {
        continueSimplification = false;
        for (auto it = task->CPFs.begin(); it != task->CPFs.end(); ++it) {
            ConditionalProbabilityFunction* cpf = *it;
            // Simplify by replacing state fluents whose CPF simplifies to their
            // initial value with their initial value
            cpf->simplify(replacements);

            // Check if this CPF now also simplifies to its initial value
            auto nc = dynamic_cast<NumericConstant*>(cpf->formula);
            double initialValue = cpf->head->initialValue;
            if (nc && MathUtils::doubleIsEqual(initialValue, nc->value)) {
                assert(replacements.find(cpf->head) == replacements.end());
                replacements[cpf->head] = nc;
                task->CPFs.erase(it);
                continueSimplification = true;
                break;
            }
        }
    }
    task->sortCPFs();
}

void Simplifier::simplifyPreconditions(Simplifications& replacements) {
    vector<LogicalExpression*> simplifiedSACs;
    for (LogicalExpression* precondition : task->SACs) {
        LogicalExpression* precond = precondition->simplify(replacements);
        if (auto conj = dynamic_cast<Conjunction*>(precond)) {
            // Split the conjunction into separate preconditions
            simplifiedSACs.insert(
                simplifiedSACs.end(), conj->exprs.begin(), conj->exprs.end());
        } else if (auto nc = dynamic_cast<NumericConstant*>(precond)) {
            // This precond is either never satisfied or always
            if (MathUtils::doubleIsEqual(nc->value, 0.0)) {
                SystemUtils::abort("Found a precond that is never satisified!");
            }
        } else {
            simplifiedSACs.push_back(precond);
        }
    }
    task->SACs = move(simplifiedSACs);
}

bool Simplifier::computeInapplicableActionFluents(
    Simplifications& replacements) {
    // TODO: since preconditions exist on two levels (as LogicalFormulas in
    //  task->SACs and as Preconditions in task->actionPreconds and
    //  task->staticSACs), we have to clear the latter and rebuild them here.
    //  The fact that preconditions exist twice is bad design and should be
    //  fixed at some point.
    task->actionPreconds.clear();
    task->staticSACs.clear();

    vector<bool> fluentIsApplicable = classifyActionPreconditions();
    return filterActionFluents(fluentIsApplicable, replacements);
}

vector<bool> Simplifier::classifyActionPreconditions() {
    vector<bool> result(task->actionFluents.size(), true);
    vector<LogicalExpression*> SACs;
    for (size_t index = 0; index < task->SACs.size(); ++index) {
        LogicalExpression* sac = task->SACs[index];
        auto precond = new ActionPrecondition("SAC " + to_string(index), sac);

        // Collect the properties of the SAC that are necessary for distinction
        // of action preconditions, (primitive) static SACs and state invariants
        precond->initialize();
        if (!precond->containsStateFluent()) {
            int afIndex = precond->triviallyForbidsActionFluent();
            if (afIndex >= 0) {
                // This is a static action constraint of the form "not a" for
                // some action fluent "a" that trivially forbids the application
                // of "a" in every state, i.e. it must be false in every
                // applicable action.
                result[afIndex] = false;
            } else {
                // An SAC that only contains action fluents is used to
                // statically forbid action combinations.
                task->staticSACs.push_back(precond);
                SACs.push_back(sac);
            }
        } else if (!precond->isActionIndependent()) {
            precond->index = task->actionPreconds.size();
            task->actionPreconds.push_back(precond);
            SACs.push_back(sac);
        }
    }
    task->SACs = move(SACs);
    return result;
}

bool Simplifier::computeRelevantActionFluents(Simplifications& replacements) {
    vector<bool> fluentIsUsed = determineUsedActionFluents();
    return filterActionFluents(fluentIsUsed, replacements);
}

vector<bool> Simplifier::determineUsedActionFluents() {
    // TODO: If an action fluent is only used in SACs, it has no direct
    //  influence on states and rewards, but could be necessary to apply other
    //  action fluents. It might be possible to compile the action fluent into
    //  other action fluents in this case. (e.g., if a1 isn't part of any CPF
    //  but a2 is, and there is a CPF a2 => a1, we can compile a1 and a2 into a
    //  new action fluent a' where a'=1 means a1=1 and a2=1, and a'=0 means
    //  a1=0 and a2=0 (or a1=1 and a2=0, this is irrelevant in this example).
    vector<bool> result(task->actionFluents.size(), false);
    for (ActionPrecondition* precond : task->actionPreconds) {
        for (ActionFluent* af : precond->dependentActionFluents) {
            result[af->index] = true;
        }
    }
    for (ActionPrecondition* sac : task->staticSACs) {
        for (ActionFluent* af : sac->dependentActionFluents) {
            result[af->index] = true;
        }
    }
    for (ConditionalProbabilityFunction* cpf : task->CPFs) {
        for (ActionFluent* af : cpf->dependentActionFluents) {
            result[af->index] = true;
        }
    }
    for (ActionFluent* af : task->rewardCPF->dependentActionFluents) {
        result[af->index] = true;
    }
    return result;
}

bool Simplifier::determineFiniteDomainActionFluents(
    Simplifications& replacements) {
    TaskMutexInfo mutexInfo = computeActionVarMutexes(task);
    if (!mutexInfo.hasMutex()) {
        return false;
    }

    task->actionFluents = fdrGen->generateFDRVars(mutexInfo, replacements);
    task->sortActionFluents();
    return true;
}

bool Simplifier::computeActions(Simplifications& replacements) {
    task->sortActionFluents();
    task->actionStates = computeApplicableActions();
    task->sortActionStates();

    // Check if there are action fluents that have the same value in every legal
    // action. We can remove those action fluents from the problem if the value
    // is 0 (allowing other values is addressed in issue 112)
    vector<set<int>> usedValues(task->actionFluents.size());
    for (ActionState const& actionState : task->actionStates) {
        vector<int> const& state = actionState.state;
        for (size_t i = 0; i < state.size(); ++i) {
            usedValues[i].insert(state[i]);
        }
    }

    bool foundUnusedActionFluent = false;
    vector<ActionFluent*> relevantActionFluents;
    for (ActionFluent* var : task->actionFluents) {
        set<int> const& usedVals = usedValues[var->index];
        assert(!usedVals.empty());
        if (usedVals.size() == 1 && (usedVals.find(0) != usedVals.end())) {
            assert(replacements.find(var) == replacements.end());
            replacements[var] = new NumericConstant(0.0);
            foundUnusedActionFluent = true;
        } else {
            int newIndex = relevantActionFluents.size();
            relevantActionFluents.push_back(var);
            var->index = newIndex;
        }
    }
    task->actionFluents = move(relevantActionFluents);

    return foundUnusedActionFluent;
}

vector<ActionState> Simplifier::computeApplicableActions() const {
    // Compute models (i.e., assignments to the state and action variables) for
    // the planning task and use the assigned action variables as legal action
    // state (being part of the model means there is at least one state where
    // the assignment is legal) and invalidate the action assignment for the
    // next iteration. Teerminates when there are no more models.
    vector<ActionState> result;
    CSP csp(task);
    csp.addPreconditions();
    while (csp.hasSolution()) {
        result.emplace_back(csp.getActionModel());
        csp.invalidateActionModel();
    }
    return result;
}

bool Simplifier::approximateDomains(Simplifications& replacements) {
    MinkowskiReachabilityAnalyser r(task);
    vector<set<double>> domains = r.determineReachableFacts();
    return removeConstantStateFluents(domains, replacements);
}

bool Simplifier::removeConstantStateFluents(
    vector<set<double>> const& domains, Simplifications& replacements) {
    vector<ConditionalProbabilityFunction*> cpfs;
    bool sfRemoved = false;
    for (ConditionalProbabilityFunction* cpf : task->CPFs) {
        set<double> const& domain = domains[cpf->head->index];
        assert(!domain.empty());
        if (domain.size() == 1) {
            assert(replacements.find(cpf->head) == replacements.end());
            replacements[cpf->head] = new NumericConstant(*domain.begin());
            sfRemoved = true;
        } else {
            // We currently do not support removal of values, so we add all
            // values up to maxVal to the domain (see issue 115)
            cpf->setDomain(static_cast<int>(*domain.rbegin()));
            cpfs.push_back(cpf);
        }
    }
    task->CPFs = move(cpfs);
    task->sortCPFs();
    return sfRemoved;
}

void Simplifier::initializeActionStates() {
    // Check for each action state and precondition if that precondition could
    // forbid the application of the action state. We do this by checking if
    // there is a state where the precondition could be evaluated to false given
    // the action is applied.
    CSP csp(task);
    int numActions = task->actionStates.size();
    int numPreconds = task->actionPreconds.size();
    vector<bool> precondIsRelevant(numPreconds, false);
    for (size_t index = 0; index < numActions; ++index) {
        ActionState& action = task->actionStates[index];
        action.index = index;

        csp.push();
        csp.assignActionVariables(action.state);

        for (ActionPrecondition* precond : task->actionPreconds) {
            csp.push();
            csp.addConstraint(precond->formula->toZ3Formula(csp, 0) == 0);
            if (csp.hasSolution()) {
                action.relevantSACs.push_back(precond);
                precondIsRelevant[precond->index] = true;
            }
            csp.pop();
        }
        csp.pop();
    }

    // Remove irrelevant preconds
    vector<ActionPrecondition*> finalPreconds;
    for (ActionPrecondition* precond : task->actionPreconds) {
        if (precondIsRelevant[precond->index]) {
            precond->index = finalPreconds.size();
            finalPreconds.push_back(precond);
        }
    }
    task->actionPreconds = move(finalPreconds);
}

bool Simplifier::filterActionFluents(
    vector<bool> const& filter, Simplifications& replacements) {
    vector<ActionFluent*>& actionFluents = task->actionFluents;
    auto keep = [&](ActionFluent* af) {return filter[af->index];};
    // Separate action fluents that are kept from discarded ones
    auto partIt = partition(actionFluents.begin(), actionFluents.end(), keep);
    bool result = (partIt != actionFluents.end());
    // Add discarded action fluents to replacements
    for (auto it = partIt; it != actionFluents.end(); ++it) {
        assert(replacements.find(*it) == replacements.end());
        replacements[*it] = new NumericConstant(0.0);
    }
    // Erase discarded action fluents
    task->actionFluents.erase(partIt, task->actionFluents.end());
    // Sort action fluents that are kept and assign indices
    task->sortActionFluents();
    return result;
}
