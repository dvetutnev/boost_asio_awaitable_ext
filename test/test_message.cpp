#include "message.h"

#include <boost/test/unit_test.hpp>
#include <boost/system.hpp>

namespace nats_coro::test {

BOOST_AUTO_TEST_SUITE(tests_Message);

BOOST_AUTO_TEST_CASE(_)
{
    {
        Message msg = Message::parse("MSG a.b 42 7\r\npayload\r\n");
        BOOST_TEST(msg[Field::op_name] == "MSG");
        BOOST_TEST(msg[Field::subject] == "a.b");
        BOOST_TEST(msg[Field::subscribe_id] == "42");
        BOOST_TEST(msg[Field::payload] == "payload");
    }
    {
        Message msg = Message::parse("MSG c.d _id 4\r\ndata\r\n");
        BOOST_TEST(msg[Field::op_name] == "MSG");
        BOOST_TEST(msg[Field::subject] == "c.d");
        BOOST_TEST(msg[Field::subscribe_id] == "_id");
        BOOST_TEST(msg[Field::payload] == "data");
    }
}

BOOST_AUTO_TEST_CASE(reply_to)
{
    Message msg = Message::parse("MSG e.f subid reply.to 11\r\nHello world\r\n");
    BOOST_TEST(msg[Field::op_name] == "MSG");
    BOOST_TEST(msg[Field::subject] == "e.f");
    BOOST_TEST(msg[Field::subscribe_id] == "subid");
    BOOST_TEST(msg[Field::reply_to] == "reply.to");
    BOOST_TEST(msg[Field::payload] == "Hello world");
}

BOOST_AUTO_TEST_CASE(bad_size)
{
    try {
        Message::parse("MSG x.y.z sub79 1i\r\nhello world\r\n");
        BOOST_FAIL("Exception not throwed");
    } catch (const boost::system::system_error& ex) {
        std::cout << ex.what() << std::endl;
    }
}

BOOST_AUTO_TEST_CASE(empty)
{
    try {
        Message::parse(std::string{});
        BOOST_FAIL("Exception not throwed");
    } catch (const boost::system::system_error& ex) {
        std::cout << ex.what() << std::endl;
    }
}

BOOST_AUTO_TEST_CASE(not_complete)
{
    try {
        Message::parse("MSG a.b 42 11\r\nhello w..\r\n");
        BOOST_FAIL("Exception not throwed");
    } catch (const boost::system::system_error& ex) {
        std::cout << ex.what() << std::endl;
    }
}

BOOST_AUTO_TEST_CASE(to_long)
{
    try {
        Message::parse("MSG a.b 42 11\r\nhello world!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
        BOOST_FAIL("Exception not throwed");
    } catch (const boost::system::system_error& ex) {
        std::cout << ex.what() << std::endl;
    }
}

BOOST_AUTO_TEST_SUITE_END();

} // namespace nats_coro::test
