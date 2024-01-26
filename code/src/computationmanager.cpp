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

#include <algorithm>

// FIXME: check if reds standard is 80 or 120 cols

ComputationManager::ComputationManager(int maxQueueSize): MAX_TOLERATED_QUEUE_SIZE(maxQueueSize)
{
    // TODO
}

int ComputationManager::requestComputation(Computation c) {
    monitorIn();

    if (buffers[c.computationType].size() >= MAX_TOLERATED_QUEUE_SIZE) {
        if (stopped) {
            monitorOut();
            throwStopException();
        }

        wait(notFullConditions[c.computationType]);

        if (stopped) {
            signal(notFullConditions[c.computationType]);
            monitorOut();
            throwStopException();
        }
    }

    // Insert the request in the queue and prepare a result for it.
    auto const id = nextId++;
    buffers[c.computationType].emplace_back(c, id);
    results.emplace_front(id, std::nullopt);

    signal(notEmptyConditions[c.computationType]);

    monitorOut();
    return id;
}

void ComputationManager::abortComputation(int id) {
    monitorIn();

    if (stopped) {
        monitorOut();
        return;
    }

    // Remove the result from the results queue.
    if (std::find_if(results.begin(), results.end(), [id](auto const& r) {return r.first == id;}) != results.end()) {
        results.erase(std::remove_if(results.begin(), results.end(), [id](auto const& r) {return r.first == id;}), results.end());
        if (!results.empty() && results.back().second.has_value()){
            signal(resultAvailable);
        }
    }

    // Look in each request queue for the request with the given id. If found, remove it and signal the notFull condition.
    for(std::size_t i = 0; i < TYPE_COUNT; ++i) {
        auto& queue = buffers[i];
        if (std::find_if(queue.begin(), queue.end(), [id](auto const& r) {return r.getId() == id;}) != queue.end()) {
            queue.erase(std::remove_if(queue.begin(), queue.end(), [id](auto const& r) {return r.getId() == id;}), queue.end());
            signal(notFullConditions[i]);
        }
    }

    monitorOut();
}

Result ComputationManager::getNextResult() {
    monitorIn();

    while (!stopped) {
        if (!results.empty()) {
            const auto& result = results.back().second;
            if (result.has_value()) {
                results.pop_back();
                monitorOut();
                return result.value();
            }
        }

        wait(resultAvailable);
    }

    monitorOut();
    throwStopException(); // FIXME: invert condition
}

Request ComputationManager::getWork(ComputationType computationType) {
    monitorIn();
    // FIXME: should be if ?
    while (buffers[computationType].empty()) {
        if (stopped) {
            monitorOut();
            throwStopException();
        }

        wait(notEmptyConditions[computationType]);

        if (stopped) {
            signal(notEmptyConditions[computationType]);
            monitorOut();
            throwStopException();
        }
    }

    auto const request = buffers[computationType].front();
    buffers[computationType].pop_front();

    signal(notFullConditions[computationType]);

    monitorOut();
    return request;
}

bool ComputationManager::continueWork(int id) {
    monitorIn();
    if (stopped) {
        monitorOut();
        return false;
    }

    auto const inProgress = std::any_of(results.begin(), results.end(), [id](auto const& r) {return r.first == id;});

    monitorOut();
    return inProgress;
}

void ComputationManager::provideResult(Result result) {
    monitorIn();

    // TODO: find a better way
    auto const it = std::find_if(results.begin(), results.end(), [result](auto const& r) {return r.first == result.getId();});
    if (it != results.end()) {
        it->second = result;
        signal(resultAvailable);
    }

    monitorOut();
}

void ComputationManager::stop() {
    monitorIn();

    stopped = true;

    auto const signalThread = [this](auto& c) {signal(c);};

    // Wake up all conditions.
    std::for_each(notEmptyConditions.begin(), notEmptyConditions.end(), signalThread);
    std::for_each(notFullConditions.begin(), notFullConditions.end(), signalThread);
    signal(resultAvailable);

    monitorOut();
}
