
#pragma once

#include <src/common/Chrono.h>
#include <src/common/Pointers.h>
#include <src/util/LockUtils.h>
#include <src/common/Assert.h>
#include <list>

namespace riner {

    /**
     * helper class for propagating changes to T (which contains statistic data) along a listener chain.
     * This means that, e.g. there is a StatisticNode for every Gpu, but also a StatisticNode that gathers information from all of the Gpu's individual nodes and aggregates their data.
     * This is mostly done by using the public lockedForEach and lockedApply
     */
    template<class T>
    class StatisticNode {

        /**
         * this Node class is just a wrapper so the entire thing can be put into a std::shared_ptr, which allows std::weak_ptr functionality (figuring out whether the instance was deleted)
         */
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
            RNR_EXPECTS(_node);
            _node->addListener(listener._node);
        }

        void removeListener(const StatisticNode<T> &listener) {
            RNR_EXPECTS(_node);
            _node->removeListener(listener._node);
        }

        /**
         * use this function to modify the T instance of this node and all child nodes by the same func.
         * !!! DEADLOCK possible! this function holds a lock while calling a callback! be careful!
         */
        void lockedForEach(std::function<void(T &)> &&func) {
            RNR_EXPECTS(_node);
            _node->lockedForEach(std::move(func));
        }
        
        /**
         * use this function in case you want to modify the T instance of this particular node but don't want a copy operation to happen, otherwise call getValue()
         * !!! DEADLOCK possible! this function holds a lock while calling a callback! be careful!
         */
        void lockedApply(std::function<void(T &)> &&func) {
            RNR_EXPECTS(_node);
            auto content = _node->_content.lock();
            func(*content);
        }

        /**
         * use this function in case you want to read the T instance of this particular node but don't want a copy operation to happen, otherwise call getValue()
         * !!! DEADLOCK possible! this function holds a lock while calling a callback! be careful!
         */
        void lockedApply(std::function<void(const T &)> &&func) const {
            RNR_EXPECTS(_node);
            auto content = _node->_content.lock();
            func(*content);
        }

        /**
         * get a copy of the T instance of this node
         */
        T getValue() const {
            auto locked = _node->_content.lock();
            T copy = *locked;
            return copy;
        }

    };

}
