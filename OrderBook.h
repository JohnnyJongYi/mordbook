#include <iostream>
#include <unordered_map>
#include <stdexcept>
#include <map>
#include <list>
#include <functional>
#include <sstream>
#include <string_view>

namespace OB{

template<typename T>
void throw_inv_arg(const T& t, std::string_view s)
{
    std::ostringstream err_ss;
    err_ss << s << ", " << t;
    throw std::invalid_argument(err_ss.str());
}
using px_t = int32_t;
using amt_t = int32_t;
using id_t = u_int32_t;

enum class side_t : int8_t
{
    BUY = 0,
    SELL = 1,
    INVALID = 2,
};

side_t operator~(side_t s)
{   
    switch(s)
    {
        case side_t::BUY:
            return side_t::SELL;
        case side_t::SELL:
            return side_t::BUY;
        case side_t::INVALID:
            return side_t::INVALID;
        default:
            throw;
    }   
}

std::ostream& operator<<(std::ostream& os, side_t side)
{
    switch(side)
    {
        case side_t::BUY:
            os << "BUY";
            break;
        case side_t::SELL:
            os << "SELL";
            break;
        case side_t::INVALID:
            os << "INVALID";
            break;
        default:
            ;
    }   
    return os;
}
        
enum class status_t
{
    OPEN = 0,
    CANCELLED = 1,
    FILLED = 2,
};

std::ostream& operator<<(std::ostream& os, status_t status)
{
    switch(status)
    {
        case status_t::OPEN:
            os << "OPEN";
            break;
        case status_t::CANCELLED:
            os << "CANCELLED";
            break;
        case status_t::FILLED:
            os << "FILLED";
            break;
        default:
            ;
    }   
    return os;
}
constexpr side_t int_to_side(int i)
{
    switch(i)
    {
        case 0:
            return side_t::BUY;
        case 1:
            return side_t::SELL;
        default:
            throw_inv_arg(i, "invalid int in int_to_side");
    }
    assert(false);
    return side_t::INVALID;
}
enum Action 
{
    ADD = 0,
    UPD = 1,
    CXL = 2,
};     

struct Order
{
    id_t id{0};
    px_t px{-1};
    amt_t amt{-1};
    side_t side{side_t::INVALID};
    status_t status{status_t::OPEN};
    std::ostream& print(std::ostream& os) const {
        os << "(id:" << id << ",px:"<< px << ",amt:" << amt << ",side:" << side << ",status:" << status << ')';
        return os;
    }
};


struct Fill
{
    px_t px;
    amt_t amt; 
    side_t side;
    id_t book_id;
    id_t new_id;
    std::ostream& print(std::ostream& os) const {
        os << "(px:"<< px << ",amt:" << amt << ",side:" << side << ",book_id:" << book_id << ",new_id:" << new_id << ')';
        return os;
    }
};

using orderlist_t = std::list<Order>;    
using order_ptr_t = orderlist_t::iterator; 
//using opt = order_ptr_t;

bool isCrossed(order_ptr_t o1, order_ptr_t o2)
{
    if(o1->side == o2->side)
        return false;
    return (o1->side == side_t::BUY && o1->px>=o2->px ) || (o1->side == side_t::SELL && o1->px<=o2->px );
}
        
class OrderLevel
{
public:
    OrderLevel() = default;
    // adds an order by splicing from other
    void addOrder(order_ptr_t order, orderlist_t& other);
    // updates an order and update internal state
    void updateOrder(order_ptr_t p_order, amt_t amt);
    // move order from this to other
    order_ptr_t moveOrder(order_ptr_t p_order, orderlist_t& other);
    // overload
    order_ptr_t moveOrder(order_ptr_t p_order, OrderLevel& other); 
    void setPx(px_t px) { m_px = px; }
    px_t getPx() const { return m_px; }

    bool empty() const { return m_orders.empty(); }
    amt_t getTotalAmt() const { return m_total_amt; }
    order_ptr_t getBestOrder()
    {
        if(m_orders.empty())
            throw_inv_arg(0, "getBestOrder on empty OrderLevel");
        return m_orders.begin();
    }   
    std::ostream& print(std::ostream& os) const 
    {
        os << "sz:" << m_total_amt << ",{";
        for(const auto& ord: m_orders)
        {
            ord.print(os) << ',';
        }
        os << "}";
        return os;
    }
private:
    void updateTotalAmt(amt_t amt) 
    { 
        m_total_amt += amt;
        if(m_total_amt < 0)
        {
            std::cout << "amt less than 0: " << amt << '\n';
            print(std::cout);
        }
            
    }

    px_t m_px{-1};
    amt_t m_total_amt{};
    orderlist_t m_orders{};
};


class OrderBook
{
private:
    template<template<class> class Comp>
    using levels_t = std::map<px_t, OrderLevel, Comp<px_t>>;
public:
    OrderBook() = default;
    
    static constexpr size_t ADD_VEC_SZ = 5;
    static constexpr size_t UPD_VEC_SZ = 4;
    static constexpr size_t CXL_VEC_SZ = 2;
    void addOrder(const std::vector<int>& args);
    void updateOrder(const std::vector<int>& args);
    void cancelOrder(const std::vector<int>& args);
    OrderLevel* getBestLevel(side_t side) 
    {   
        if(side==side_t::BUY)
        {
            if(!m_bids.empty())
                return &(m_bids.begin()->second);
        }
        if(side==side_t::SELL)
        {
            if(!m_asks.empty())
                return &(m_asks.begin()->second);
        }
        return nullptr;
    }
    OrderLevel& getLevel(side_t side, px_t px) 
    {    
        if(side==side_t::BUY)
        {   
            //if(m_bids.find(px)==m_bids.end())
            //   throw_inv_arg(px, "getLevel invalid price"); 
            return (m_bids.at(px));
        }
        else
        {
            //if(m_asks.find(px)==m_asks.end())
            //   throw_inv_arg(px, "getLevel invalid price"); 
            return (m_asks.at(px));
        }
    }

    std::ostream& print(std::ostream& os) const 
    {
        os << "Book:\n";
        os << "bids:{\n";
        for(const auto& lvl: m_bids)
        {       
            os << lvl.first << ',';
            lvl.second.print(os) << ",\n";
        }
        os << "}\n";
        os << "asks:{\n";
        for(const auto& lvl: m_asks)
        {       
            os << lvl.first << ',';
            lvl.second.print(os) << ",\n";
        }
        os << "}\n";
        os << "dones: {";
        for(const auto& ord: m_done_orders)
        {
            ord.print(os) << ',';
        }
        os << "}\n";
        os << "fills:{";
        for(const auto& fill: m_fills)
            fill.print(os) << ',';
        os << "}\n";
        return os;
   } 
private:
    // will override old order if order id is existing 
    void addOrder(Order&& order);
    // will reset priority of order 
    void updateOrder(id_t id, px_t px, amt_t amt);
    // throws if order is already done
    void cancelOrder(id_t id);
    // will erase OrderLevel if the order to move is the last remaining one
    bool moveToDispatch(order_ptr_t ord, OrderLevel& olvl);
    // will add orders into done or respective OrderLevels. Will construct OrderLevel if not exist.
    void dispatchOrders();
    // fills are checked here
    bool checkCross(order_ptr_t order);
    // throws if id is not found in m_order_lookup
    order_ptr_t getOrder(id_t id);
private:
    levels_t<std::greater> m_bids;
    levels_t<std::less> m_asks;
    orderlist_t m_done_orders;
    orderlist_t m_dispatch_orders;
    std::vector<Fill> m_fills;
    std::unordered_map<id_t, order_ptr_t> m_order_lookup;
};


} // namespace OB
