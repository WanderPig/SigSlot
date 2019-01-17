// sigslot.h: Signal/Slot classes
//
// Written by Sarah Thompson (sarah@telergy.com) 2002.
//
// License: Public domain. You are free to use this code however you like, with the proviso that
//          the author takes on no responsibility or liability for any use.
//
// QUICK DOCUMENTATION
//
//              (see also the full documentation at http://sigslot.sourceforge.net/)
//
//      #define switches
//          SIGSLOT_PURE_ISO            - Define this to force ISO C++ compliance. This also disables
//                                        all of the thread safety support on platforms where it is
//                                        available.
//
//          SIGSLOT_USE_POSIX_THREADS   - Force use of Posix threads when using a C++ compiler other than
//                                        gcc on a platform that supports Posix threads. (When using gcc,
//                                        this is the default - use SIGSLOT_PURE_ISO to disable this if
//                                        necessary)
//
//          SIGSLOT_DEFAULT_MT_POLICY   - Where thread support is enabled, this defaults to multi_threaded_global.
//                                        Otherwise, the default is single_threaded. #define this yourself to
//                                        override the default. In pure ISO mode, anything other than
//                                        single_threaded will cause a compiler error.
//
//      PLATFORM NOTES
//
//          Win32                       - On Win32, the WIN32 symbol must be #defined. Most mainstream
//                                        compilers do this by default, but you may need to define it
//                                        yourself if your build environment is less standard. This causes
//                                        the Win32 thread support to be compiled in and used automatically.
//
//          Unix/Linux/BSD, etc.        - If you're using gcc, it is assumed that you have Posix threads
//                                        available, so they are used automatically. You can override this
//                                        (as under Windows) with the SIGSLOT_PURE_ISO switch. If you're using
//                                        something other than gcc but still want to use Posix threads, you
//                                        need to #define SIGSLOT_USE_POSIX_THREADS.
//
//          ISO C++                     - If none of the supported platforms are detected, or if
//                                        SIGSLOT_PURE_ISO is defined, all multithreading support is turned off,
//                                        along with any code that might cause a pure ISO C++ environment to
//                                        complain. Before you ask, gcc -ansi -pedantic won't compile this
//                                        library, but gcc -ansi is fine. Pedantic mode seems to throw a lot of
//                                        errors that aren't really there. If you feel like investigating this,
//                                        please contact the author.
//
//
//      THREADING MODES
//
//          single_threaded             - Your program is assumed to be single threaded from the point of view
//                                        of signal/slot usage (i.e. all objects using signals and slots are
//                                        created and destroyed from a single thread). Behaviour if objects are
//                                        destroyed concurrently is undefined (i.e. you'll get the occasional
//                                        segmentation fault/memory exception).
//
//          multi_threaded_global       - Your program is assumed to be multi threaded. Objects using signals and
//                                        slots can be safely created and destroyed from any thread, even when
//                                        connections exist. In multi_threaded_global mode, this is achieved by a
//                                        single global mutex (actually a critical section on Windows because they
//                                        are faster). This option uses less OS resources, but results in more
//                                        opportunities for contention, possibly resulting in more context switches
//                                        than are strictly necessary.
//
//          multi_threaded_local        - Behaviour in this mode is essentially the same as multi_threaded_global,
//                                        except that each signal, and each object that inherits has_slots, all
//                                        have their own mutex/critical section. In practice, this means that
//                                        mutex collisions (and hence context switches) only happen if they are
//                                        absolutely essential. However, on some platforms, creating a lot of
//                                        mutexes can slow down the whole OS, so use this option with care.
//
//      USING THE LIBRARY
//
//          See the full documentation at http://sigslot.sourceforge.net/
//
//

#ifndef SIGSLOT_H__
#define SIGSLOT_H__

#include <set>
#include <list>
#include <functional>
#include <mutex>

namespace sigslot {

    class has_slots;

    namespace internal {

        template<class... args>
        class _connection_base
        {
        public:
            bool one_shot = false;
            bool expired = false;
            virtual ~_connection_base() { }
            virtual has_slots* getdest() const = 0;
            virtual void emit(args...) = 0;
            virtual _connection_base* clone() = 0;
            virtual _connection_base* duplicate(has_slots* pnewdest) = 0;
        };

        class _signal_base_lo
        {
        protected:
            std::mutex m_barrier;
        public:
            virtual void slot_disconnect(has_slots* pslot) = 0;
            virtual void slot_duplicate(const has_slots* poldslot, has_slots* pnewslot) = 0;
        };

        template<class... args>
        class _signal_base : public _signal_base_lo
        {
        public:
            typedef typename std::list<_connection_base<args...> *>  connections_list;

            _signal_base() : _signal_base_lo(), m_connected_slots()
            {
                ;
            }

            _signal_base(const _signal_base& s)
                    : _signal_base_lo(s), m_connected_slots()
            {
                std::lock_guard<std::mutex> m_barrier;
                for (auto i : s.m_connected_slots) {
                    i->getdest()->signal_connect(this);
                    m_connected_slots.push_back(i->clone());
                }
            }

            _signal_base(_signal_base &&) = delete;

            ~_signal_base()
            {
                disconnect_all();
            }

            void disconnect_all()
            {
                std::lock_guard<std::mutex> m_barrier;
                for (auto i : m_connected_slots) {
                    i->getdest()->signal_disconnect(this);
                    delete i;
                }
                m_connected_slots.erase(m_connected_slots.begin(), m_connected_slots.end());
            }

            void disconnect(has_slots* pclass)
            {
                std::lock_guard<std::mutex> m_barrier;
                bool found{false};
                m_connected_slots.remove_if([pclass, &found](_connection_base<args...> * x) {
                    if (x->getdest() == pclass) {
                        delete x;
                        found = true;
                        return true;
                    }
                    return false;
                });
                if (found) pclass->signal_disconnect(this);
            }

            void slot_disconnect(has_slots* pslot)
            {
                std::lock_guard<std::mutex> m_barrier;
                m_connected_slots.remove_if(
                    [pslot](_connection_base<, args...> * x) {
                        if (x->getdest() == pslot) {
                            delete x;
                            return true;
                        }
                        return false;
                    }
                );
            }

            void slot_duplicate(const has_slots* oldtarget, has_slots* newtarget)
            {
                std::lock_guard<std::mutex> m_barrier;
                for (auto i : m_connected_slots) {
                    if(i->getdest() == oldtarget)
                    {
                        m_connected_slots.push_back(i->duplicate(newtarget));
                    }
                }
            }

        protected:
            connections_list m_connected_slots;
        };

        template<class dest_type, class... args>
        class _connection : public _connection_base<args...>
        {
        public:
            _connection(dest_type *pobject, std::function<void(args... a)> fn, bool once)
                    : m_pobject(pobject), m_fn(fn) { _connection_base<args...>::one_shot = once; }
            
            virtual _connection_base<args...>* clone()
            {
                return new _connection<dest_type, args...>(m_pobject, m_fn,
                                                                      _connection_base<args...>::one_shot);
            }

            virtual _connection_base<args...>* duplicate(has_slots* pnewdest)
            {
                return new _connection<dest_type, args...>((dest_type *) pnewdest, m_fn,
                                                                      _connection_base<args...>::one_shot);
            }

            virtual void emit(args... a)
            {
                m_fn(a...);
            }

            virtual has_slots* getdest() const
            {
                return m_pobject;
            }
        private:
            dest_type* m_pobject;
            std::function<void(args...)> m_fn;
        };
    }

    class has_slots
    {
    private:
        typedef typename std::set<internal::_signal_base_lo *> sender_set;

    public:
        has_slots() : m_senders()
        {
            ;
        }

        has_slots(const has_slots& hs)
                : m_senders()
        {
            std::lock_guard<std::mutex> m_barrier;
            for (auto i : hs.m_senders) {
                i->slot_duplicate(&hs, this);
                m_senders.insert(i);
            }
        }

        has_slots(has_slots &&) = delete;

        void signal_connect(internal::_signal_base_lo* sender)
        {
            std::lock_guard<std::mutex> m_barrier;
            m_senders.insert(sender);
        }

        void signal_disconnect(internal::_signal_base_lo* sender)
        {
            std::lock_guard<std::mutex> m_barrier;
            m_senders.erase(sender);
        }

        virtual ~has_slots()
        {
            disconnect_all();
        }

        void disconnect_all()
        {
            std::lock_guard<std::mutex> m_barrier;
            for (auto i : m_senders) {
                i->slot_disconnect(this);
            }

            m_senders.erase(m_senders.begin(), m_senders.end());
        }

    private:
        sender_set m_senders;
    };



    template<class... args>
    class signal : public internal::_signal_base<args...>
    {
    public:
        typedef typename internal::_signal_base<args...>::connections_list::const_iterator const_iterator;
        signal()
        {
            ;
        }

        signal(const signal<args...>& s)
        : internal::_signal_base<args...>(s)
        {
            ;
        }

        template<class desttype>
        void connect(desttype *pclass, std::function<void(args...)> &&fn, bool one_shot = false)
        {
            std::lock_guard<std::mutex> m_barrier;
            internal::_connection<desttype, args...> *conn = new internal::_connection<desttype, args...>(
                    pclass, fn, one_shot);
            this->m_connected_slots.push_back(conn);
            pclass->signal_connect(this);
        }
        
        // Helper for ptr-to-member; call the member function "normally".
        template<class desttype>
        void connect(desttype *pclass, void (desttype::* memfn)(args...), bool one_shot = false)
        {
            this->connect(pclass, [pclass, memfn](args... a) { (pclass->*memfn)(a...); }, one_shot);
        }

        // This code uses the long-hand because it assumes it may mutate the list.
        void emit(args... a)
        {
            std::lock_guard<std::mutex> m_barrier;
            const_iterator itNext, it = this->m_connected_slots.begin();
            const_iterator itEnd = this->m_connected_slots.end();

            while(it != itEnd)
            {
                itNext = it;
                ++itNext;

                (*it)->emit(a...);
                if ((*it)->one_shot) {
                    (*it)->expired = true;
                }

                it = itNext;
            }

            this->m_connected_slots.remove_if([this](internal::_connection_base<args...> *x) {
                if (x->expired) {
                    x->getdest()->signal_disconnect(this);
                    delete x;
                    return true;
                }
                return false;
            });
            // Might need to reconnect new signals. This needs improvement...
            for (auto const conn : this->m_connected_slots) {
                conn->getdest()->signal_connect(this);
            }
        }

        void operator()(args... a)
        {
            this->emit(a...);
        }
    };

} // namespace sigslot

#endif // SIGSLOT_H__

