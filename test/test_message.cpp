#include "message.h"

#include <boost/test/unit_test.hpp>
#include <boost/system.hpp>

namespace nats_coro::test {

BOOST_AUTO_TEST_SUITE(tests_parse_msg_);

BOOST_AUTO_TEST_CASE(without_reply_to)
{
    auto res = parse_msg("MSG a.b 42 7\r\n1234567\r\n");
    BOOST_TEST(res.op_name() == "MSG");
    BOOST_TEST(res.subject() == "a.b");
    BOOST_TEST(res.subscribe_id() == "42");
    BOOST_TEST(res.reply_to().empty());
    BOOST_TEST(res.payload_size() == 7);
}

BOOST_AUTO_TEST_CASE(with_reply_to)
{
    auto res = parse_msg("MSG c.d subid reply.to 99\r\nddddddddddd");
    BOOST_TEST(res.op_name() == "MSG");
    BOOST_TEST(res.subject() == "c.d");
    BOOST_TEST(res.subscribe_id() == "subid");
    BOOST_TEST(res.reply_to() == "reply.to");
    BOOST_TEST(res.payload_size() == 99);
}

BOOST_AUTO_TEST_CASE(invalid_payload_size)
{
    BOOST_CHECK_THROW(parse_msg("MSG c.c id i0\r\n"), boost::system::system_error);
    BOOST_CHECK_THROW(parse_msg("MSG c.c id reply.to i0\r\n"), boost::system::system_error);
    BOOST_CHECK_THROW(parse_msg("MSG c.c id 1o\r\n"), boost::system::system_error);
    BOOST_CHECK_THROW(parse_msg("MSG c.c id reply.to 1o\r\n"), boost::system::system_error);
}

BOOST_AUTO_TEST_SUITE_END();

BOOST_AUTO_TEST_CASE(tests_ControlLineView_default_ctor)
{
    auto res = ControlLineView();
    BOOST_TEST(res.op_name().empty());
    BOOST_TEST(res.subject().empty());
    BOOST_TEST(res.subscribe_id().empty());
    BOOST_TEST(res.reply_to().empty());
    BOOST_TEST(res.payload_size() == 0);
}

BOOST_AUTO_TEST_CASE(tests_make_message_)
{
    auto packet = make_message("MSG q.w ID to.reply 9\r\n123456789\r\n");
    BOOST_TEST(packet.head().op_name() == "MSG");
    BOOST_TEST(packet.head().subject() == "q.w");
    BOOST_TEST(packet.head().subscribe_id() == "ID");
    BOOST_TEST(packet.head().reply_to() == "to.reply");
    BOOST_TEST(packet.head().payload_size() == 9);
    BOOST_TEST(packet.payload() == "123456789");
}

} // namespace nats_coro::test
