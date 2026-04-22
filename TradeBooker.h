#include <iostream>
#include <unordered_set>
#include <string>
#include <mutex>

class TradeBooker {
private:
    // In production, this would be a persistent Key-Value store like RocksDB
    std::unordered_set<std::string> processedExecIDs;
    std::mutex mtx;

public:
    void onExecutionReport(const std::string& execID, double price, int qty) {
        std::lock_guard<std::mutex> lock(mtx);

        // 1. Deduplication Check
        if (processedExecIDs.find(execID) != processedExecIDs.end()) {
            std::cout << "Duplicate Trade Ignored: " << execID << std::endl;
            return;
        }

        // 2. Persist to Disk/DB (Synchronous or WAL)
        if (persistToDatabase(execID, price, qty)) {
            // 3. Update In-Memory State
            processedExecIDs.insert(execID);
            updatePositions(price, qty);
            std::cout << "Trade Booked Successfully: " << execID << std::endl;
        }
    }

private:
    bool persistToDatabase(const std::string& id, double p, int q) {
        // Logic to ensure data hits non-volatile storage
        return true; 
    }

    void updatePositions(double p, int q) {
        // Business logic for position management
    }
};