#include <cassert>
#include "OrderBook.h"
#include <iostream>

using namespace OB;
using namespace std;


void OrderLevel::addOrder(order_ptr_t order, orderlist_t& other)
{
    updateTotalAmt(order->amt);
    m_orders.splice(m_orders.end(), other, order);
}

void OrderLevel::updateOrder(order_ptr_t p_order, amt_t amt)
{
    cout << "updateOrder DBG: orig: " << getTotalAmt();
    updateTotalAmt(amt - p_order->amt);
    cout << ", after: " << getTotalAmt() << '\n';
    p_order->amt = amt;
}

order_ptr_t OrderLevel::moveOrder(order_ptr_t p_order, orderlist_t& other)
{
    assert(p_order->amt >= 0);
    cout << "updateOrder DBG: orig: " << getTotalAmt();
    updateTotalAmt(-p_order->amt);
    cout << ", after: " << getTotalAmt() << '\n';
    other.splice(other.end(), m_orders, p_order);
    return std::prev(other.end()); 
}

order_ptr_t OrderLevel::moveOrder(order_ptr_t p_order, OrderLevel& other)
{
    return moveOrder(p_order, other.m_orders);
}
    
void OrderBook::addOrder(Order&& order)
{
    order.status = status_t::OPEN;
    m_dispatch_orders.emplace_back(std::move(order));
    checkCross(std::prev(m_dispatch_orders.end()));
}

order_ptr_t OrderBook::getOrder(id_t id)
{
    auto map_it = m_order_lookup.find(id);
    if(map_it==m_order_lookup.end())
        throw_inv_arg(id, "id not found in lookup");
    return map_it->second;
} 


void OrderBook::updateOrder(id_t id, px_t px, amt_t amt)
{
    auto order_it = getOrder(id); 
    if(order_it->status!=status_t::OPEN)
    {
        throw_inv_arg(id, "Trying to update closed order");
        return;
    }   
    auto& old_lvl = getLevel(order_it->side, order_it->px);
    old_lvl.updateOrder(order_it, amt);
    if(amt == 0) 
    {
        order_it->status = status_t::CANCELLED;
        moveToDispatch(order_it, old_lvl);
        return;
    }

    if(order_it->px != px)
    {
        moveToDispatch(order_it, old_lvl);
        order_it->px = px;
        checkCross(order_it);
    }
 } 
     
void OrderBook::cancelOrder(id_t id)
{
    auto order_it = getOrder(id); 
    if(order_it->status!=status_t::OPEN)
    {
        throw_inv_arg(id, "Trying to cancel closed order");
        return;
    }   
    order_it->status = status_t::CANCELLED;
    auto& lvl = getLevel(order_it->side, order_it->px);
    moveToDispatch(order_it, lvl);
}

bool OrderBook::checkCross(order_ptr_t order)
{
    while(auto* best_lvl = getBestLevel(~order->side))
    {
        if(!isCrossed(order, best_lvl->getBestOrder()))
            break;
     
        order_ptr_t best_order = best_lvl->getBestOrder();
        px_t trd_px = best_order->px;
        amt_t trd_amt = min(order->amt, best_order->amt);
        m_fills.push_back({trd_px, trd_amt, order->side, best_order->id, order->id}); 
        cout << "FILL: ";
        m_fills.back().print(std::cout);
        cout << '\n';

        order->amt -= trd_amt;
        best_order->amt -= trd_amt;

        if(best_order->amt == 0)
        {
            best_order->status = status_t::FILLED;
            if(moveToDispatch(best_order, *best_lvl))
                break;
        }
        if(order->amt == 0)
        {
            order->status = status_t::FILLED;
            return true;
        }
     
    }
    return false;
}

// return true if the olvl is erased
bool OrderBook::moveToDispatch(order_ptr_t ord, OrderLevel& olvl)
{
    ord = olvl.moveOrder(ord,m_dispatch_orders);
    cout << "moveToDispatch DBG: Ord:";
    ord->print(cout);
    cout << ", Lvl:";
    olvl.print(cout);
    cout << ",px:" << olvl.getPx() << '\n';
    if(olvl.empty())
    {
        assert(olvl.getTotalAmt()==0);
        side_t side = ord->side;
        if(side==side_t::BUY)
            m_bids.erase(olvl.getPx());
        else
            m_asks.erase(olvl.getPx());
        return true;
    }
    return false;
}

void OrderBook::dispatchOrders()
{
    order_ptr_t old_order;
    while(!m_dispatch_orders.empty())
    {
        //guarantee progress
        assert(m_dispatch_orders.begin()!=old_order);
        order_ptr_t order = m_dispatch_orders.begin();
        old_order = order;
        if(order->status != status_t::OPEN)
        {
            m_done_orders.splice(m_done_orders.end(), m_dispatch_orders, order);
        }
        else
        {
            assert(order->amt != 0);
            if(order->side == side_t::BUY)
            {       
                auto& level = m_bids[order->px];
                level.addOrder(order, m_dispatch_orders);
                if(level.getPx()==-1)
                    level.setPx(order->px);
            }
            else
            {
                auto& level = m_asks[order->px];
                level.addOrder(order, m_dispatch_orders);
                if(level.getPx()==-1)
                    level.setPx(order->px);
            }
            m_order_lookup[order->id] = order;
        }
    }
}

void OrderBook::addOrder(const vector<int>& args)
{
    assert(args.size() == ADD_VEC_SZ);
    side_t side = int_to_side(args.back());
    addOrder(Order{static_cast<id_t>(args[1]),args[2],args[3], side});
    dispatchOrders();
}    

void OrderBook::updateOrder(const vector<int>& args)
{
    assert(args.size() >= OrderBook::UPD_VEC_SZ);
    updateOrder(args[1],args[2],args[3]);
    dispatchOrders();
}

void OrderBook::cancelOrder(const vector<int>& args)
{
    assert(args.size() >= CXL_VEC_SZ);
    cancelOrder(args[1]);
    dispatchOrders();
}

int main()
{
    //cout << "Hello World Order Book!" << endl;
    // action, id, px, sz, side
    // action = add, up, cxl
    // 0,1,2
    vector<vector<int>> tc = {{
        {{0,1,100,10,1}},
        {{0,2,99,10,0}},
        {{0,3,101,20,1}}, 
        {{0,4,98,20,0}},
        {{0,5,102,5,0}},
        {{2,2}},
        {{1,3,102,30}},
        }};
    OrderBook ob;
    auto ingest = [&ob](vector<vector<int>>& stream)
    {
        for(const auto& vec: stream)
        {
            assert(!vec.empty());
            cout << "ingesting {";
            for(int i: vec)
                cout << i << ',';
            cout << "}\n";
            try
            {
                switch(vec[0])
                {

                    case CXL:
                    {
                        ob.cancelOrder(vec);
                        break;
                    }
                    case Action::ADD:
                    {
                        ob.addOrder(vec);
                        break;
                    }
                    case Action::UPD:
                    {
                        ob.updateOrder(vec);
                        break;
                    }
                    default:
                        throw_inv_arg(vec[0], "Invalid action encountered");
                }
            }
            catch(const std::exception& e)
            {
                cout << "Exception caught for case: {";
                for(auto arg: vec)
                    cout << arg << ',';
                cout << "}\n";
                cout << e.what() << '\n';
            }
            //ob.print(std::cout);
        }
    };    

    ingest(tc);
    ob.print(cout);
    cout << endl;
    return 0;
}   
