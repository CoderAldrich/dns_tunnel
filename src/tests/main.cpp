#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#define usleep Sleep
#else
#include <unistd.h>
#endif
#include <iostream>
#include <algorithm>
#include <vector>
#include <string>
#include "../ip_utility.h"
#include "../hsocket.h"
#include "../dns_packet.h"
#include "../message.h"
#include "../tunnel_dns.h"
#include "../convert_utility.h"
#include "../pque.h"
#include "../vec.h"

/* A subclass for testing purposes,
 * where the header of the dns_packet 
 * is exposed through a member function
 */
class dns_test_t : public dns_packet {
    public:
        dns::DNS_HEADER get_header(){ return this->header; }
        dns_test_t(unsigned short id, dns::dns_type type) : dns_packet(id,type) {};
        dns_test_t(const std::string& str) : dns_packet(str) {};
};


// UDP socket tests
std::string test_udp (int& success);
std::string test_nonblocking_udp (int& success);

// DNS packet tests
std::string test_dns_to_str_questions (int& success);
std::string test_dns_to_str_answers (int& success);
std::string test_dns_query (int& success);

// message class tests
std::string test_message_to_str (int& success);

// convert utility tests
std::string test_convert_util (int& success);

// tunnel_dns tests
std::string test_tunnel_dns_out_q (int& success);
std::string test_tunnel_dns_in_out_q (int& success);
std::string test_tunnel_dns_out_r (int& success);

// pque tests
std::string test_pque (int& success);

// vec tests
std::string test_vec (int& success);

int main(int argc, char** argv) {
    std::cout << "Running tests...\n";
    std::vector<std::string(*)(int&)> tests = {
        test_udp, test_nonblocking_udp,
        test_dns_to_str_questions, test_dns_to_str_answers, test_dns_query,
        test_message_to_str,
        test_convert_util,
        test_tunnel_dns_out_q,
        test_tunnel_dns_in_out_q,
        test_tunnel_dns_out_r,
        test_pque,
        test_vec };

    int succeeded = tests.size();
    for ( auto test : tests) {
        int success = 0;
        std::string test_name = test(success);
        if (success) {
            std::cout << "\e[38;5;40mSuccess: ";
        } else {
            std::cout << "\e[38;5;196mFail: ";
            succeeded--;
        }
        std::cout << test_name << "\e[39m\n";
    }
    if (succeeded == tests.size()) {
        std::cout << "All tests passed!\n";
        return 0;
    } else {
        std::cout << succeeded << " tests passed, " << tests.size()-succeeded << " tests failed\n";
        return 1;
    }
}

/* Tests are defined here
 * 
 */

// hsocket tests start

std::string test_udp (int& success) {
    hsocket s(hsocket::UDP);
    s.bind(1337,"127.0.0.1");
    hsocket sender(hsocket::UDP);
    sender.connect(1337,"127.0.0.1");

    std::string data = "udp_listen_test";
    std::string received;

    sender << data;
    s >> received;

    s.close();
    sender.close();
    success = data == received;
    return "UDP blocking test";
}
std::string test_nonblocking_udp (int& success) {
    hsocket listener(hsocket::UDP);
    hsocket connector(hsocket::UDP);
    listener.bind(1234,"127.0.0.1");
    listener << hsocket::NONBLOCKING;
    connector << hsocket::NONBLOCKING;
    connector.connect(1234,"127.0.0.1");
    success = 0;
    int sent = 0;
    std::string recieved;
    std::string data = "Test data 123123";
    while (1) {
        usleep(100);
        listener >> recieved;
        if (!sent) {
            connector << data;
            sent = 1;
        } else {
            if (recieved.length())
                break;
        }
    }
    listener.close();
    connector.close();
    success = recieved == data;
    return "UDP nonblocking test";
}

// dns tests start

std::string test_dns_to_str_questions (int& success) {
    // create a dns query packet with two questions
    dns_test_t first (1337,dns::query_t);
    first.add_question("helsinki.fi", dns::A);
    first.add_question("helsinki.fi", dns::ANY);

    // create the second from the byte representation of the first
    std::string d = first.str();
    dns_test_t second(d);
    
    success = 1;
    // compare the two
    if (first.get_header() != second.get_header()) {
        std::cout << "testing headers failed\n";
        first.print_header();
        second.print_header();
        success = 0;
    }

    if (first.questions.size() == second.questions.size()) {
        for (int i = 0 ; i < 2; ++i) {
            if (first.questions[i] != second.questions[i]) {
                first.print_questions();
                second.print_questions();
                success = 0;
                break;
            }
        }
    } else {
        success = 0;
    }
    return "DNS to str test with questions";
}
std::string test_dns_to_str_answers (int& success) {
    // create a dns query packet with two questions
    dns_test_t first (1337,dns::query_t);
    first.add_question("helsinki.fi", dns::A);
    // add response
    first.add_response(ipton("127.0.0.1"),dns::A);

    // create the second from the byte representation of the first
    std::string d = first.str();
    dns_test_t second(d);
    
    success = 1;
    // compare the two
    if (first.get_header() != second.get_header()) {
        std::cout << "testing headers failed\n";
        first.print_header();
        second.print_header();
        success = 0;
    }

    if (first.questions.size() == second.questions.size()) {
        for (int i = 0 ; i < first.questions.size(); ++i) {
            if (first.questions[i] != second.questions[i]) {
                first.print_questions();
                second.print_questions();
                success = 0;
                break;
            }
        }
    } else if (first.responses.size() == second.responses.size()) {
        for (int i = 0 ; i < first.responses.size(); ++i) {
            if (first.responses[i] != second.responses[i]) {
                first.print_responses();
                second.print_responses();
                success = 0;
                break;
            }
        }
    } else {
        success = 0;
    }
    return "DNS to str test with questions and answers";
}
/*
 * Asks google dns 8.8.8.8 for an A record of helsinki.fi.
 * expected result is 128.214.222.24.
 */
std::string test_dns_query (int& success) {
    // title
    std::string ret = "DNS real dns query for helsinki.fi";

    // form a network byte order data string
    std::string expected_ip = iptonstr("128.214.222.24");
    hsocket s(hsocket::UDP);
    // Googles DNS
    s.connect(53,"8.8.8.8");
    // create the packet
    dns_test_t query(1337,dns::query_t);
    query.add_question("helsinki.fi",dns::A);
    std::string recieved;
    int counter = 0;
    // try to send 2 times
    s << hsocket::timeout(1);
    while (recieved.empty()) {
        s << query.str();
        s >> recieved;
        counter++;
        if (counter > 2) {
            success = 0;
            std::cout << "Error: timeout, retry limit for DNS query to 8.8.8.8 exceeded (> 2)\n";
            return ret;
        }
    }
    // check the response
    success = 0;
    dns_test_t resp(recieved);

    if (resp.get_header().rcode != dns::NON_ERROR) {
        std::cout << "Error: query failed\n";
        resp.print_header();
        return ret;
    }
    if (!resp.responses.size()) {
        std::cout << "no responses from 8.8.8.8\n";
        return ret;
    }

    for (auto r : resp.responses) {
        if (ntohs(r.resource.type) == dns::A) {
            success |= r.data == expected_ip;
        }
    }

    return ret;
}


std::string test_message_to_str (int& success) {
    std::string ret = "Message to str test";

    message hbeat(message::HEARTBEAT,"heartbeat_test");
    std::string bytes = hbeat.str();
    message result(bytes);
    success = 1;
    if (result != hbeat) {
        std::cout << "HEARTBEAT message failed\n";
        success = 0;
    }

    message ok(message::OK, "oktest");
    bytes = ok.str();
    message ok_result(bytes);
    if (ok_result != ok) {
        std::cout << "OK message failed\n";
        success = 0;
    }

    message not_ok(message::M_ERROR, "error_test");
    bytes = not_ok.str();
    message not_ok_result(bytes);
    if (not_ok == not_ok_result) {
        std::cout << "ERROR message failed\n";
        success = 0;
    }

    return ret;
}

std::string test_convert_util (int& success) {
    std::string ret = "Convert utility test";
    std::string data = "0123456789ABCDEFHIJKLMNOPQRSTUVWXYZ";
    std::string result = to_hex(data,data.length());
    success = (data == from_hex(result)) ? 1 : 0;
    if (!success) {
        std::cout << "Result was " << from_hex(result) << '\n';
    }
    return ret;
}


std::string test_tunnel_dns_out_q (int& success) {
    std::string ret = "Tunnel DNS outgoing query test";
    tunnel_dns tun(tunnel::OUTGOING,dns::query_t,dns::A,"helsinki.fi");
    std::string data = "AAAA";
    tun << data;
    std::string output;
    tun >> output;
    success = (output == "41414141.helsinki.fi") ? 1 : 0;
    if (!success) {
        std::cout << "Output was " << output << '\n';
    }
    data = "AAA AAA AAA AAA AAA AAA AAA AAAA";
    tun << data;
    output.clear();
    tun >> output;
    // longer output so it gets split into two labels
    if (output != "414141204141412041414120414141204141412041414120414141204141.4141.helsinki.fi")
        success = 0;
    if (!success) {
        std::cout << "Output was " << output << '\n';
    }
    return ret;
}
std::string test_tunnel_dns_in_out_q (int& success) {
    std::string ret = "Tunnel DNS outgoing and incoming query test";
    tunnel_dns tun_out(tunnel::OUTGOING,dns::query_t,dns::A,"helsinki.fi");
    tunnel_dns tun_in(tunnel::INCOMING,dns::query_t,dns::A,"helsinki.fi");
    std::string data = "This is a test", dns_data, result;

    tun_out << data;
    tun_out >> dns_data;

    tun_in << dns_data;
    tun_in >> result;

    success = (data == result) ? 1 : 0;
    if (!success) {
        std::cout << "Output was " << result << " instead of " << data << '\n';
    }
    return ret;
}
std::string test_tunnel_dns_out_r (int& success) {
    std::string ret = "Tunnel DNS outgoing response test";
    tunnel_dns tun(tunnel::OUTGOING, dns::response_t, dns::A,"helsinki.fi");
    std::string data = "test", result;
    std::vector<std::string> responses;
    std::vector<std::string> results;

    results.push_back("0.4.116.101");
    results.push_back("1.115.116.0");

    tun.set_response_limit(10);
    tun << data;
    for (int i = 0; i < 10; i++) {
        std::string temp;
        tun >> temp;
        if (temp.empty()) break;
        std::string ip = ntoipstr(temp);
        responses.push_back(ip);
    }
    success = (responses == results) ? 1 : 0;
    if (!success) {
        for (auto a : responses) {
            std::cout << "IP " << a << '\n';
        }
    }
    return ret;
}

std::string test_pque (int& success) {
    std::string ret = "Pque test";

    std::vector<std::string> v,vres;
    v.push_back("8test");
    v.push_back("3test");
    v.push_back("5test");
    v.push_back("2test");
    v.push_back("0test");
    v.push_back("3test");
    v.push_back("1test");
    v.push_back("atest");

    // new queue
    pque<std::string> Q(1);
    for (auto s : v)
        Q.insert(s);
    while(Q.size() > 0) {
        vres.push_back(Q.pop());
        Q.remove();
    }
    std::sort(v.begin(),v.end(),std::less<std::string>());

    success = vres == v;
    if (!success) {
        for (auto s : v)
            std::cout << s << '\n';
        std::cout << '\n';
        for (auto s : vres)
            std::cout << s << '\n';
    }
    return ret;
}

std::string test_vec (int& success) {
    std::string ret = "Vec test";
    vec<std::string> v(1);
    v.push_back("1test");
    v.push_back("2test");
    v.push_back("3test");
    v.push_back("4test");
    std::vector<std::string> res ({ "1test","2test","3test","4test" });
    success = res.size() == v.size();
    for (int i = 0; i < std::min(res.size(),v.size()); i++) {
        success &= v[i] == res[i];
    }
    return ret;

}

