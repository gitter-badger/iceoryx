#include "iceoryx_posh/iceoryx_posh_config.hpp"
#include "iceoryx_posh/internal/roudi/roudi_multi_process.hpp"
#include "iceoryx_posh/popo/publisher.hpp"
#include "iceoryx_posh/popo/subscriber.hpp"
#include "iceoryx_posh/roudi/iceoryx_roudi_components.hpp"
#include "iceoryx_posh/runtime/posh_runtime_single_process.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>

std::atomic_bool keepRunning{true};

struct TransmissionData_t
{
    uint64_t counter;
};

void consoleOutput(const std::string& output)
{
    static std::mutex consoleOutputMutex;

    std::lock_guard<std::mutex> lock(consoleOutputMutex);
    std::cout << output << std::endl;
}

void sender()
{
    iox::popo::Publisher publisher({"Single", "Process", "Demo"});
    publisher.offer();

    uint64_t counter{0};
    while (keepRunning.load())
    {
        auto sample = static_cast<TransmissionData_t*>(publisher.allocateChunk(sizeof(TransmissionData_t)));
        sample->counter = counter++;
        consoleOutput(std::string("Sending: " + std::to_string(sample->counter)));
        publisher.sendChunk(sample);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void receiver()
{
    iox::popo::Subscriber subscriber({"Single", "Process", "Demo"});

    uint64_t cacheQueueSize = 10;
    subscriber.subscribe(cacheQueueSize);

    while (keepRunning.load())
    {
        if (iox::popo::SubscriptionState::SUBSCRIBED == subscriber.getSubscriptionState())
        {
            const void* rawSample = nullptr;
            while (subscriber.getChunk(&rawSample))
            {
                auto sample = static_cast<const TransmissionData_t*>(rawSample);
                consoleOutput(std::string("Receiving : " + std::to_string(sample->counter)));
                subscriber.releaseChunk(rawSample);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main()
{
    iox::RouDiConfig_t defaultRouDiConfig = iox::RouDiConfig_t().setDefaults();
    iox::roudi::IceOryxRouDiComponents roudiComponents(defaultRouDiConfig);

    iox::roudi::RouDiMultiProcess roudi(
        roudiComponents.m_rouDiMemoryManager, roudiComponents.m_portManager, iox::roudi::MonitoringMode::OFF, false);

    iox::runtime::PoshRuntimeSingleProcess runtime("/singleProcessDemo");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::thread receiverThread(receiver), senderThread(sender);

    std::this_thread::sleep_for(std::chrono::seconds(2));
    keepRunning.store(false);
    senderThread.join();
    receiverThread.join();
}
