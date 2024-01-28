//     ____  __________     ___   ____ ___  _____ //
//    / __ \/ ____/ __ \   |__ \ / __ \__ \|__  / //
//   / /_/ / /   / / / /   __/ // / / /_/ / /_ <  //
//  / ____/ /___/ /_/ /   / __// /_/ / __/___/ /  //
// /_/    \____/\____/   /____/\____/____/____/   //
// Auteurs : Timothée Van Hove, Aubry Mangold


// A vous de remplir les méthodes, vous pouvez ajouter des attributs ou méthodes pour vous aider
// déclarez les dans ComputationManager.h et définissez les méthodes ici.
// Certaines fonctions ci-dessous ont déjà un peu de code, il est à remplacer, il est là temporairement
// afin de faire attendre les threads appelants et aussi afin que le code compile.

#include "computationmanager.h"

#include <algorithm>

ComputationManager::ComputationManager(int maxQueueSize) : MAX_TOLERATED_QUEUE_SIZE(maxQueueSize) {}

int ComputationManager::requestComputation(Computation c) {
    monitorIn();

    if (stopped) {
        monitorOut();
        throwStopException();
    }

    // Check if the queue is full and if so, wait for it to be not full.
    if (requestsBuffer[c.computationType].size() >= MAX_TOLERATED_QUEUE_SIZE) {
        wait(notFullConditions[c.computationType]);

        // Re-checking is mandatory here since the condition may have been signaled by the stop() method.
        if (stopped) {
            signal(notFullConditions[c.computationType]);
            monitorOut();
            throwStopException();
        }
    }

    // Insert the request in the queue and prepare a result for it.
    auto const id = nextId++;
    requestsBuffer[c.computationType].emplace_back(c, id);
    resultsQueue.emplace_front(id);

    // Signal that the queue is not empty.
    signal(notEmptyConditions[c.computationType]);

    monitorOut();
    return id;
}

void ComputationManager::abortComputation(int id) {
    monitorIn();

    // Check whether the program is already stopped.
    if (stopped) {
        monitorOut();
        return;
    }

    // Remove the result from the results queue. By design, all known identifiers are in the results queue.
    auto const resultToRemove =
        std::remove_if(resultsQueue.begin(), resultsQueue.end(), [id](auto const& r) { return r.id == id; });

    // Avoid unnecessary work if the id wasn't found i.e. if it is incorrect.
    if (resultToRemove == resultsQueue.end()) {
        monitorOut();
        return;
    }

    resultsQueue.erase(resultToRemove, resultsQueue.end());

    // Look in each buffer for the request with the given id. If found, remove it and signal the notFull condition.
    for (std::size_t i = 0; i < TYPE_COUNT; ++i) {
        auto& queue = requestsBuffer[i];
        auto const requestToRemove = std::remove_if(queue.begin(), queue.end(), [id](auto const& r) { return r.getId() == id; });

        // Erase the request and signal if it was found in the current queue.
        if (requestToRemove != queue.end()) {
            queue.erase(requestToRemove, queue.end());
            signal(notFullConditions[i]);

            break; // No need to continue if the request was found.
        }
    }

    monitorOut();
}

Result ComputationManager::getNextResult() {
    monitorIn();

    if (stopped) {
        monitorOut();
        throwStopException();
    }

    // Check whether the result is available and if not, wait for it to be.
    // Note: a while loop is used because the finished workloads do not come in order. Hence, a wait with a result
    // in second position may be signaled, but we still need to wait for another signal to provide the first result.
    while (resultsQueue.empty() || !resultsQueue.back().value.has_value()) {
        wait(resultAvailable);

        // Re-checking is mandatory here since the condition may have been signaled by the stop() method.
        if (stopped) {
            signal(resultAvailable);
            monitorOut();
            throwStopException();
        }
    }

    const auto& result = resultsQueue.back().value;
    resultsQueue.pop_back();

    monitorOut();
    return result.value();
}

Request ComputationManager::getWork(ComputationType computationType) {
    monitorIn();

    if (stopped) {
        monitorOut();
        throwStopException();
    }

    // Check whether the buffer is empty and if so, wait for it to be not empty.
    if (requestsBuffer[computationType].empty()) {
        wait(notEmptyConditions[computationType]);

        // Re-checking is mandatory here since the condition may have been signaled by the stop() method.
        if (stopped) {
            signal(notEmptyConditions[computationType]);
            monitorOut();
            throwStopException();
        }
    }

    // Extract the request from the queue and signal that the queue is not full.
    auto const request = requestsBuffer[computationType].front();
    requestsBuffer[computationType].pop_front();
    signal(notFullConditions[computationType]);

    monitorOut();
    return request;
}

bool ComputationManager::continueWork(int id) {
    monitorIn();

    // Check whether the program should continue work or not.
    if (stopped) {
        monitorOut();
        return false;
    }

    // Check whether the result for the work request is already available.
    auto const inProgress =
        std::any_of(resultsQueue.begin(), resultsQueue.end(), [id](auto const& r) { return r.id == id; });

    monitorOut();
    return inProgress;
}

void ComputationManager::provideResult(Result result) {
    monitorIn();

    // Find the result based on its id.
    auto const it = std::find_if(
        resultsQueue.begin(), resultsQueue.end(), [result](auto const& r) { return r.id == result.getId(); });

    // If the result was found, update the optional value.
    if (it != resultsQueue.end()) {
        it->value = result;
        signal(resultAvailable);
    }

    monitorOut();
}

void ComputationManager::stop() {
    monitorIn();

    stopped = true;

    // Start to cascade wake-up calls to all conditions so that threads may exit.
    auto const signalThread = [this](auto& c) { signal(c); };
    std::for_each(notEmptyConditions.begin(), notEmptyConditions.end(), signalThread);
    std::for_each(notFullConditions.begin(), notFullConditions.end(), signalThread);
    signal(resultAvailable);

    monitorOut();
}
