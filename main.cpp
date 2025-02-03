#include <iostream>
#include <map>
#include <set>
#include <list>
#include <cmath>
#include <ctime>
#include <deque>
#include <queue>
#include <stack>
#include <limits>
#include <string>
#include <vector>
#include <numeric>
#include <algorithm>
#include <unordered_map>
#include <memory>
#include <variant>
#include <optional>
#include <tuple>

/**
 * OrderBook System Architecture
 * 
 * This implementation represents a limit order book system commonly used in financial trading.
 * The system maintains two primary order lists:
 * 1. Bids (buy orders) - sorted in descending order by price
 * 2. Asks (sell orders) - sorted in ascending order by price
 * 
 * Key Components:
 * - Order matching engine with price-time priority
 * - Support for GoodTilCancel and FillAndKill order types
 * - Real-time order book level information
 * - Order modification and cancellation capabilities
 * 
 * Performance Considerations:
 * - Uses std::map for price levels (O(log n) for insertions/deletions)
 * - Uses std::list for orders at each price level (O(1) for insertions/deletions)
 * - Uses std::unordered_map for order lookup by ID (O(1) average case)
 */

enum class OrderType {
    GoodTilCancel,
    FillAndKill
};

/**
 * Side indicates whether the order is a buy or sell order
 */
enum class Side {
    Buy,
    Sell
};

// Type aliases for better code readability and maintenance
using Price = std::int32_t;     // Signed integer for price to allow for negative values
using Quantity = std::uint64_t;  // Unsigned integer for quantity (cannot be negative)
using OrderId = std::uint64_t;   // Unique identifier for orders

/**
 * LevelInfo represents aggregated information for a price level
 * Contains the price and total quantity of all orders at that price
 */
struct LevelInfo {
    Price price;
    Quantity quantity;

    LevelInfo(Price p, Quantity q) : price(p), quantity(q) {}
};

using LevelInfos = std::vector<LevelInfo>;

/**
 * OrderbookLevelInfos provides a snapshot of the entire order book
 * Contains vectors of LevelInfo for both bid and ask sides
 */
class OrderbookLevelInfos {
public:
    OrderbookLevelInfos(const LevelInfos& bids, const LevelInfos& asks) 
        : bids(bids), asks(asks) {}

    const LevelInfos& GetBids() const { return bids; }
    const LevelInfos& GetAsks() const { return asks; }

private:
    LevelInfos bids;  // Bid price levels sorted high to low
    LevelInfos asks;  // Ask price levels sorted low to high
};

/**
 * Order class represents a single order in the system
 * Contains all essential order information and methods to manage its lifecycle
 */
class Order {
public:
    Order(OrderType type, OrderId id, Side s, Price p, Quantity q) 
        : orderType(type), orderId(id), side(s), price(p),
          initialQuantity(q), remainingQuantity(q) {}

    // Getters for order properties
    OrderId GetOrderId() const { return orderId; }
    Side GetSide() const { return side; }
    Price GetPrice() const { return price; }
    OrderType GetOrderType() const { return orderType; }
    Quantity GetInitialQuantity() const { return initialQuantity; }
    Quantity GetRemainingQuantity() const { return remainingQuantity; }
    Quantity GetFilledQuantity() const { return initialQuantity - remainingQuantity; }
    bool IsFilled() const { return remainingQuantity == 0; }

    /**
     * Fill a portion of the order
     * @throws std::runtime_error if fill quantity exceeds remaining quantity
     */
    void Fill(Quantity quantity) {
        if (quantity > remainingQuantity) {
            throw std::runtime_error(
                "Order (" + std::to_string(orderId) + 
                ") cannot be filled for more than its remaining quantity.");
        }
        remainingQuantity -= quantity;
    }

private:
    OrderType orderType;
    OrderId orderId;
    Side side;
    Price price;
    Quantity initialQuantity;
    Quantity remainingQuantity;
};

// Smart pointer aliases for memory management
using OrderPtr = std::shared_ptr<Order>;
using OrderList = std::list<OrderPtr>;

/**
 * OrderModify represents a request to modify an existing order
 * Used to change price or quantity of an existing order
 */
class OrderModify {
public:
    OrderModify(OrderId id, Side s, Price p, Quantity q) 
        : orderId(id), side(s), price(p), quantity(q) {}

    OrderId GetOrderId() const { return orderId; }
    Side GetSide() const { return side; }
    Price GetPrice() const { return price; }
    Quantity GetQuantity() const { return quantity; }

    /**
     * Creates a new Order object with modified parameters
     */
    OrderPtr ToOrderPtr(OrderType type) const {
        return std::make_shared<Order>(type, orderId, side, price, quantity);
    }

private:
    OrderId orderId;
    Side side;
    Price price;
    Quantity quantity;
};

/**
 * TradeInfo represents one side of a trade (either buy or sell)
 * Contains the order ID, executed price, and quantity
 */
struct TradeInfo {
    OrderId orderId;
    Price price;
    Quantity quantity;

    TradeInfo(OrderId id, Price p, Quantity q) 
        : orderId(id), price(p), quantity(q) {}
};

/**
 * Trade represents a matched trade between a buy and sell order
 * Contains TradeInfo for both sides of the trade
 */
class Trade {
public:
    Trade(const TradeInfo& bid, const TradeInfo& ask) 
        : bidTrade(bid), askTrade(ask) {}

    const TradeInfo& GetBidTrade() const { return bidTrade; }
    const TradeInfo& GetAskTrade() const { return askTrade; }

private:
    TradeInfo bidTrade;
    TradeInfo askTrade;
};

using Trades = std::vector<Trade>;

/**
 * OrderBook is the main class that manages the entire order book system
 * Handles order addition, modification, cancellation, and matching
 */
class OrderBook {
private:
    /**
     * OrderEntry stores an order and its location in the order list
     * Used for efficient order cancellation and modification
     */
    struct OrderEntry {
        OrderPtr order;
        typename OrderList::iterator location;

        OrderEntry(OrderPtr o, typename OrderList::iterator loc) 
            : order(o), location(loc) {}
    };

    using OrderMap = std::map<Price, OrderList>;
    OrderMap bids;  // Price-ordered map of bid orders
    OrderMap asks;  // Price-ordered map of ask orders
    std::unordered_map<OrderId, OrderEntry> orders;  // Quick lookup by order ID

    /**
     * Checks if an order can be matched at the given price
     * @returns true if the order can be matched with existing orders
     */
    bool CanMatch(Side side, Price price) const {
        if (side == Side::Buy) {
            if (asks.empty()) return false;
            return price >= asks.begin()->first;
        } else {
            if (bids.empty()) return false;
            return price <= bids.begin()->first;
        }
    }

    /**
     * Helper function to process order insertion
     * Adds order to the appropriate price level and maintains order book structure
     */
    void ProcessOrder(OrderPtr order, OrderMap& orderMap) {
        OrderList& orderList = orderMap[order->GetPrice()];
        orderList.push_back(order);
        auto it = std::prev(orderList.end());
        orders.emplace(order->GetOrderId(), OrderEntry(order, it));
    }

    /**
     * Core matching engine that pairs compatible buy and sell orders
     * Implements price-time priority matching algorithm
     * @returns vector of executed trades
     */
    Trades MatchOrders() {
        Trades trades;
        trades.reserve(orders.size());

        while (!bids.empty() && !asks.empty()) {
            auto bidIt = bids.begin();
            auto askIt = asks.begin();
            
            if (bidIt->first < askIt->first) break;

            OrderList& bidList = bidIt->second;
            OrderList& askList = askIt->second;

            while (!bidList.empty() && !askList.empty()) {
                OrderPtr bid = bidList.front();
                OrderPtr ask = askList.front();

                Quantity quantity = std::min(
                    bid->GetRemainingQuantity(),
                    ask->GetRemainingQuantity()
                );

                bid->Fill(quantity);
                ask->Fill(quantity);

                trades.emplace_back(
                    TradeInfo(bid->GetOrderId(), bid->GetPrice(), quantity),
                    TradeInfo(ask->GetOrderId(), ask->GetPrice(), quantity)
                );

                if (bid->IsFilled()) {
                    bidList.pop_front();
                    orders.erase(bid->GetOrderId());
                }
                if (ask->IsFilled()) {
                    askList.pop_front();
                    orders.erase(ask->GetOrderId());
                }
                
                if (bidList.empty()) bids.erase(bidIt);
                if (askList.empty()) asks.erase(askIt);
            }
        }

        // Handle unfilled FillAndKill orders
        for (const auto& orderPair : bids) {
            const OrderList& orderList = orderPair.second;
            if (!orderList.empty() && 
                orderList.front()->GetOrderType() == OrderType::FillAndKill) {
                CancelOrder(orderList.front()->GetOrderId());
            }
        }

        for (const auto& orderPair : asks) {
            const OrderList& orderList = orderPair.second;
            if (!orderList.empty() && 
                orderList.front()->GetOrderType() == OrderType::FillAndKill) {
                CancelOrder(orderList.front()->GetOrderId());
            }
        }

        return trades;
    }

public:
    /**
     * Adds a new order to the book
     * @returns vector of trades if order was matched
     */
    Trades AddOrder(OrderPtr order) {
        if (orders.find(order->GetOrderId()) != orders.end()) {
            return Trades();
        }

        if (order->GetOrderType() == OrderType::FillAndKill && 
            !CanMatch(order->GetSide(), order->GetPrice())) {
            return Trades();
        }

        if (order->GetSide() == Side::Buy) {
            ProcessOrder(order, bids);
        } else {
            ProcessOrder(order, asks);
        }

        return MatchOrders();
    }

    /**
     * Cancels an existing order
     * Removes order from both price level and ID lookup
     */
    void CancelOrder(OrderId orderId) {
        auto it = orders.find(orderId);
        if (it == orders.end()) return;

        OrderPtr order = it->second.order;
        auto& orderMap = (order->GetSide() == Side::Buy) ? bids : asks;
        
        OrderList& orderList = orderMap[order->GetPrice()];
        orderList.erase(it->second.location);
        
        if (orderList.empty()) {
            orderMap.erase(order->GetPrice());
        }
        
        orders.erase(orderId);
    }

    /**
     * Modifies an existing order
     * Implements modification as cancel-and-replace
     * @returns vector of trades if modified order was matched
     */
    Trades ModifyOrder(const OrderModify& modify) {
        auto it = orders.find(modify.GetOrderId());
        if (it == orders.end()) return Trades();

        OrderType type = it->second.order->GetOrderType();
        CancelOrder(modify.GetOrderId());
        return AddOrder(modify.ToOrderPtr(type));
    }

    /**
     * @returns current number of active orders in the book
     */
    std::size_t Size() const { 
        return orders.size(); 
    }

    /**
     * Creates a snapshot of current order book state
     * @returns aggregated level information for both sides of the book
     */
    OrderbookLevelInfos GetOrderInfos() const {
        LevelInfos bidInfos, askInfos;
        
        for (const auto& [price, orderList] : bids) {
            Quantity total = 0;
            for (const auto& order : orderList) {
                total += order->GetRemainingQuantity();
            }
            bidInfos.emplace_back(price, total);
        }
        
        for (const auto& [price, orderList] : asks) {
            Quantity total = 0;
            for (const auto& order : orderList) {
                total += order->GetRemainingQuantity();
            }
            askInfos.emplace_back(price, total);
        }

        return OrderbookLevelInfos(bidInfos, askInfos);
    }
};

/**
 * Example usage of the OrderBook system
 */
int main() {
    OrderBook orderbook;
    
    // Create and add a new order
    const OrderId orderId = 1;
    auto order = std::make_shared<Order>(
        OrderType::GoodTilCancel, 
        orderId, 
        Side::Buy, 
        100,  // price
        10    // quantity
    );
    
    // Add order and print size
    orderbook.AddOrder(order);
    std::cout << "Order count: " << orderbook.Size() << std::endl;
    
    // Cancel order and print size
    orderbook.CancelOrder(orderId);
    std::cout << "Order count after cancel: " << orderbook.Size() << std::endl;

    return 0;
}
