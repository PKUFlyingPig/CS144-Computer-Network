#ifndef SPONGE_LIBSPONGE_LOSSY_FD_ADAPTER_HH
#define SPONGE_LIBSPONGE_LOSSY_FD_ADAPTER_HH

#include "file_descriptor.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "util.hh"

#include <optional>
#include <random>
#include <utility>

//! An adapter class that adds random dropping behavior to an FD adapter
template <typename AdapterT>
class LossyFdAdapter {
  private:
    //! Fast RNG used by _should_drop()
    std::mt19937 _rand{get_random_generator()};

    //! The underlying FD adapter
    AdapterT _adapter;

    //! \brief Determine whether or not to drop a given read or write
    //! \param[in] uplink is `true` to use the uplink loss probability, else use the downlink loss probability
    //! \returns `true` if the segment should be dropped
    bool _should_drop(bool uplink) {
        const auto &cfg = _adapter.config();
        const uint16_t loss = uplink ? cfg.loss_rate_up : cfg.loss_rate_dn;
        return loss != 0 && uint16_t(_rand()) < loss;
    }

  public:
    //! Conversion to a FileDescriptor by returning the underlying AdapterT
    operator const FileDescriptor &() const { return _adapter; }

    //! Construct from a FileDescriptor appropriate to the AdapterT constructor
    explicit LossyFdAdapter(AdapterT &&adapter) : _adapter(std::move(adapter)) {}

    //! \brief Read from the underlying AdapterT instance, potentially dropping the read datagram
    //! \returns std::optional<TCPSegment> that is empty if the segment was dropped or if
    //!          the underlying AdapterT returned an empty value
    std::optional<TCPSegment> read() {
        auto ret = _adapter.read();
        if (_should_drop(false)) {
            return {};
        }
        return ret;
    }

    //! \brief Write to the underlying AdapterT instance, potentially dropping the datagram to be written
    //! \param[in] seg is the packet to either write or drop
    void write(TCPSegment &seg) {
        if (_should_drop(true)) {
            return;
        }
        return _adapter.write(seg);
    }

    //! \name
    //! Passthrough functions to the underlying AdapterT instance

    //!@{
    void set_listening(const bool l) { _adapter.set_listening(l); }      //!< FdAdapterBase::set_listening passthrough
    const FdAdapterConfig &config() const { return _adapter.config(); }  //!< FdAdapterBase::config passthrough
    FdAdapterConfig &config_mut() { return _adapter.config_mut(); }      //!< FdAdapterBase::config_mut passthrough
    void tick(const size_t ms_since_last_tick) {
        _adapter.tick(ms_since_last_tick);
    }  //!< FdAdapterBase::tick passthrough
    //!@}
};

#endif  // SPONGE_LIBSPONGE_LOSSY_FD_ADAPTER_HH
