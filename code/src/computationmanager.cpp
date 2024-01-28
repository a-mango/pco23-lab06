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

ComputationManager::ComputationManager(int maxQueueSize): MAX_TOLERATED_QUEUE_SIZE(maxQueueSize)
{
    // TODO
}

int ComputationManager::requestComputation(Computation c) {
    monitorIn();

    // FIXME: could check for stop here but not huge improvement, may be
    // mentioned in report.

    // Check if the queue is full and if so, wait for it to be not full.
    if (requestsBuffer[c.computationType].size() >= MAX_TOLERATED_QUEUE_SIZE) {
        if (stopped) {
            monitorOut();
            throwStopException();
        }

        wait(notFullConditions[c.computationType]);

        // Re-checking is mandatory here since the condition may have been
        // signaled by the stop() method while we were waiting.
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

    // Remove the result from the results queue.
    auto const toRemove = std::remove_if(
        resultsQueue.begin(), resultsQueue.end(), [id](auto const& r) {
            return r.id == id;
        });

    if (toRemove == resultsQueue.end()) {
        monitorOut();
        return;
    }

    resultsQueue.erase(toRemove, resultsQueue.end());

    // Signal that there is a new result if appropriate.
    if (!resultsQueue.empty() && resultsQueue.back().value.has_value()) {
        signal(resultAvailable);
    }

    // Look in each request queue for the request with the given id. If found,
    // remove it and signal the notFull condition.
    auto const findRequestById = [id](auto const& r) {return r.getId() == id;};
    for (std::size_t i = 0; i < TYPE_COUNT; ++i) {
        auto& queue = requestsBuffer[i];
        queue.erase(
            std::remove_if(queue.begin(), queue.end(), findRequestById),
            queue.end()
        );

        // Signal that there is now room in the queue if appropriate.
        signal(notFullConditions[i]);
    }

    monitorOut();
}

Result ComputationManager::getNextResult() {
    monitorIn();

    // Check whether the program has stopped since waking from wait.
    while (!stopped) {
        if (!resultsQueue.empty()) {
            const auto& result = resultsQueue.back().value;
            if (result.has_value()) {
                resultsQueue.pop_back();
                // Wake up the next result thread if there is already a result.
                if (!resultsQueue.empty() &&
                    resultsQueue.back().value.has_value()) {
                    signal(resultAvailable);
                }
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

    // FIXME: could check for stop here but not huge improvement, may be
    // mentioned in report.

    // Check whether the buffer is empty and if so, wait for it to be not empty.
    while (requestsBuffer[computationType].empty()) {
        if (stopped) {
            monitorOut();
            throwStopException();
        }

        wait(notEmptyConditions[computationType]);

        // Re-checking is mandatory here since the condition may have been
        // signaled by the stop() method while we were waiting.
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
    auto const hasResult = [id](auto const& r) {return r.id == id;};
    auto const inProgress =
        std::any_of(resultsQueue.begin(), resultsQueue.end(), hasResult);

    monitorOut();
    return inProgress;
}

void ComputationManager::provideResult(Result result) {
    monitorIn();

    // Find the result based on its id.
    auto const it = std::find_if(
        resultsQueue.begin(), resultsQueue.end(), [result](auto const& r) {
            return r.id == result.getId();
        });

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

    // Wake up all conditions so that threads may exit.
    auto const signalThread = [this](auto& c) {signal(c);};
    std::for_each(
        notEmptyConditions.begin(), notEmptyConditions.end(), signalThread);
    std::for_each(
        notFullConditions.begin(), notFullConditions.end(), signalThread);
    signal(resultAvailable);

    monitorOut();
}
