#ifndef SPONGE_LIBSPONGE_EVENTLOOP_HH
#define SPONGE_LIBSPONGE_EVENTLOOP_HH

#include "file_descriptor.hh"

#include <cstdlib>
#include <functional>
#include <list>
#include <poll.h>

//! Waits for events on file descriptors and executes corresponding callbacks.
class EventLoop {
  public:
    //! Indicates interest in reading (In) or writing (Out) a polled fd.
    enum class Direction : short {
        In = POLLIN,   //!< Callback will be triggered when Rule::fd is readable.
        Out = POLLOUT  //!< Callback will be triggered when Rule::fd is writable.
    };

  private:
    using CallbackT = std::function<void(void)>;  //!< Callback for ready Rule::fd
    using InterestT = std::function<bool(void)>;  //!< `true` return indicates Rule::fd should be polled.

    //! \brief Specifies a condition and callback that an EventLoop should handle.
    //! \details Created by calling EventLoop::add_rule() or EventLoop::add_cancelable_rule().
    class Rule {
      public:
        FileDescriptor fd;    //!< FileDescriptor to monitor for activity.
        Direction direction;  //!< Direction::In for reading from fd, Direction::Out for writing to fd.
        CallbackT callback;   //!< A callback that reads or writes fd.
        InterestT interest;   //!< A callback that returns `true` whenever fd should be polled.
        CallbackT cancel;     //!< A callback that is called when the rule is cancelled (e.g. on hangup)

        //! Returns the number of times fd has been read or written, depending on the value of Rule::direction.
        //! \details This function is used internally by EventLoop; you will not need to call it
        unsigned int service_count() const;
    };

    std::list<Rule> _rules{};  //!< All rules that have been added and not canceled.

  public:
    //! Returned by each call to EventLoop::wait_next_event.
    enum class Result {
        Success,  //!< At least one Rule was triggered.
        Timeout,  //!< No rules were triggered before timeout.
        Exit  //!< All rules have been canceled or were uninterested; make no further calls to EventLoop::wait_next_event.
    };

    //! Add a rule whose callback will be called when `fd` is ready in the specified Direction.
    void add_rule(const FileDescriptor &fd,
                  const Direction direction,
                  const CallbackT &callback,
                  const InterestT &interest = [] { return true; },
                  const CallbackT &cancel = [] {});

    //! Calls [poll(2)](\ref man2::poll) and then executes callback for each ready fd.
    Result wait_next_event(const int timeout_ms);
};

using Direction = EventLoop::Direction;

//! \class EventLoop
//!
//! An EventLoop holds a std::list of Rule objects. Each time EventLoop::wait_next_event is
//! executed, the EventLoop uses the Rule objects to construct a call to [poll(2)](\ref man2::poll).
//!
//! When a Rule is installed using EventLoop::add_rule, it will be polled for the specified Rule::direction
//! whenver the Rule::interest callback returns `true`, until Rule::fd is no longer readable
//! (for Rule::direction == Direction::In) or writable (for Rule::direction == Direction::Out).
//! Once this occurs, the Rule is canceled, i.e., the EventLoop deletes it.
//!
//! A Rule installed using EventLoop::add_cancelable_rule will be polled and canceled under the
//! same conditions, with the additional condition that if Rule::callback returns `true`, the
//! Rule will be canceled.

#endif  // SPONGE_LIBSPONGE_EVENTLOOP_HH
