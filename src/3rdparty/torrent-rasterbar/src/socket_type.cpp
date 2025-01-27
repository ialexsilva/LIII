/*

Copyright (c) 2009-2016, Arvid Norberg
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#include "libtorrent/config.hpp"
#include "libtorrent/socket_type.hpp"
#include "libtorrent/aux_/openssl.hpp"

#ifdef TORRENT_USE_OPENSSL
#include <boost/asio/ssl/context.hpp>

#if BOOST_VERSION >= 104700
#include <boost/asio/ssl/rfc2818_verification.hpp>
#endif

#endif

#if defined TORRENT_ASIO_DEBUGGING
#include "libtorrent/debug.hpp"
#endif

namespace libtorrent
{

	bool is_ssl(socket_type const& s)
	{
#ifdef TORRENT_USE_OPENSSL
#define CASE(t) case socket_type_int_impl<ssl_stream<t> >::value:
		switch (s.type())
		{
			CASE(tcp::socket)
			CASE(socks5_stream)
			CASE(http_stream)
			CASE(utp_stream)
				return true;
			default: return false;
		};
#undef CASE
#else
		TORRENT_UNUSED(s);
		return false;
#endif
	}

	bool is_utp(socket_type const& s)
	{
		return s.get<utp_stream>()
#ifdef TORRENT_USE_OPENSSL
			|| s.get<ssl_stream<utp_stream> >()
#endif
			;
	}

#if TORRENT_USE_I2P
	bool is_i2p(socket_type const& s)
	{
		return s.get<i2p_stream>()
#ifdef TORRENT_USE_OPENSSL
			|| s.get<ssl_stream<i2p_stream> >()
#endif
			;
	}
#endif

	void setup_ssl_hostname(socket_type& s, std::string const& hostname, error_code& ec)
	{
#if defined TORRENT_USE_OPENSSL && BOOST_VERSION >= 104700
		// for SSL connections, make sure to authenticate the hostname
		// of the certificate
#define CASE(t) case socket_type_int_impl<ssl_stream<t> >::value: \
		s.get<ssl_stream<t> >()->set_verify_callback( \
			boost::asio::ssl::rfc2818_verification(hostname), ec); \
		ssl = s.get<ssl_stream<t> >()->native_handle(); \
		ctx = SSL_get_SSL_CTX(ssl); \
		break;

		SSL* ssl = 0;
		SSL_CTX* ctx = 0;

		switch(s.type())
		{
			CASE(tcp::socket)
			CASE(socks5_stream)
			CASE(http_stream)
			CASE(utp_stream)
		}
#undef CASE

#if OPENSSL_VERSION_NUMBER >= 0x90812f
		if (ctx)
		{
			aux::openssl_set_tlsext_servername_callback(ctx, 0);
			aux::openssl_set_tlsext_servername_arg(ctx, 0);
		}
#endif // OPENSSL_VERSION_NUMBER

#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
		if (ssl)
		{
			aux::openssl_set_tlsext_hostname(ssl, hostname.c_str());
		}
#endif

#else
		TORRENT_UNUSED(ec);
		TORRENT_UNUSED(hostname);
		TORRENT_UNUSED(s);
#endif
	}

#ifdef TORRENT_USE_OPENSSL
	namespace {

	void nop(boost::shared_ptr<void>) {}

	void on_close_socket(socket_type* s, boost::shared_ptr<void>)
	{
#if defined TORRENT_ASIO_DEBUGGING
		complete_async("on_close_socket");
#endif
		error_code ec;
		s->close(ec);
	}

	} // anonymous namespace
#endif

	// the second argument is a shared pointer to an object that
	// will keep the socket (s) alive for the duration of the async operation
	void async_shutdown(socket_type& s, boost::shared_ptr<void> holder)
	{
		error_code e;

#ifdef TORRENT_USE_OPENSSL
		// for SSL connections, first do an async_shutdown, before closing the socket
#if defined TORRENT_ASIO_DEBUGGING
#define MAYBE_ASIO_DEBUGGING add_outstanding_async("on_close_socket");
#else
#define MAYBE_ASIO_DEBUGGING
#endif

	char const buffer[] = "";
	// chasing the async_shutdown by a write is a trick to close the socket as
	// soon as we've sent the close_notify, without having to wait to receive a
	// response from the other end
	// https://stackoverflow.com/questions/32046034/what-is-the-proper-way-to-securely-disconnect-an-asio-ssl-socket

#define CASE(t) case socket_type_int_impl<ssl_stream<t> >::value: \
	MAYBE_ASIO_DEBUGGING \
	s.get<ssl_stream<t> >()->async_shutdown(boost::bind(&nop, holder)); \
	s.get<ssl_stream<t> >()->async_write_some(boost::asio::buffer(buffer), boost::bind(&on_close_socket, &s, holder)); \
	break;

		switch (s.type())
		{
			CASE(tcp::socket)
			CASE(socks5_stream)
			CASE(http_stream)
			CASE(utp_stream)
			default: s.close(e); break;
		}
#undef CASE
#else
		TORRENT_UNUSED(holder);
		s.close(e);
#endif // TORRENT_USE_OPENSSL
	}

	void socket_type::destruct()
	{
		typedef tcp::socket tcp_socket;
		switch (m_type)
		{
			case 0: break;
			case socket_type_int_impl<tcp::socket>::value:
				get<tcp::socket>()->~tcp_socket();
				break;
			case socket_type_int_impl<socks5_stream>::value:
				get<socks5_stream>()->~socks5_stream();
				break;
			case socket_type_int_impl<http_stream>::value:
				get<http_stream>()->~http_stream();
				break;
			case socket_type_int_impl<utp_stream>::value:
				get<utp_stream>()->~utp_stream();
				break;
#if TORRENT_USE_I2P
			case socket_type_int_impl<i2p_stream>::value:
				get<i2p_stream>()->~i2p_stream();
				break;
#endif
#ifdef TORRENT_USE_OPENSSL
			case socket_type_int_impl<ssl_stream<tcp::socket> >::value:
				get<ssl_stream<tcp::socket> >()->~ssl_stream();
				break;
			case socket_type_int_impl<ssl_stream<socks5_stream> >::value:
				get<ssl_stream<socks5_stream> >()->~ssl_stream();
				break;
			case socket_type_int_impl<ssl_stream<http_stream> >::value:
				get<ssl_stream<http_stream> >()->~ssl_stream();
				break;
			case socket_type_int_impl<ssl_stream<utp_stream> >::value:
				get<ssl_stream<utp_stream> >()->~ssl_stream();
				break;
#endif
			default: TORRENT_ASSERT(false);
		}
		m_type = 0;
	}

	void socket_type::construct(int type, void* userdata)
	{
#ifndef TORRENT_USE_OPENSSL
		TORRENT_UNUSED(userdata);
#endif

		destruct();
		switch (type)
		{
			case 0: break;
			case socket_type_int_impl<tcp::socket>::value:
				new (reinterpret_cast<tcp::socket*>(&m_data)) tcp::socket(m_io_service);
				break;
			case socket_type_int_impl<socks5_stream>::value:
				new (reinterpret_cast<socks5_stream*>(&m_data)) socks5_stream(m_io_service);
				break;
			case socket_type_int_impl<http_stream>::value:
				new (reinterpret_cast<http_stream*>(&m_data)) http_stream(m_io_service);
				break;
			case socket_type_int_impl<utp_stream>::value:
				new (reinterpret_cast<utp_stream*>(&m_data)) utp_stream(m_io_service);
				break;
#if TORRENT_USE_I2P
			case socket_type_int_impl<i2p_stream>::value:
				new (reinterpret_cast<i2p_stream*>(&m_data)) i2p_stream(m_io_service);
				break;
#endif
#ifdef TORRENT_USE_OPENSSL
			case socket_type_int_impl<ssl_stream<tcp::socket> >::value:
				TORRENT_ASSERT(userdata);
				new (reinterpret_cast<ssl_stream<tcp::socket>*>(&m_data)) ssl_stream<tcp::socket>(m_io_service
					, *static_cast<ssl::context*>(userdata));
				break;
			case socket_type_int_impl<ssl_stream<socks5_stream> >::value:
				TORRENT_ASSERT(userdata);
				new (reinterpret_cast<ssl_stream<socks5_stream>*>(&m_data)) ssl_stream<socks5_stream>(m_io_service
					, *static_cast<ssl::context*>(userdata));
				break;
			case socket_type_int_impl<ssl_stream<http_stream> >::value:
				TORRENT_ASSERT(userdata);
				new (reinterpret_cast<ssl_stream<http_stream>*>(&m_data)) ssl_stream<http_stream>(m_io_service
					, *static_cast<ssl::context*>(userdata));
				break;
			case socket_type_int_impl<ssl_stream<utp_stream> >::value:
				TORRENT_ASSERT(userdata);
				new (reinterpret_cast<ssl_stream<utp_stream>*>(&m_data)) ssl_stream<utp_stream>(m_io_service
					, *static_cast<ssl::context*>(userdata));
				break;
#endif
			default: TORRENT_ASSERT(false);
		}

		m_type = type;
	}

	char const* socket_type::type_name() const
	{
		static char const* const names[] =
		{
			"uninitialized",
			"TCP",
			"Socks5",
			"HTTP",
			"uTP",
#if TORRENT_USE_I2P
			"I2P",
#else
			"",
#endif
#ifdef TORRENT_USE_OPENSSL
			"SSL/TCP",
			"SSL/Socks5",
			"SSL/HTTP",
			"SSL/uTP"
#else
			"","","",""
#endif
		};
		return names[m_type];
	}

	io_service& socket_type::get_io_service() const
	{ return m_io_service; }

	socket_type::~socket_type()
	{ destruct(); }

	bool socket_type::is_open() const
	{
		if (m_type == 0) return false;
		TORRENT_SOCKTYPE_FORWARD_RET(is_open(), false)
	}

	void socket_type::open(protocol_type const& p, error_code& ec)
	{ TORRENT_SOCKTYPE_FORWARD(open(p, ec)) }

	void socket_type::close(error_code& ec)
	{
		if (m_type == 0) return;
		TORRENT_SOCKTYPE_FORWARD(close(ec))
	}

	void socket_type::set_close_reason(boost::uint16_t code)
	{
		switch (m_type)
		{
			case socket_type_int_impl<utp_stream>::value:
				get<utp_stream>()->set_close_reason(code);
				break;
#ifdef TORRENT_USE_OPENSSL
			case socket_type_int_impl<ssl_stream<utp_stream> >::value:
				get<ssl_stream<utp_stream> >()->lowest_layer().set_close_reason(code);
				break;
#endif
			default: break;
		}
	}

	boost::uint16_t socket_type::get_close_reason()
	{
		switch (m_type)
		{
			case socket_type_int_impl<utp_stream>::value:
				return get<utp_stream>()->get_close_reason();
#ifdef TORRENT_USE_OPENSSL
			case socket_type_int_impl<ssl_stream<utp_stream> >::value:
				return get<ssl_stream<utp_stream> >()->lowest_layer().get_close_reason();
#endif
			default: return 0;
		}
	}

	socket_type::endpoint_type socket_type::local_endpoint(error_code& ec) const
	{ TORRENT_SOCKTYPE_FORWARD_RET(local_endpoint(ec), socket_type::endpoint_type()) }

	socket_type::endpoint_type socket_type::remote_endpoint(error_code& ec) const
	{ TORRENT_SOCKTYPE_FORWARD_RET(remote_endpoint(ec), socket_type::endpoint_type()) }

	void socket_type::bind(endpoint_type const& endpoint, error_code& ec)
	{ TORRENT_SOCKTYPE_FORWARD(bind(endpoint, ec)) }

	std::size_t socket_type::available(error_code& ec) const
	{ TORRENT_SOCKTYPE_FORWARD_RET(available(ec), 0) }

	int socket_type::type() const { return m_type; }

#ifndef BOOST_NO_EXCEPTIONS
	void socket_type::open(protocol_type const& p)
	{ TORRENT_SOCKTYPE_FORWARD(open(p)) }

	void socket_type::close()
	{
		if (m_type == 0) return;
		TORRENT_SOCKTYPE_FORWARD(close())
	}

	socket_type::endpoint_type socket_type::local_endpoint() const
	{ TORRENT_SOCKTYPE_FORWARD_RET(local_endpoint(), socket_type::endpoint_type()) }

	socket_type::endpoint_type socket_type::remote_endpoint() const
	{ TORRENT_SOCKTYPE_FORWARD_RET(remote_endpoint(), socket_type::endpoint_type()) }

	void socket_type::bind(endpoint_type const& endpoint)
	{ TORRENT_SOCKTYPE_FORWARD(bind(endpoint)) }

	std::size_t socket_type::available() const
	{ TORRENT_SOCKTYPE_FORWARD_RET(available(), 0) }
#endif

}

