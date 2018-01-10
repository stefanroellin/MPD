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

#ifndef TIDAL_LOGIN_CLIENT_HXX
#define TIDAL_LOGIN_CLIENT_HXX

#include "check.h"
#include "lib/curl/Init.hxx"
#include "lib/curl/Handler.hxx"
#include "lib/curl/Slist.hxx"
#include "thread/Mutex.hxx"
#include "event/DeferEvent.hxx"

#include <boost/intrusive/list.hpp>

#include <memory>
#include <string>

class CurlRequest;

class TidalLoginHandler
	: public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::safe_link>>
{
public:
	virtual void OnTidalLogin() noexcept = 0;
};

class TidalLoginClient final : CurlResponseHandler {
	/**
	 * The Tidal API base URL.
	 */
	const char *const base_url;

	/**
	 * The configured Tidal application token.
	 */
	const char *const token;

	/**
	 * The configured Tidal user name.
	 */
	const char *const username;

	/**
	 * The configured Tidal password.
	 */
	const char *const password;

	CurlInit curl;

	CurlSlist request_headers;

	std::unique_ptr<CurlRequest> request;
	std::string response_body;

	DeferEvent defer_invoke_handlers;

	/**
	 * Protects #session, #error and #login_handlers.
	 */
	mutable Mutex mutex;

	std::exception_ptr error;

	/**
	 * The current Tidal session id, empty if none.
	 */
	std::string session;

	typedef boost::intrusive::list<TidalLoginHandler,
				       boost::intrusive::constant_time_size<false>> LoginHandlerList;

	LoginHandlerList login_handlers;

public:
	TidalLoginClient(EventLoop &event_loop,
			 const char *_base_url, const char *_token,
			 const char *_username,
			 const char *_password) noexcept;

	~TidalLoginClient() noexcept;

	EventLoop &GetEventLoop() noexcept {
		return defer_invoke_handlers.GetEventLoop();
	}

	const char *GetBaseUrl() const noexcept {
		return base_url;
	}

	void AddLoginHandler(TidalLoginHandler &h) noexcept;

	void RemoveLoginHandler(TidalLoginHandler &h) noexcept {
		const std::lock_guard<Mutex> protect(mutex);
		if (h.is_linked())
			login_handlers.erase(login_handlers.iterator_to(h));
	}

	const char *GetToken() const noexcept {
		return token;
	}

	std::string GetSession() const {
		const std::lock_guard<Mutex> protect(mutex);

		if (error)
			std::rethrow_exception(error);

		if (session.empty())
			throw std::runtime_error("No session");

		return session;
	}

private:
	void InvokeHandlers() noexcept {
		const std::lock_guard<Mutex> protect(mutex);
		while (!login_handlers.empty()) {
			auto &h = login_handlers.front();
			login_handlers.pop_front();

			const ScopeUnlock unlock(mutex);
			h.OnTidalLogin();
		}
	}

	void ScheduleInvokeHandlers() noexcept {
		defer_invoke_handlers.Schedule();
	}

	/* virtual methods from CurlResponseHandler */
	void OnHeaders(unsigned status,
		       std::multimap<std::string, std::string> &&headers) override;
	void OnData(ConstBuffer<void> data) override;
	void OnEnd() override;
	void OnError(std::exception_ptr e) noexcept override;
};

#endif
