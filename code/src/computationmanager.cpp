//     ____  __________     ___   ____ ___  _____ //
//    / __ \/ ____/ __ \   |__ \ / __ \__ \|__  / //
//   / /_/ / /   / / / /   __/ // / / /_/ / /_ <  //
//  / ____/ /___/ /_/ /   / __// /_/ / __/___/ /  //
// /_/    \____/\____/   /____/\____/____/____/   //
// Auteurs : Prénom Nom, Prénom Nom


// A vous de remplir les méthodes, vous pouvez ajouter des attributs ou méthodes pour vous aider
// déclarez les dans ComputationManager.h et définissez les méthodes ici.
// Certaines fonctions ci-dessous ont déjà un peu de code, il est à remplacer, il est là temporairement
// afin de faire attendre les threads appelants et aussi afin que le code compile.

#include "computationmanager.h"


ComputationManager::ComputationManager(int maxQueueSize): MAX_TOLERATED_QUEUE_SIZE(maxQueueSize)
{
    // TODO
}

int ComputationManager::requestComputation(Computation c) {
    monitorIn();

    while (requests[c.computationType].size() >= MAX_TOLERATED_QUEUE_SIZE) {
        if (stopped) {
            monitorOut();
            throwStopException();
        }

        wait(fullConditions[c.computationType]);
    }

    auto const id = nextId++;
    requests[c.computationType].emplace(c, id);
    results.emplace_front(id, std::nullopt);

    signal(emptyConditions[c.computationType]);

    monitorOut();
    return id;
}

void ComputationManager::abortComputation(int id) {
    // TODO
}

Result ComputationManager::getNextResult() {
    // TODO
    // Replace all of the code below by your code

    // Filled with some code in order to make the thread in the UI wait
    monitorIn();
    auto c = Condition();
    wait(c);
    monitorOut();

    return Result(-1, 0.0);
}

Request ComputationManager::getWork(ComputationType computationType) {
    monitorIn();

    // FIXME: should be if ?
    while (requests[computationType].empty()) {
        if (stopped) {
            monitorOut();
            throwStopException();
        }

        wait(emptyConditions[computationType]);
    }

    auto const r = requests[computationType].front();
    requests[computationType].pop();

    signal(fullConditions[computationType]);

    monitorOut();
    return r;
}

bool ComputationManager::continueWork(int id) {
    // TODO
    return true;
}

void ComputationManager::provideResult(Result result) {
    // TODO
}

void ComputationManager::stop() {
    // TODO
}
