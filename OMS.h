#include <iostream>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <cstring>

//two core C++ principles for low-latency systems:
// 1 zero dynamic memory allocation in the critical path (no new/delete) 
// 2 lock-free concurrency.
//System Architecture & C++ Design Choices
// Memory Pools: Instead of instantiating Order objects on the fly, you pre-allocate a massive array (pool) of Order structs at startup.
// String Avoidance: std::string causes heap allocations. We use fixed-size char arrays or 64-bit integers for Symbols and Order IDs.
// Threading Model: A single-threaded event loop pinned to an isolated CPU core for processing to avoid context switching and lock contention, communicating with network I/O threads via Single-Producer Single-Consumer (SPSC) lock-free queues.

// 1. Core Data Structures
enum class OrderState : uint8_t { NEW, PARTIALLY_FILLED, FILLED, CANCELED, REJECTED };
enum class Side : uint8_t { BUY, SELL };

// Cache-line aligned to prevent false sharing
struct alignas(64) Order {
    uint64_t cl_ord_id;       // Client Order ID
    char symbol[8];           // Fixed size string to avoid heap allocation
    Side side;
    double price;
    uint32_t quantity;
    uint32_t executed_qty;
    OrderState state;
    bool is_active;
};

// 2. The Order Management System
class OrderManagementSystem {
private:
    // Pre-allocated memory pool for orders
    std::vector<Order> order_pool;
    size_t next_free_index = 0;

    // Fast lookup table (In a real low-latency system, this might be a custom flat map)
    std::unordered_map<uint64_t, Order*> order_map;

public:
    OrderManagementSystem(size_t max_orders) {
        order_pool.resize(max_orders);
        order_map.reserve(max_orders);
    }

    // Critical Path: Handling a New Order
    void onNewOrder(uint64_t id, const char* sym, Side side, double price, uint32_t qty) {
        if (next_free_index >= order_pool.size()) {
            // Handle capacity error (out of critical path)
            return; 
        }

        // 1. Allocate from pool (O(1), no heap allocation)
        Order* order = &order_pool[next_free_index++];
        
        // 2. Initialize
        order->cl_ord_id = id;
        std::strncpy(order->symbol, sym, 8);
        order->side = side;
        order->price = price;
        order->quantity = qty;
        order->executed_qty = 0;
        order->state = OrderState::NEW;
        order->is_active = true;

        // 3. Store for O(1) lookup
        order_map[id] = order;

        // 4. Send to Risk Check / Exchange Routing (Simulated)
        routeToExchange(order);
    }

    // Critical Path: Handling an Execution/Fill from the exchange
    void onExecutionReport(uint64_t id, uint32_t fill_qty, double fill_price) {
        auto it = order_map.find(id);
        if (it != order_map.end()) {
            Order* order = it->second;
            
            order->executed_qty += fill_qty;
            
            if (order->executed_qty >= order->quantity) {
                order->state = OrderState::FILLED;
                order->is_active = false;
            } else {
                order->state = OrderState::PARTIALLY_FILLED;
            }

            // Publish trade to downstream systems (Position/Risk/P&L)
            publishTradeOutbound(order, fill_qty, fill_price);
        }
    }

private:
    void routeToExchange(Order* order) { /* Push to network queue */ }
    void publishTradeOutbound(Order* order, uint32_t qty, double price) { /* Push to internal queue */ }
};

// What is a Drop Copy?
// A Drop Copy is a secondary, read-only FIX connection provided by an exchange or broker. 
// the Drop Copy session streams a duplicate set of execution reports.

// How to ensure Exactly-Once processing (Idempotency):
// To guarantee exactly-once booking despite network failures, design a system 
// idempotent—meaning processing the same message 1 time or 100 times yields the exact same state.