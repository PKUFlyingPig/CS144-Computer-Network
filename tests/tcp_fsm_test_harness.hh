#ifndef SPONGE_LIBSPONGE_TCP_FSM_TEST_HARNESS_HH
#define SPONGE_LIBSPONGE_TCP_FSM_TEST_HARNESS_HH

#include "fd_adapter.hh"
#include "file_descriptor.hh"
#include "tcp_config.hh"
#include "tcp_connection.hh"
#include "tcp_expectation_forward.hh"
#include "tcp_segment.hh"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <vector>

//! \brief A wrapper class for a SOCK_SEQPACKET [Unix-domain socket](\ref man7::unix), for use by TCPTestHarness
//! \details A TestFD comprises the two endpoints of a Unix-domain socket connection.
//! The contents of a write() to this object can subsequently be read() from it.
class TestFD : public FileDescriptor {
  private:
    //! A wrapper for one end of a SOCK_SEQPACKET socket pair
    class TestRFD : public FileDescriptor {
      public:
        //! Construction uses the base class constructor
        using FileDescriptor::FileDescriptor;

        bool can_read() const;  //!< Is this descriptor readable?
        std::string read();     //!< Read a from this socket
    };

    TestRFD _recv_fd;  //!< The end of a SOCK_SEQPACKET socket pair from which TCPTestHarness reads

    //! Max-sized segment plus some margin
    static constexpr size_t MAX_RECV = TCPConfig::MAX_PAYLOAD_SIZE + TCPHeader::LENGTH + 16;

    //! Construct from a pair of sockets
    explicit TestFD(std::pair<FileDescriptor, TestRFD> fd_pair);

  public:
    TestFD();  //!< Default constructor invokes TestFD(std::pair<FileDescriptor, TestRFD> fd_pair)

    void write(const BufferViewList &buffer);  //!< Write a buffer

    bool can_read() const { return _recv_fd.can_read(); }  //!< Is TestFD::_recv_fd readable?

    std::string read() { return _recv_fd.read(); }  //!< Read from TestFD::_recv_fd
};

//! An FdAdapterBase that writes to a TestFD. Does not (need to) support reading.
class TestFdAdapter : public FdAdapterBase, public TestFD {
  public:
    void write(TCPSegment &seg);  //!< Write a TCPSegment to the underlying TestFD

    void config_segment(TCPSegment &seg);  //!< Copy information from FdAdapterConfig into a TCPSegment
};

//! Test adapter for TCPConnection
class TCPTestHarness {
  public:
    TestFdAdapter _flt{};  //!< FdAdapter mockup
    TCPConnection _fsm;    //!< The TCPConnection under test

    //! A list of test steps that passed
    std::vector<std::string> _steps_executed{};

    using State = TCPState::State;                 //!< TCP state names
    using VecIterT = std::string::const_iterator;  //!< Alias for a const iterator to a vector of bytes

    //! Construct a test harness, optionally passing a configuration to the TCPConnection under test
    explicit TCPTestHarness(const TCPConfig &c_fsm = {}) : _fsm(c_fsm) {}

    //! construct a FIN segment and inject it into TCPConnection
    void send_fin(const WrappingInt32 seqno, const std::optional<WrappingInt32> ackno = {});

    //! construct an ACK segment and inject it into TCPConnection
    void send_ack(const WrappingInt32 seqno, const WrappingInt32 ackno, const std::optional<uint16_t> swin = {});

    //! construct a RST segment and inject it into TCPConnection
    void send_rst(const WrappingInt32 seqno, const std::optional<WrappingInt32> ackno = {});

    //! construct a SYN segment and inject it into TCPConnection
    void send_syn(const WrappingInt32 seqno, const std::optional<WrappingInt32> ackno = {});

    //! construct a segment containing one byte and inject it into TCPConnection
    void send_byte(const WrappingInt32 seqno, const std::optional<WrappingInt32> ackno, const uint8_t val);

    //! construct a segment containing the specified payload and inject it into TCPConnection
    void send_data(const WrappingInt32 seqno, const WrappingInt32 ackno, VecIterT begin, VecIterT end);

    //! is it possible to read from the TestFdAdapter (i.e., read a segment TCPConnection previously wrote)?
    bool can_read() const { return _flt.can_read(); }

    //! \brief execute one step in a test.
    //! \param step is a representation of the step. i.e. an action or
    //!        expectation
    //! \param note is an (optional) string describing the significance of this
    //!        step
    void execute(const TCPTestStep &step, std::string note = "");

    //! \brief expect and read a segment
    //! \param expectaion is a representation of the properties of the expected
    //!        segment.
    //! \param note is an (optional) string describing the significance of this
    //!        step
    TCPSegment expect_seg(const ExpectSegment &expectation, std::string note = "");

    //! Create an FSM in the "LISTEN" state.
    static TCPTestHarness in_listen(const TCPConfig &cfg);

    //! \brief Create an FSM which has sent a SYN.
    //! \details The SYN has been consumed, but not ACK'd by the test harness
    //! \param[in] tx_isn is the ISN of the FSM's outbound sequence. i.e. the
    //!            seqno for the SYN.
    static TCPTestHarness in_syn_sent(const TCPConfig &cfg, const WrappingInt32 tx_isn = WrappingInt32{0});

    //! \brief Create an FSM with an established connection
    //! \details The mahine has sent and received a SYN, and both SYNs have been ACK'd
    //!          No payload was exchanged.
    //! \param[in] tx_isn is the ISN of the FSM's outbound sequence. i.e. the
    //!            seqno for the SYN.
    //! \param[in] rx_isn is the ISN of the FSM's inbound sequence. i.e. the
    //!            seqno for the SYN.
    static TCPTestHarness in_established(const TCPConfig &cfg,
                                         const WrappingInt32 tx_isn = WrappingInt32{0},
                                         const WrappingInt32 rx_isn = WrappingInt32{0});

    //! \brief Create an FSM in CLOSE_WAIT
    //! \details SYNs have been traded, and then the machine received and ACK'd FIN.
    //!          No payload was exchanged.
    //! \param[in] tx_isn is the ISN of the FSM's outbound sequence. i.e. the
    //!            seqno for the SYN.
    //! \param[in] rx_isn is the ISN of the FSM's inbound sequence. i.e. the
    //!            seqno for the SYN.
    static TCPTestHarness in_close_wait(const TCPConfig &cfg,
                                        const WrappingInt32 tx_isn = WrappingInt32{0},
                                        const WrappingInt32 rx_isn = WrappingInt32{0});

    //! \brief Create an FSM in LAST_ACK
    //! \details SYNs have been traded, then the machine received and ACK'd FIN, and then it sent its own FIN.
    //!          No payload was exchanged.
    //! \param[in] tx_isn is the ISN of the FSM's outbound sequence. i.e. the
    //!            seqno for the SYN.
    //! \param[in] rx_isn is the ISN of the FSM's inbound sequence. i.e. the
    //!            seqno for the SYN.
    static TCPTestHarness in_last_ack(const TCPConfig &cfg,
                                      const WrappingInt32 tx_isn = WrappingInt32{0},
                                      const WrappingInt32 rx_isn = WrappingInt32{0});

    //! \brief Create an FSM in FIN_WAIT_1
    //! \details SYNs have been traded, then the TCP sent FIN.
    //!          No payload was exchanged.
    //! \param[in] tx_isn is the ISN of the FSM's outbound sequence. i.e. the
    //!            seqno for the SYN.
    //! \param[in] rx_isn is the ISN of the FSM's inbound sequence. i.e. the
    //!            seqno for the SYN.
    static TCPTestHarness in_fin_wait_1(const TCPConfig &cfg,
                                        const WrappingInt32 tx_isn = WrappingInt32{0},
                                        const WrappingInt32 rx_isn = WrappingInt32{0});

    //! \brief Create an FSM in FIN_WAIT_2
    //! \details SYNs have been traded, then the TCP sent FIN.
    //!          No payload was exchanged.
    //! \param[in] tx_isn is the ISN of the FSM's outbound sequence. i.e. the
    //!            seqno for the SYN.
    //! \param[in] rx_isn is the ISN of the FSM's inbound sequence. i.e. the
    //!            seqno for the SYN.
    static TCPTestHarness in_fin_wait_2(const TCPConfig &cfg,
                                        const WrappingInt32 tx_isn = WrappingInt32{0},
                                        const WrappingInt32 rx_isn = WrappingInt32{0});

    //! \brief Create an FSM in CLOSING
    //! \details SYNs have been traded, then the TCP sent FIN, then received FIN
    //!          No payload was exchanged.
    //! \param[in] tx_isn is the ISN of the FSM's outbound sequence. i.e. the
    //!            seqno for the SYN.
    //! \param[in] rx_isn is the ISN of the FSM's inbound sequence. i.e. the
    //!            seqno for the SYN.
    static TCPTestHarness in_closing(const TCPConfig &cfg,
                                     const WrappingInt32 tx_isn = WrappingInt32{0},
                                     const WrappingInt32 rx_isn = WrappingInt32{0});

    //! \brief Create an FSM in TIME_WAIT
    //! \details SYNs have been traded, then the TCP sent FIN, then received FIN/ACK, and ACK'd.
    //!          No payload was exchanged.
    //! \param[in] tx_isn is the ISN of the FSM's outbound sequence. i.e. the
    //!            seqno for the SYN.
    //! \param[in] rx_isn is the ISN of the FSM's inbound sequence. i.e. the
    //!            seqno for the SYN.
    static TCPTestHarness in_time_wait(const TCPConfig &cfg,
                                       const WrappingInt32 tx_isn = WrappingInt32{0},
                                       const WrappingInt32 rx_isn = WrappingInt32{0});
};

#endif  // SPONGE_LIBSPONGE_TCP_FSM_TEST_HARNESS_HH
