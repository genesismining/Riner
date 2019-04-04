
#pragma once

#include <src/common/Chrono.h>
#include <src/common/Pointers.h>
#include <src/util/LockUtils.h>
#include <src/common/Assert.h>
#include <list>

namespace miner {

    //TODO: write some nice documentation about StatisticNode because theres a lot of stuff going on
    template<class T>
    class StatisticNode : public JsonSerializable {

        static_assert(std::is_base_of<JsonSerializable, T>::value, "T must implement JsonSerializable interface");

        struct Node {
            shared_ptr<LockGuarded<T>> _content;
            LockGuarded<std::list<weak_ptr<Node>>> _listeners;
            std::string _name = "unnamed";

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
                    auto content = _content->lock();
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

        nl::json toJson() const override {
            MI_EXPECTS(_node);

            LOG(WARNING) << "StatisticNode toJson() not implemented yet";
            return _node->_content->lock()->toJson();
        }



        inline void addListener(StatisticNode<T> &listener) {
            MI_EXPECTS(_node);
            _node->addListener(listener._node);
        }

        void removeListener(const StatisticNode<T> &listener) {
            MI_EXPECTS(_node);
            _node->removeListener(listener._node);
        }

        //use this function to modify the T instance of this node and all child nodes by the same func.
        //!!! DEADLOCK possible! this function holds a lock while the func callback is processed! be careful!
        void lockedForEach(std::function<void(T &)> &&func) {
            MI_EXPECTS(_node);
            _node->lockedForEach(std::move(func));
        }

        //use this function in case you want to read the T instance of this particular node but don't want a copy operation to happen, otherwise call getValue()
        //!!! DEADLOCK possible! this function holds a lock while the func callback is processed! be careful!
        template<class Func>
        void lockedRead(Func &&func) {
            MI_EXPECTS(_node);
            auto content = _node->_content->lock();
            func(*content);
        }

        //get a copy of the T instance of this node
        T getValue() const {
            T copy = *_node->_content->lock();
            return copy;
        }

    };

}