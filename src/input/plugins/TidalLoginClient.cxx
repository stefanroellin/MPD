/*
 * Copyright 2003-2018 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "TidalLoginClient.hxx"
#include "lib/curl/Global.hxx"
#include "lib/curl/Request.hxx"
#include "lib/curl/Form.hxx"
#include "event/Call.hxx"
#include "util/RuntimeError.hxx"

TidalLoginClient::TidalLoginClient(EventLoop &event_loop,
				   const char *_base_url, const char *_token,
				   const char *_username,
				   const char *_password) noexcept
	:base_url(_base_url), token(_token),
	 username(_username), password(_password),
	 curl(event_loop),
	 defer_invoke_handlers(event_loop,
			       BIND_THIS_METHOD(InvokeHandlers))
{
	request_headers.Append((std::string("X-Tidal-Token:")
				+ token).c_str());
}

TidalLoginClient::~TidalLoginClient() noexcept
{
	BlockingCall(GetEventLoop(), [this](){
			request.reset();
		});
}

void
TidalLoginClient::AddLoginHandler(TidalLoginHandler &h) noexcept
{
	const std::lock_guard<Mutex> protect(mutex);
	assert(!h.is_linked());

	const bool was_empty = login_handlers.empty();
	login_handlers.push_front(h);

	if (was_empty && session.empty()) {
		// TODO: throttle login attempts?

		std::string login_uri(base_url);
		login_uri += "/login/username";

		try {
			CurlResponseHandler &handler = *this;
			request = std::make_unique<CurlRequest>(*curl,
								login_uri.c_str(),
								handler);
			request->SetOption(CURLOPT_HTTPHEADER, request_headers.Get());

			request->SetOption(CURLOPT_COPYPOSTFIELDS,
					   EncodeForm(request->Get(),
						      {{"username", username}, {"password", password}}).c_str());

			BlockingCall(GetEventLoop(), [this](){
					request->Start();
				});
		} catch (...) {
			error = std::current_exception();
			ScheduleInvokeHandlers();
			return;
		}
	}
}

void
TidalLoginClient::OnHeaders(unsigned status,
			    std::multimap<std::string, std::string> &&headers)
{
	if (status != 200)
		throw FormatRuntimeError("Status %u from Tidal", status);

	auto i = headers.find("content-type");
	if (i == headers.end() || i->second.find("/json") == i->second.npos)
		throw std::runtime_error("Not a JSON response from Tidal");
}

void
TidalLoginClient::OnData(ConstBuffer<void> data)
{
	constexpr size_t max_size = 4096;
	size_t remaining = max_size - response_body.length();
	if (data.size > remaining)
		throw std::runtime_error("Login response is too large");

	response_body.append((const char *)data.data, data.size);
}

void
TidalLoginClient::OnEnd()
{
	/* TODO: replace poor man's JSON parser */
	auto start = response_body.find("\"sessionId\":\"");
	if (start == response_body.npos)
		throw std::runtime_error("No sessionId in login response");

	start += 13;

	auto end = response_body.find('"', start);
	if (end == response_body.npos)
		throw std::runtime_error("No sessionId in login response");

	session.assign(response_body, start, end - start);
	response_body.clear();

	ScheduleInvokeHandlers();
}

void
TidalLoginClient::OnError(std::exception_ptr e) noexcept
{
	{
		const std::lock_guard<Mutex> protect(mutex);
		error = e;
	}

	ScheduleInvokeHandlers();
}
