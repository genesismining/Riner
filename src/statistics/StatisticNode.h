
#pragma once

#include <src/common/Chrono.h>
#include <src/common/Pointers.h>
#include <src/util/LockUtils.h>
#include <src/common/Assert.h>
#include <list>

namespace miner {

    //TODO: write some nice documentation about StatisticNode because theres a lot of stuff going on
    //TODO: should this node use Ctor/Dtor to call AddListener/RemoveListener instead of relying on weak_ptrs that clean up on iteration?
    template<class T>
    class StatisticNode {

        struct Node {
            LockGuarded<T> _content;
            LockGuarded<std::list<weak_ptr<Node>>> _listeners;

            void addListener(shared_ptr<Node> &listener) {
                _listeners.lock()->emplace_back(make_weak(listener));
            }

            void removeListener(const shared_ptr<Node> &listener) {
                auto listeners = _listeners.lock();
                for (auto it = listeners->begin(); it != listeners->end(); ) {
                    auto current = it;
                    ++it;
                    if (shared_ptr<Node> shared = current->lock()) {
                        if (shared == listener) {
                            listeners->erase(current);
                            break;
                        }
                    }
                    else listeners->erase(current);
                }
            }

            std::function<void(T &)> &&lockedForEach(std::function<void(T &)> &&func) {

                { // content lock scope
                    auto content = _content.lock();
                    func(*content);
                } // unlock content

                std::vector<shared_ptr<Node>> toBeCalled; // temporarily adds 1 refcount to every active listener

                { // listeners lock scope
                    auto listeners = _listeners.lock();
                    toBeCalled.reserve(listeners->size());

                    for (auto it = listeners->begin(); it != listeners->end(); ) {
                        if (shared_ptr<Node> lis = it->lock()) {

                            toBeCalled.emplace_back(lis);
                            ++it;
                        }
                        else {
                            listeners->erase(it++);
                        }
                    }
                } // unlock listeners

                //notify listeners without holding the listeners lock
                for (shared_ptr<Node> &node : toBeCalled) {
                    //move the function in and out of the recursive call
                    func = std::move(node->lockedForEach(std::move(func)));
                }
                return std::move(func);
            }
        };

        shared_ptr<Node> _node = make_shared<Node>(); //always valid, never nullptr

    public:

        StatisticNode(const StatisticNode &) = delete;
        StatisticNode &operator=(const StatisticNode &) = delete;

        StatisticNode() = default;
        StatisticNode(StatisticNode &&) noexcept = default;
        StatisticNode &operator=(StatisticNode &&) noexcept = default;

        inline void addListener(StatisticNode<T> &listener) {
            MI_EXPECTS(_node);
            _node->addListener(listener._node);
        }

        void removeListener(const StatisticNode<T> &listener) {
            MI_EXPECTS(_node);
            _node->removeListener(listener._node);
        }

        //use this function to modify the T instance of this node and all child nodes by the same func.
        //!!! DEADLOCK possible! this function holds a lock while calling a callback! be careful!
        void lockedForEach(std::function<void(T &)> &&func) {
            MI_EXPECTS(_node);
            _node->lockedForEach(std::move(func));
        }

        //use this function in case you want to modify the T instance of this particular node but don't want a copy operation to happen, otherwise call getValue()
        //!!! DEADLOCK possible! this function holds a lock while calling a callback! be careful!
        void lockedApply(std::function<void(T &)> &&func) {
            MI_EXPECTS(_node);
            auto content = _node->_content.lock();
            func(*content);
        }

        //use this function in case you want to read the T instance of this particular node but don't want a copy operation to happen, otherwise call getValue()
        //!!! DEADLOCK possible! this function holds a lock while calling a callback! be careful!
        void lockedApply(std::function<void(const T &)> &&func) const {
            MI_EXPECTS(_node);
            auto content = _node->_content.lock();
            func(*content);
        }

        //get a copy of the T instance of this node
        T getValue() const {
            auto locked = _node->_content.lock();
            T copy = *locked;
            return copy;
        }

    };

}