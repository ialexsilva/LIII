/*

Copyright (c) 2003-2016, Arvid Norberg
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

#ifndef TORRENT_TORRENT_HANDLE_HPP_INCLUDED
#define TORRENT_TORRENT_HANDLE_HPP_INCLUDED

#include "libtorrent/config.hpp"

#include "libtorrent/aux_/disable_warnings_push.hpp"

#include <vector>
#include <set>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/function.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/cstdint.hpp>

#ifndef TORRENT_NO_DEPRECATE
// for deprecated force_reannounce
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#endif

#include "libtorrent/aux_/disable_warnings_pop.hpp"

#include "libtorrent/address.hpp"
#include "libtorrent/socket.hpp" // tcp::endpoint

namespace libtorrent
{
	namespace aux
	{
		struct session_impl;
	}

	class entry;
	struct pool_file_status;
	struct announce_entry;
	class torrent_info;
	struct torrent_plugin;
	struct peer_info;
	struct peer_list_entry;
	struct torrent_status;
	struct torrent_handle;
	class sha1_hash;
	struct storage_interface;
	class torrent;

	// allows torrent_handle to be used in unordered_map and unordered_set.
	TORRENT_EXPORT std::size_t hash_value(torrent_status const& ts);

#ifndef BOOST_NO_EXCEPTIONS
	void throw_invalid_handle() TORRENT_NO_RETURN;
#endif

	using boost::shared_ptr;
	using boost::make_shared;

	// holds the state of a block in a piece. Who we requested
	// it from and how far along we are at downloading it.
	struct TORRENT_EXPORT block_info
	{
		// this is the enum used for the block_info::state field.
		enum block_state_t
		{
			// This block has not been downloaded or requested form any peer.
			none,
			// The block has been requested, but not completely downloaded yet.
			requested,
			// The block has been downloaded and is currently queued for being
			// written to disk.
			writing,
			// The block has been written to disk.
			finished
		};

	private:
		TORRENT_UNION addr_t
		{
			address_v4::bytes_type v4;
#if TORRENT_USE_IPV6
			address_v6::bytes_type v6;
#endif
		} addr;

		boost::uint16_t port;
	public:

		// The peer is the ip address of the peer this block was downloaded from.
		void set_peer(tcp::endpoint const& ep)
		{
#if TORRENT_USE_IPV6
			is_v6_addr = ep.address().is_v6();
			if (is_v6_addr)
				addr.v6 = ep.address().to_v6().to_bytes();
			else
#endif
				addr.v4 = ep.address().to_v4().to_bytes();
			port = ep.port();
		}
		tcp::endpoint peer() const
		{
#if TORRENT_USE_IPV6
			if (is_v6_addr)
				return tcp::endpoint(address_v6(addr.v6), port);
			else
#endif
				return tcp::endpoint(address_v4(addr.v4), port);
		}

		// the number of bytes that have been received for this block
		unsigned bytes_progress:15;

		// the total number of bytes in this block.
		unsigned block_size:15;

		// the state this block is in (see block_state_t)
		unsigned state:2;

		// the number of peers that is currently requesting this block. Typically
		// this is 0 or 1, but at the end of the torrent blocks may be requested
		// by more peers in parallel to speed things up.
		unsigned num_peers:14;
	private:
#if TORRENT_USE_IPV6
		// the type of the addr union
		unsigned is_v6_addr:1;
#endif
	};

	// This class holds information about pieces that have outstanding requests
	// or outstanding writes
	struct TORRENT_EXPORT partial_piece_info
	{
		// the index of the piece in question. ``blocks_in_piece`` is the number
		// of blocks in this particular piece. This number will be the same for
		// most pieces, but
		// the last piece may have fewer blocks than the standard pieces.
		int piece_index;

		// the number of blocks in this piece
		int blocks_in_piece;

		// the number of blocks that are in the finished state
		int finished;

		// the number of blocks that are in the writing state
		int writing;

		// the number of blocks that are in the requested state
		int requested;

		// this is an array of ``blocks_in_piece`` number of
		// items. One for each block in the piece.
		//
		// .. warning:: This is a pointer that points to an array
		//	that's owned by the session object. The next time
		//	get_download_queue() is called, it will be invalidated.
		block_info* blocks;

#ifndef TORRENT_NO_DEPRECATE
		// the speed classes. These may be used by the piece picker to
		// coalesce requests of similar download rates
		enum state_t { none, slow, medium, fast };

		// the download speed class this piece falls into.
		// this is used internally to cluster peers of the same
		// speed class together when requesting blocks.
		// 
		// set to either ``fast``, ``medium``, ``slow`` or ``none``. It tells
		// which download rate category the peers downloading this piece falls
		// into. ``none`` means that no peer is currently downloading any part of
		// the piece. Peers prefer picking pieces from the same category as
		// themselves. The reason for this is to keep the number of partially
		// downloaded pieces down. Pieces set to ``none`` can be converted into
		// any of ``fast``, ``medium`` or ``slow`` as soon as a peer want to
		// download from it.
		state_t TORRENT_DEPRECATED_MEMBER piece_state;
#else
		// hidden
		enum deprecated_state_t { none, slow, medium, fast };
		deprecated_state_t deprecated_piece_state;
#endif
	};

	// for boost::hash (and to support using this type in unordered_map etc.)
	TORRENT_EXPORT std::size_t hash_value(torrent_handle const& h);

	// You will usually have to store your torrent handles somewhere, since it's
	// the object through which you retrieve information about the torrent and
	// aborts the torrent.
	// 
	// .. warning::
	// 	Any member function that returns a value or fills in a value has to be
	// 	made synchronously. This means it has to wait for the main thread to
	// 	complete the query before it can return. This might potentially be
	// 	expensive if done from within a GUI thread that needs to stay
	// 	responsive. Try to avoid querying for information you don't need, and
	// 	try to do it in as few calls as possible. You can get most of the
	// 	interesting information about a torrent from the
	// 	torrent_handle::status() call.
	// 
	// The default constructor will initialize the handle to an invalid state.
	// Which means you cannot perform any operation on it, unless you first
	// assign it a valid handle. If you try to perform any operation on an
	// uninitialized handle, it will throw ``invalid_handle``.
	// 
	// .. warning::
	// 	All operations on a torrent_handle may throw libtorrent_exception
	// 	exception, in case the handle is no longer referring to a torrent.
	// 	There is one exception is_valid() will never throw. Since the torrents
	// 	are processed by a background thread, there is no guarantee that a
	// 	handle will remain valid between two calls.
	// 
	struct TORRENT_EXPORT torrent_handle
	{
		// TODO: 3 consider replacing all the setters and getters for pause,
		// resume, stop-when-ready, share-mode, upload-mode, super-seeding,
		// apply-ip-filter, resolve-countries, pinned, sequential-download,
		// seed-mode
		// with just set_flags() and clear_flags() using the flags from
		// add_torrent_params. Perhaps those flags should have a more generic
		// name.
		friend class invariant_access;
		friend struct aux::session_impl;
		friend class session;
		friend struct session_handle;
		friend struct feed;
		friend class torrent;
		friend TORRENT_EXPORT std::size_t hash_value(torrent_handle const& th);

		// constructs a torrent handle that does not refer to a torrent.
		// i.e. is_valid() will return false.
		torrent_handle() {}

		torrent_handle(torrent_handle const& t)
		{ if (!t.m_torrent.expired()) m_torrent = t.m_torrent; }

#if __cplusplus >= 201103L
		torrent_handle& operator=(torrent_handle const&) = default;
#endif

		// flags for add_piece().
		enum flags_t { overwrite_existing = 1 };

		// This function will write ``data`` to the storage as piece ``piece``,
		// as if it had been downloaded from a peer. ``data`` is expected to
		// point to a buffer of as many bytes as the size of the specified piece.
		// The data in the buffer is copied and passed on to the disk IO thread
		// to be written at a later point.
		//
		// By default, data that's already been downloaded is not overwritten by
		// this buffer. If you trust this data to be correct (and pass the piece
		// hash check) you may pass the overwrite_existing flag. This will
		// instruct libtorrent to overwrite any data that may already have been
		// downloaded with this data.
		//
		// Since the data is written asynchronously, you may know that is passed
		// or failed the hash check by waiting for piece_finished_alert or
		// hash_failed_alert.
		void add_piece(int piece, char const* data, int flags = 0) const;

		// This function starts an asynchronous read operation of the specified
		// piece from this torrent. You must have completed the download of the
		// specified piece before calling this function.
		// 
		// When the read operation is completed, it is passed back through an
		// alert, read_piece_alert. Since this alert is a response to an explicit
		// call, it will always be posted, regardless of the alert mask.
		// 
		// Note that if you read multiple pieces, the read operations are not
		// guaranteed to finish in the same order as you initiated them.
		void read_piece(int piece) const;

		// Returns true if this piece has been completely downloaded, and false
		// otherwise.
		bool have_piece(int piece) const;

		// internal
		void get_full_peer_list(std::vector<peer_list_entry>& v) const;

		// takes a reference to a vector that will be cleared and filled with one
		// entry for each peer connected to this torrent, given the handle is
		// valid. If the torrent_handle is invalid, it will throw
		// libtorrent_exception exception. Each entry in the vector contains
		// information about that particular peer. See peer_info.
		void get_peer_info(std::vector<peer_info>& v) const;

		// flags to pass in to status() to specify which properties of the
		// torrent to query for. By default all flags are set.
		enum status_flags_t
		{
			// calculates ``distributed_copies``, ``distributed_full_copies`` and
			// ``distributed_fraction``.
			query_distributed_copies = 1,
			// includes partial downloaded blocks in ``total_done`` and
			// ``total_wanted_done``.
			query_accurate_download_counters = 2,
			// includes ``last_seen_complete``.
			query_last_seen_complete = 4,
			// includes ``pieces``.
			query_pieces = 8,
			// includes ``verified_pieces`` (only applies to torrents in *seed
			// mode*).
			query_verified_pieces = 16,
			// includes ``torrent_file``, which is all the static information from
			// the .torrent file.
			query_torrent_file = 32,
			// includes ``name``, the name of the torrent. This is either derived
			// from the .torrent file, or from the ``&dn=`` magnet link argument
			// or possibly some other source. If the name of the torrent is not
			// known, this is an empty string.
			query_name = 64,
			// includes ``save_path``, the path to the directory the files of the
			// torrent are saved to.
			query_save_path = 128
		};

		// ``status()`` will return a structure with information about the status
		// of this torrent. If the torrent_handle is invalid, it will throw
		// libtorrent_exception exception. See torrent_status. The ``flags``
		// argument filters what information is returned in the torrent_status.
		// Some information in there is relatively expensive to calculate, and if
		// you're not interested in it (and see performance issues), you can
		// filter them out.
		// 
		// By default everything is included. The flags you can use to decide
		// what to *include* are defined in the status_flags_t enum.
		torrent_status status(boost::uint32_t flags = 0xffffffff) const;

		// ``get_download_queue()`` takes a non-const reference to a vector which
		// it will fill with information about pieces that are partially
		// downloaded or not downloaded at all but partially requested. See
		// partial_piece_info for the fields in the returned vector.
		void get_download_queue(std::vector<partial_piece_info>& queue) const;

		// flags for set_piece_deadline().
		enum deadline_flags { alert_when_available = 1 };

		// This function sets or resets the deadline associated with a specific
		// piece index (``index``). libtorrent will attempt to download this
		// entire piece before the deadline expires. This is not necessarily
		// possible, but pieces with a more recent deadline will always be
		// prioritized over pieces with a deadline further ahead in time. The
		// deadline (and flags) of a piece can be changed by calling this
		// function again.
		// 
		// The ``flags`` parameter can be used to ask libtorrent to post an alert
		// once the piece has been downloaded, by passing alert_when_available.
		// When set, the read_piece_alert alert will be delivered, with the piece
		// data, when it's downloaded.
		// 
		// If the piece is already downloaded when this call is made, nothing
		// happens, unless the alert_when_available flag is set, in which case it
		// will have the same effect as calling read_piece() for ``index``.
		// 
		// ``deadline`` is the number of milliseconds until this piece should be
		// completed.
		// 
		// ``reset_piece_deadline`` removes the deadline from the piece. If it
		// hasn't already been downloaded, it will no longer be considered a
		// priority.
		// 
		// ``clear_piece_deadlines()`` removes deadlines on all pieces in
		// the torrent. As if reset_piece_deadline() was called on all pieces.
		void set_piece_deadline(int index, int deadline, int flags = 0) const;
		void reset_piece_deadline(int index) const;
		void clear_piece_deadlines() const;

		// This sets the bandwidth priority of this torrent. The priority of a
		// torrent determines how much bandwidth its peers are assigned when
		// distributing upload and download rate quotas. A high number gives more
		// bandwidth. The priority must be within the range [0, 255].
		// 
		// The default priority is 0, which is the lowest priority.
		// 
		// To query the priority of a torrent, use the
		// ``torrent_handle::status()`` call.
		// 
		// Torrents with higher priority will not necessarily get as much
		// bandwidth as they can consume, even if there's is more quota. Other
		// peers will still be weighed in when bandwidth is being distributed.
		// With other words, bandwidth is not distributed strictly in order of
		// priority, but the priority is used as a weight.
		// 
		// Peers whose Torrent has a higher priority will take precedence when
		// distributing unchoke slots. This is a strict prioritisation where
		// every interested peer on a high priority torrent will be unchoked
		// before any other, lower priority, torrents have any peers unchoked.
		void set_priority(int prio) const;
		
#ifndef TORRENT_NO_DEPRECATE
#if !TORRENT_NO_FPU
		// fills the specified vector with the download progress [0, 1]
		// of each file in the torrent. The files are ordered as in
		// the torrent_info.
		TORRENT_DEPRECATED
		void file_progress(std::vector<float>& progress) const;
#endif
#endif

		// flags to be passed in file_progress().
		enum file_progress_flags_t
		{
			// only calculate file progress at piece granularity. This makes
			// the file_progress() call cheaper and also only takes bytes that
			// have passed the hash check into account, so progress cannot
			// regress in this mode.
			piece_granularity = 1
		};

		// This function fills in the supplied vector with the the number of
		// bytes downloaded of each file in this torrent. The progress values are
		// ordered the same as the files in the torrent_info. This operation is
		// not very cheap. Its complexity is *O(n + mj)*. Where *n* is the number
		// of files, *m* is the number of downloading pieces and *j* is the
		// number of blocks in a piece.
		// 
		// The ``flags`` parameter can be used to specify the granularity of the
		// file progress. If left at the default value of 0, the progress will be
		// as accurate as possible, but also more expensive to calculate. If
		// ``torrent_handle::piece_granularity`` is specified, the progress will
		// be specified in piece granularity. i.e. only pieces that have been
		// fully downloaded and passed the hash check count. When specifying
		// piece granularity, the operation is a lot cheaper, since libtorrent
		// already keeps track of this internally and no calculation is required.
		void file_progress(std::vector<boost::int64_t>& progress, int flags = 0) const;

		// This function fills in the passed in vector with status about files
		// that are open for this torrent. Any file that is not open in this
		// torrent, will not be reported in the vector, i.e. it's possible that
		// the vector is empty when returning, if none of the files in the
		// torrent are currently open.
		//
		// see pool_file_status.
		void file_status(std::vector<pool_file_status>& status) const;

		// If the torrent is in an error state (i.e. ``torrent_status::error`` is
		// non-empty), this will clear the error and start the torrent again.
		void clear_error() const;

		// ``trackers()`` will return the list of trackers for this torrent. The
		// announce entry contains both a string ``url`` which specify the
		// announce url for the tracker as well as an int ``tier``, which is
		// specifies the order in which this tracker is tried. If you want
		// libtorrent to use another list of trackers for this torrent, you can
		// use ``replace_trackers()`` which takes a list of the same form as the
		// one returned from ``trackers()`` and will replace it. If you want an
		// immediate effect, you have to call force_reannounce(). See
		// announce_entry.
		// 
		// ``add_tracker()`` will look if the specified tracker is already in the
		// set. If it is, it doesn't do anything. If it's not in the current set
		// of trackers, it will insert it in the tier specified in the
		// announce_entry.
		// 
		// The updated set of trackers will be saved in the resume data, and when
		// a torrent is started with resume data, the trackers from the resume
		// data will replace the original ones.
		std::vector<announce_entry> trackers() const;
		void replace_trackers(std::vector<announce_entry> const&) const;
		void add_tracker(announce_entry const&) const;

		// TODO: 3 unify url_seed and http_seed with just web_seed, using the
		// web_seed_entry.

		// ``add_url_seed()`` adds another url to the torrent's list of url
		// seeds. If the given url already exists in that list, the call has no
		// effect. The torrent will connect to the server and try to download
		// pieces from it, unless it's paused, queued, checking or seeding.
		// ``remove_url_seed()`` removes the given url if it exists already.
		// ``url_seeds()`` return a set of the url seeds currently in this
		// torrent. Note that URLs that fails may be removed automatically from
		// the list.
		// 
		// See http-seeding_ for more information.
		void add_url_seed(std::string const& url) const;
		void remove_url_seed(std::string const& url) const;
		std::set<std::string> url_seeds() const;

		// These functions are identical as the ``*_url_seed()`` variants, but
		// they operate on `BEP 17`_ web seeds instead of `BEP 19`_.
		// 
		// See http-seeding_ for more information.
		void add_http_seed(std::string const& url) const;
		void remove_http_seed(std::string const& url) const;
		std::set<std::string> http_seeds() const;

		// add the specified extension to this torrent. The ``ext`` argument is
		// a function that will be called from within libtorrent's context
		// passing in the internal torrent object and the specified userdata
		// pointer. The function is expected to return a shared pointer to
		// a torrent_plugin instance.
		void add_extension(
			boost::function<boost::shared_ptr<torrent_plugin>(torrent_handle const&, void*)> const& ext
			, void* userdata = 0);

		// ``set_metadata`` expects the *info* section of metadata. i.e. The
		// buffer passed in will be hashed and verified against the info-hash. If
		// it fails, a ``metadata_failed_alert`` will be generated. If it passes,
		// a ``metadata_received_alert`` is generated. The function returns true
		// if the metadata is successfully set on the torrent, and false
		// otherwise. If the torrent already has metadata, this function will not
		// affect the torrent, and false will be returned.
		bool set_metadata(char const* metadata, int size) const;

		// Returns true if this handle refers to a valid torrent and false if it
		// hasn't been initialized or if the torrent it refers to has been
		// aborted. Note that a handle may become invalid after it has been added
		// to the session. Usually this is because the storage for the torrent is
		// somehow invalid or if the filenames are not allowed (and hence cannot
		// be opened/created) on your filesystem. If such an error occurs, a
		// file_error_alert is generated and all handles that refers to that
		// torrent will become invalid.
		bool is_valid() const;

		// flags for torrent_session::pause()
		enum pause_flags_t { graceful_pause = 1 };

		// ``pause()``, and ``resume()`` will disconnect all peers and reconnect
		// all peers respectively. When a torrent is paused, it will however
		// remember all share ratios to all peers and remember all potential (not
		// connected) peers. Torrents may be paused automatically if there is a
		// file error (e.g. disk full) or something similar. See
		// file_error_alert.
		// 
		// To know if a torrent is paused or not, call
		// ``torrent_handle::status()`` and inspect ``torrent_status::paused``.
		// 
		// The ``flags`` argument to pause can be set to
		// ``torrent_handle::graceful_pause`` which will delay the disconnect of
		// peers that we're still downloading outstanding requests from. The
		// torrent will not accept any more requests and will disconnect all idle
		// peers. As soon as a peer is done transferring the blocks that were
		// requested from it, it is disconnected. This is a graceful shut down of
		// the torrent in the sense that no downloaded bytes are wasted.
		// 
		// .. note::
		// 	Torrents that are auto-managed may be automatically resumed again. It
		// 	does not make sense to pause an auto-managed torrent without making it
		// 	not auto-managed first. Torrents are auto-managed by default when added
		// 	to the session. For more information, see queuing_.
		// 
		void pause(int flags = 0) const;
		void resume() const;

		// set or clear the stop-when-ready flag. When this flag is set, the
		// torrent will *force stop* whenever it transitions from a
		// non-data-transferring state into a data-transferring state (referred to
		// as being ready to download or seed). This is useful for torrents that
		// should not start downloading or seeding yet, but want to be made ready
		// to do so. A torrent may need to have its files checked for instance, so
		// it needs to be started and possibly queued for checking (auto-managed
		// and started) but as soon as it's done, it should be stopped.
		// 
		// *Force stopped* means auto-managed is set to false and it's paused. As
		// if auto_manage(false) and pause() were called on the torrent.
		// 
		// Note that the torrent may transition into a downloading state while
		// calling this function, and since the logic is edge triggered you may
		// miss the edge. To avoid this race, if the torrent already is in a
		// downloading state when this call is made, it will trigger the
		// stop-when-ready immediately.
		// 
		// When the stop-when-ready logic fires, the flag is cleared. Any
		// subsequent transitions between downloading and non-downloading states
		// will not be affected, until this function is used to set it again.
		// 
		// The behavior is more robust when setting this flag as part of adding
		// the torrent. See add_torrent_params.
		// 
		// The stop-when-ready flag fixes the inherent race condition of waiting
		// for the state_changed_alert and then call pause(). The download/seeding
		// will most likely start in between posting the alert and receiving the
		// call to pause.
		void stop_when_ready(bool b) const;

		// Explicitly sets the upload mode of the torrent. In upload mode, the
		// torrent will not request any pieces. If the torrent is auto managed,
		// it will automatically be taken out of upload mode periodically (see
		// ``settings_pack::optimistic_disk_retry``). Torrents are
		// automatically put in upload mode whenever they encounter a disk write
		// error.
		// 
		// ``m`` should be true to enter upload mode, and false to leave it.
		// 
		// To test if a torrent is in upload mode, call
		// ``torrent_handle::status()`` and inspect
		// ``torrent_status::upload_mode``.
		void set_upload_mode(bool b) const;

		// Enable or disable share mode for this torrent. When in share mode, the
		// torrent will not necessarily be downloaded, especially not the whole
		// of it. Only parts that are likely to be distributed to more than 2
		// other peers are downloaded, and only if the previous prediction was
		// correct.
		void set_share_mode(bool b) const;

		// Instructs libtorrent to flush all the disk caches for this torrent and
		// close all file handles. This is done asynchronously and you will be
		// notified that it's complete through cache_flushed_alert.
		// 
		// Note that by the time you get the alert, libtorrent may have cached
		// more data for the torrent, but you are guaranteed that whatever cached
		// data libtorrent had by the time you called
		// ``torrent_handle::flush_cache()`` has been written to disk.
		void flush_cache() const;

		// Set to true to apply the session global IP filter to this torrent
		// (which is the default). Set to false to make this torrent ignore the
		// IP filter.
		void apply_ip_filter(bool b) const;

		// ``force_recheck`` puts the torrent back in a state where it assumes to
		// have no resume data. All peers will be disconnected and the torrent
		// will stop announcing to the tracker. The torrent will be added to the
		// checking queue, and will be checked (all the files will be read and
		// compared to the piece hashes). Once the check is complete, the torrent
		// will start connecting to peers again, as normal.
		void force_recheck() const;

		// flags used in the save_resume_data call to control additional
		// actions or fields to save.
		enum save_resume_flags_t
		{
			// the disk cache will be flushed before creating the resume data.
			// This avoids a problem with file timestamps in the resume data in
			// case the cache hasn't been flushed yet.
			flush_disk_cache = 1,

			// the resume data will contain the metadata from the torrent file as
			// well. This is default for any torrent that's added without a
			// torrent file (such as a magnet link or a URL).
			save_info_dict = 2,

			// if nothing significant has changed in the torrent since the last
			// time resume data was saved, fail this attempt. Significant changes
			// primarily include more data having been downloaded, file or piece
			// priorities having changed etc. If the resume data doesn't need
			// saving, a save_resume_data_failed_alert is posted with the error
			// resume_data_not_modified.
			only_if_modified = 4
		};

		// ``save_resume_data()`` asks libtorrent to generate fast-resume data for
		// this torrent.
		// 
		// The ``flags`` argument is a bitmask of flags ORed together. see
		// save_resume_flags_t
		// 
		// This operation is asynchronous, ``save_resume_data`` will return
		// immediately. The resume data is delivered when it's done through an
		// save_resume_data_alert.
		// 
		// The fast resume data will be empty in the following cases:
		// 
		//	1. The torrent handle is invalid.
		//	2. The torrent hasn't received valid metadata and was started without
		//	   metadata (see libtorrent's metadata-from-peers_ extension)
		// 
		// Note that by the time you receive the fast resume data, it may already
		// be invalid if the torrent is still downloading! The recommended
		// practice is to first pause the session, then generate the fast resume
		// data, and then close it down. Make sure to not remove_torrent() before
		// you receive the save_resume_data_alert though. There's no need to
		// pause when saving intermittent resume data.
		// 
		//.. warning::
		//   If you pause every torrent individually instead of pausing the
		//   session, every torrent will have its paused state saved in the
		//   resume data!
		// 
		//.. warning::
		//   The resume data contains the modification timestamps for all files.
		//   If one file has been modified when the torrent is added again, the
		//   will be rechecked. When shutting down, make sure to flush the disk
		//   cache before saving the resume data. This will make sure that the
		//   file timestamps are up to date and won't be modified after saving
		//   the resume data. The recommended way to do this is to pause the
		//   torrent, which will flush the cache and disconnect all peers.
		// 
		//.. note::
		//   It is typically a good idea to save resume data whenever a torrent
		//   is completed or paused. In those cases you don't need to pause the
		//   torrent or the session, since the torrent will do no more writing to
		//   its files. If you save resume data for torrents when they are
		//   paused, you can accelerate the shutdown process by not saving resume
		//   data again for paused torrents. Completed torrents should have their
		//   resume data saved when they complete and on exit, since their
		//   statistics might be updated.
		// 
		//	In full allocation mode the resume data is never invalidated by
		//	subsequent writes to the files, since pieces won't move around. This
		//	means that you don't need to pause before writing resume data in full
		//	or sparse mode. If you don't, however, any data written to disk after
		//	you saved resume data and before the session closed is lost.
		// 
		// It also means that if the resume data is out dated, libtorrent will
		// not re-check the files, but assume that it is fairly recent. The
		// assumption is that it's better to loose a little bit than to re-check
		// the entire file.
		// 
		// It is still a good idea to save resume data periodically during
		// download as well as when closing down.
		// 
		// Example code to pause and save resume data for all torrents and wait
		// for the alerts:
		// 
		// .. code:: c++
		// 
		//	extern int outstanding_resume_data; // global counter of outstanding resume data
		//	std::vector<torrent_handle> handles = ses.get_torrents();
		//	ses.pause();
		//	for (torrent_handle i : handles)
		//	{
		//		torrent_handle& h = *i;
		//		if (!h.is_valid()) continue;
		//		torrent_status s = h.status();
		//		if (!s.has_metadata) continue;
		//		if (!s.need_save_resume_data()) continue;
		//
		//		h.save_resume_data();
		//		++outstanding_resume_data;
		//	}
		//
		//	while (outstanding_resume_data > 0)
		//	{
		//		alert const* a = ses.wait_for_alert(seconds(10));
		//
		//		// if we don't get an alert within 10 seconds, abort
		//		if (a == 0) break;
		//		
		//		std::vector<alert*> alerts;
		//		ses.pop_alerts(&alerts);
		//
		//		for (alert* i : alerts)
		//		{
		//			if (alert_cast<save_resume_data_failed_alert>(a))
		//			{
		//				process_alert(a);
		//				--outstanding_resume_data;
		//				continue;
		//			}
		//
		//			save_resume_data_alert const* rd = alert_cast<save_resume_data_alert>(a);
		//			if (rd == 0)
		//			{
		//				process_alert(a);
		//				continue;
		//			}
		//		
		//			torrent_handle h = rd->handle;
		//			torrent_status st = h.status(torrent_handle::query_save_path
		//				| torrent_handle::query_name);
		//			std::ofstream out((st.save_path
		//				+ "/" + st.name + ".fastresume").c_str()
		//				, std::ios_base::binary);
		//			out.unsetf(std::ios_base::skipws);
		//			bencode(std::ostream_iterator<char>(out), *rd->resume_data);
		//			--outstanding_resume_data;
		//		}
		//	}
		// 
		//.. note::
		//	Note how ``outstanding_resume_data`` is a global counter in this
		//	example. This is deliberate, otherwise there is a race condition for
		//	torrents that was just asked to save their resume data, they posted
		//	the alert, but it has not been received yet. Those torrents would
		//	report that they don't need to save resume data again, and skipped by
		//	the initial loop, and thwart the counter otherwise.
		void save_resume_data(int flags = 0) const;

		// This function returns true if any whole chunk has been downloaded
		// since the torrent was first loaded or since the last time the resume
		// data was saved. When saving resume data periodically, it makes sense
		// to skip any torrent which hasn't downloaded anything since the last
		// time.
		//
		//.. note::
		//	A torrent's resume data is considered saved as soon as the alert is
		//	posted. It is important to make sure this alert is received and
		//	handled in order for this function to be meaningful.
		bool need_save_resume_data() const;

		// changes whether the torrent is auto managed or not. For more info,
		// see queuing_.
		void auto_managed(bool m) const;

		// Every torrent that is added is assigned a queue position exactly one
		// greater than the greatest queue position of all existing torrents.
		// Torrents that are being seeded have -1 as their queue position, since
		// they're no longer in line to be downloaded.
		// 
		// When a torrent is removed or turns into a seed, all torrents with
		// greater queue positions have their positions decreased to fill in the
		// space in the sequence.
		// 
		// ``queue_position()`` returns the torrent's position in the download
		// queue. The torrents with the smallest numbers are the ones that are
		// being downloaded. The smaller number, the closer the torrent is to the
		// front of the line to be started.
		// 
		// The queue position is also available in the torrent_status.
		// 
		// The ``queue_position_*()`` functions adjust the torrents position in
		// the queue. Up means closer to the front and down means closer to the
		// back of the queue. Top and bottom refers to the front and the back of
		// the queue respectively.
		int queue_position() const;
		void queue_position_up() const;
		void queue_position_down() const;
		void queue_position_top() const;
		void queue_position_bottom() const;

		// updates the position in the queue for this torrent. The relative order
		// of all other torrents remain intact but their numerical queue position
		// shifts to make space for this torrent's new position
		void queue_position_set(int p) const;

#ifndef TORRENT_NO_DEPRECATE
		// deprecated in 1.1

		// Sets or gets the flag that determines if countries should be resolved
		// for the peers of this torrent. It defaults to false. If it is set to
		// true, the peer_info structure for the peers in this torrent will have
		// their ``country`` member set. See peer_info for more information on
		// how to interpret this field.
		TORRENT_DEPRECATED
		void resolve_countries(bool r);
		TORRENT_DEPRECATED
		bool resolve_countries() const;
#endif

		// For SSL torrents, use this to specify a path to a .pem file to use as
		// this client's certificate. The certificate must be signed by the
		// certificate in the .torrent file to be valid.
		//
		// The set_ssl_certificate_buffer() overload takes the actual certificate,
		// private key and DH params as strings, rather than paths to files. This
		// overload is only available when libtorrent is built against boost
		// 1.54 or later.
		// 
		// ``cert`` is a path to the (signed) certificate in .pem format
		// corresponding to this torrent.
		// 
		// ``private_key`` is a path to the private key for the specified
		// certificate. This must be in .pem format.
		// 
		// ``dh_params`` is a path to the Diffie-Hellman parameter file, which
		// needs to be in .pem format. You can generate this file using the
		// openssl command like this: ``openssl dhparam -outform PEM -out
		// dhparams.pem 512``.
		// 
		// ``passphrase`` may be specified if the private key is encrypted and
		// requires a passphrase to be decrypted.
		// 
		// Note that when a torrent first starts up, and it needs a certificate,
		// it will suspend connecting to any peers until it has one. It's
		// typically desirable to resume the torrent after setting the SSL
		// certificate.
		// 
		// If you receive a torrent_need_cert_alert, you need to call this to
		// provide a valid cert. If you don't have a cert you won't be allowed to
		// connect to any peers.
		void set_ssl_certificate(std::string const& certificate
			, std::string const& private_key
			, std::string const& dh_params
			, std::string const& passphrase = "");
		void set_ssl_certificate_buffer(std::string const& certificate
			, std::string const& private_key
			, std::string const& dh_params);

		// Returns the storage implementation for this torrent. This depends on the
		// storage constructor function that was passed to add_torrent.
		storage_interface* get_storage_impl() const;

		// Returns a pointer to the torrent_info object associated with this
		// torrent. The torrent_info object may be a copy of the internal object.
		// If the torrent doesn't have metadata, the pointer will not be
		// initialized (i.e. a NULL pointer). The torrent may be in a state
		// without metadata only if it was started without a .torrent file, e.g.
		// by using the libtorrent extension of just supplying a tracker and
		// info-hash.
		shared_ptr<const torrent_info> torrent_file() const;

#ifndef TORRENT_NO_DEPRECATE

		// ================ start deprecation ============

		// deprecated in 1.0
		// use status() instead (with query_save_path)
		TORRENT_DEPRECATED
		std::string save_path() const;

		// deprecated in 1.0
		// use status() instead (with query_name)
		// returns the name of this torrent, in case it doesn't
		// have metadata it returns the name assigned to it
		// when it was added.
		TORRENT_DEPRECATED
		std::string name() const;

		// use torrent_file() instead
		TORRENT_DEPRECATED
		const torrent_info& get_torrent_info() const;

		// deprecated in 0.16, feature will be removed
		TORRENT_DEPRECATED
		int get_peer_upload_limit(tcp::endpoint ip) const;
		TORRENT_DEPRECATED
		int get_peer_download_limit(tcp::endpoint ip) const;
		TORRENT_DEPRECATED
		void set_peer_upload_limit(tcp::endpoint ip, int limit) const;
		TORRENT_DEPRECATED
		void set_peer_download_limit(tcp::endpoint ip, int limit) const;

		// deprecated in 0.16, feature will be removed
		TORRENT_DEPRECATED
		void set_ratio(float up_down_ratio) const;

		// deprecated in 0.16. use status() instead
		TORRENT_DEPRECATED
		bool is_seed() const;
		TORRENT_DEPRECATED
		bool is_finished() const;
		TORRENT_DEPRECATED
		bool is_paused() const;
		TORRENT_DEPRECATED
		bool is_auto_managed() const;
		TORRENT_DEPRECATED
		bool is_sequential_download() const;
		TORRENT_DEPRECATED
		bool has_metadata() const;
		TORRENT_DEPRECATED
		bool super_seeding() const;

		// deprecated in 0.13
		// all these are deprecated, use piece
		// priority functions instead
		// marks the piece with the given index as filtered
		// it will not be downloaded
		TORRENT_DEPRECATED
		void filter_piece(int index, bool filter) const;
		TORRENT_DEPRECATED
		void filter_pieces(std::vector<bool> const& pieces) const;
		TORRENT_DEPRECATED
		bool is_piece_filtered(int index) const;
		TORRENT_DEPRECATED
		std::vector<bool> filtered_pieces() const;
		// marks the file with the given index as filtered
		// it will not be downloaded
		TORRENT_DEPRECATED
		void filter_files(std::vector<bool> const& files) const;

		// deprecated in 0.14
		// use save_resume_data() instead. It is async. and
		// will return the resume data in an alert
		TORRENT_DEPRECATED
		entry write_resume_data() const;

		// ``use_interface()`` sets the network interface this torrent will use
		// when it opens outgoing connections. By default, it uses the same
		// interface as the session uses to listen on. The parameter must be a
		// string containing one or more, comma separated, ip-address (either an
		// IPv4 or IPv6 address). When specifying multiple interfaces, the
		// torrent will round-robin which interface to use for each outgoing
		// connection. This is useful for clients that are multi-homed.
		TORRENT_DEPRECATED
		void use_interface(const char* net_interface) const;
		// ================ end deprecation ============
#endif

		// Fills the specified ``std::vector<int>`` with the availability for
		// each piece in this torrent. libtorrent does not keep track of
		// availability for seeds, so if the torrent is seeding the availability
		// for all pieces is reported as 0.
		// 
		// The piece availability is the number of peers that we are connected
		// that has advertised having a particular piece. This is the information
		// that libtorrent uses in order to prefer picking rare pieces.
		void piece_availability(std::vector<int>& avail) const;

		// These functions are used to set and get the priority of individual
		// pieces. By default all pieces have priority 4. That means that the
		// random rarest first algorithm is effectively active for all pieces.
		// You may however change the priority of individual pieces. There are 8
		// priority levels. 0 means not to download the piece at all. Otherwise,
		// lower priority values means less likely to be picked. Piece priority
		// takes precedence over piece availability. Every piece with priority 7
		// will be attempted to be picked before a priority 6 piece and so on.
		// 
		// The default priority of pieces is 4.
		// 
		// Piece priorities can not be changed for torrents that have not
		// downloaded the metadata yet. For instance, magnet links and torrents
		// added by URL won't have metadata immediately. see the
		// metadata_received_alert.
		// 
		// ``piece_priority`` sets or gets the priority for an individual piece,
		// specified by ``index``.
		// 
		// ``prioritize_pieces`` takes a vector of integers, one integer per
		// piece in the torrent. All the piece priorities will be updated with
		// the priorities in the vector.
		// The second overload of ``prioritize_pieces`` that takes a vector of pairs
		// will update the priorities of only select pieces, and leave all other
		// unaffected. Each pair is (piece, priority). That is, the first item is
		// the piece index and the second item is the priority of that piece.
		// Invalid entries, where the piece index or priority is out of range, are
		// not allowed.
		// 
		// ``piece_priorities`` returns a vector with one element for each piece
		// in the torrent. Each element is the current priority of that piece.
		void piece_priority(int index, int priority) const;
		int piece_priority(int index) const;
		void prioritize_pieces(std::vector<int> const& pieces) const;
		void prioritize_pieces(std::vector<std::pair<int, int> > const& pieces) const;
		std::vector<int> piece_priorities() const;

		// ``index`` must be in the range [0, number_of_files).
		// 
		// ``file_priority()`` queries or sets the priority of file ``index``.
		// 
		// ``prioritize_files()`` takes a vector that has at as many elements as
		// there are files in the torrent. Each entry is the priority of that
		// file. The function sets the priorities of all the pieces in the
		// torrent based on the vector.
		// 
		// ``file_priorities()`` returns a vector with the priorities of all
		// files.
		// 
		// The priority values are the same as for piece_priority().
		// 
		// Whenever a file priority is changed, all other piece priorities are
		// reset to match the file priorities. In order to maintain special
		// priorities for particular pieces, piece_priority() has to be called
		// again for those pieces.
		// 
		// You cannot set the file priorities on a torrent that does not yet have
		// metadata or a torrent that is a seed. ``file_priority(int, int)`` and
		// prioritize_files() are both no-ops for such torrents.
		void file_priority(int index, int priority) const;
		int file_priority(int index) const;
		void prioritize_files(std::vector<int> const& files) const;
		std::vector<int> file_priorities() const;

		// ``force_reannounce()`` will force this torrent to do another tracker
		// request, to receive new peers. The ``seconds`` argument specifies how
		// many seconds from now to issue the tracker announces.
		// 
		// If the tracker's ``min_interval`` has not passed since the last
		// announce, the forced announce will be scheduled to happen immediately
		// as the ``min_interval`` expires. This is to honor trackers minimum
		// re-announce interval settings.
		//
		// The ``tracker_index`` argument specifies which tracker to re-announce.
		// If set to -1 (which is the default), all trackers are re-announce.
		// 
		// ``force_dht_announce`` will announce the torrent to the DHT
		// immediately.
		void force_reannounce(int seconds = 0, int tracker_index = -1) const;
		void force_dht_announce() const;

#ifndef TORRENT_NO_DEPRECATE
		// forces a reannounce in the specified amount of time.
		// This overrides the default announce interval, and no
		// announce will take place until the given time has
		// timed out.
		TORRENT_DEPRECATED
		void force_reannounce(boost::posix_time::time_duration) const;
#endif

		// ``scrape_tracker()`` will send a scrape request to a tracker. By
		// default (``idx`` = -1) it will scrape the last working tracker. If
		// ``idx`` is >= 0, the tracker with the specified index will scraped.
		// 
		// A scrape request queries the tracker for statistics such as total
		// number of incomplete peers, complete peers, number of downloads etc.
		// 
		// This request will specifically update the ``num_complete`` and
		// ``num_incomplete`` fields in the torrent_status struct once it
		// completes. When it completes, it will generate a scrape_reply_alert.
		// If it fails, it will generate a scrape_failed_alert.
		void scrape_tracker(int idx = -1) const;

		// ``set_upload_limit`` will limit the upload bandwidth used by this
		// particular torrent to the limit you set. It is given as the number of
		// bytes per second the torrent is allowed to upload.
		// ``set_download_limit`` works the same way but for download bandwidth
		// instead of upload bandwidth. Note that setting a higher limit on a
		// torrent then the global limit
		// (``settings_pack::upload_rate_limit``) will not override the global
		// rate limit. The torrent can never upload more than the global rate
		// limit.
		// 
		// ``upload_limit`` and ``download_limit`` will return the current limit
		// setting, for upload and download, respectively.
		// 
		// Local peers are not rate limited by default. see peer-classes_.
		void set_upload_limit(int limit) const;
		int upload_limit() const;
		void set_download_limit(int limit) const;
		int download_limit() const;

		// A pinned torrent may not be unloaded by libtorrent. When the dynamic
		// loading and unloading of torrents is enabled (by setting a load
		// function on the session), this can be used to exempt certain torrents
		// from the unloading logic.
		// 
		// Magnet links, and other torrents that start out without having
		// metadata are pinned automatically. This is to give the client a chance
		// to get the metadata and save it before it's unloaded. In this case, it
		// may be useful to unpin the torrent once its metadata has been saved
		// to disk.
		// 
		// For more information about dynamically loading and unloading torrents,
		// see dynamic-loading-of-torrent-files_.
		// 
		void set_pinned(bool p) const;

		// ``set_sequential_download()`` enables or disables *sequential
		// download*. When enabled, the piece picker will pick pieces in sequence
		// instead of rarest first. In this mode, piece priorities are ignored,
		// with the exception of priority 7, which are still preferred over the
		// sequential piece order.
		// 
		// Enabling sequential download will affect the piece distribution
		// negatively in the swarm. It should be used sparingly.
		void set_sequential_download(bool sd) const;

		// ``connect_peer()`` is a way to manually connect to peers that one
		// believe is a part of the torrent. If the peer does not respond, or is
		// not a member of this torrent, it will simply be disconnected. No harm
		// can be done by using this other than an unnecessary connection attempt
		// is made. If the torrent is uninitialized or in queued or checking
		// mode, this will throw libtorrent_exception. The second (optional)
		// argument will be bitwise ORed into the source mask of this peer.
		// Typically this is one of the source flags in peer_info. i.e.
		// ``tracker``, ``pex``, ``dht`` etc.
		//
		// ``flags`` are the same flags that are passed along with the ``ut_pex`` extension.
		//
		// ==== ==========================================
		// 0x01 peer supports encryption.
		// 
		// 0x02 peer is a seed
		// 
		// 0x04 supports uTP. If this is not set, the peer will only be contacted
		//      over TCP.
		// 
		// 0x08 supports hole punching protocol. If this
		//      flag is received from a peer, it can be
		//      used as a rendezvous point in case direct
		//      connections to the peer fail
		// ==== ==========================================
		void connect_peer(tcp::endpoint const& adr, int source = 0
			, int flags = 0x1 + 0x4 + 0x8) const;

		// ``set_max_uploads()`` sets the maximum number of peers that's unchoked
		// at the same time on this torrent. If you set this to -1, there will be
		// no limit. This defaults to infinite. The primary setting controlling
		// this is the global unchoke slots limit, set by unchoke_slots_limit in
		// settings_pack.
		// 
		// ``max_uploads()`` returns the current settings.
		void set_max_uploads(int max_uploads) const;
		int max_uploads() const;

		// ``set_max_connections()`` sets the maximum number of connection this
		// torrent will open. If all connections are used up, incoming
		// connections may be refused or poor connections may be closed. This
		// must be at least 2. The default is unlimited number of connections. If
		// -1 is given to the function, it means unlimited. There is also a
		// global limit of the number of connections, set by
		// ``connections_limit`` in settings_pack.
		// 
		// ``max_connections()`` returns the current settings.
		void set_max_connections(int max_connections) const;
		int max_connections() const;

#ifndef TORRENT_NO_DEPRECATE
		// sets a username and password that will be sent along in the HTTP-request
		// of the tracker announce. Set this if the tracker requires authorization.
		TORRENT_DEPRECATED
		void set_tracker_login(std::string const& name
			, std::string const& password) const;
#endif

		// Moves the file(s) that this torrent are currently seeding from or
		// downloading to. If the given ``save_path`` is not located on the same
		// drive as the original save path, the files will be copied to the new
		// drive and removed from their original location. This will block all
		// other disk IO, and other torrents download and upload rates may drop
		// while copying the file.
		// 
		// Since disk IO is performed in a separate thread, this operation is
		// also asynchronous. Once the operation completes, the
		// ``storage_moved_alert`` is generated, with the new path as the
		// message. If the move fails for some reason,
		// ``storage_moved_failed_alert`` is generated instead, containing the
		// error message.
		// 
		// The ``flags`` argument determines the behavior of the copying/moving
		// of the files in the torrent. see move_flags_t.
		// 
		// 	* always_replace_files = 0
		// 	* fail_if_exist = 1
		// 	* dont_replace = 2
		// 
		// ``always_replace_files`` is the default and replaces any file that
		// exist in both the source directory and the target directory.
		// 
		// ``fail_if_exist`` first check to see that none of the copy operations
		// would cause an overwrite. If it would, it will fail. Otherwise it will
		// proceed as if it was in ``always_replace_files`` mode. Note that there
		// is an inherent race condition here. If the files in the target
		// directory appear after the check but before the copy or move
		// completes, they will be overwritten. When failing because of files
		// already existing in the target path, the ``error`` of
		// ``move_storage_failed_alert`` is set to
		// ``boost::system::errc::file_exists``.
		// 
		// The intention is that a client may use this as a probe, and if it
		// fails, ask the user which mode to use. The client may then re-issue
		// the ``move_storage`` call with one of the other modes.
		// 
		// ``dont_replace`` always takes the existing file in the target
		// directory, if there is one. The source files will still be removed in
		// that case.
		// 
		// Files that have been renamed to have absolute paths are not moved by
		// this function. Keep in mind that files that don't belong to the
		// torrent but are stored in the torrent's directory may be moved as
		// well. This goes for files that have been renamed to absolute paths
		// that still end up inside the save path.
		void move_storage(std::string const& save_path, int flags = 0) const;

		// Renames the file with the given index asynchronously. The rename
		// operation is complete when either a file_renamed_alert or
		// file_rename_failed_alert is posted.
		void rename_file(int index, std::string const& new_name) const;

#ifndef TORRENT_NO_DEPRECATE
#if TORRENT_USE_WSTRING
		// all wstring APIs are deprecated since 0.16.11
		// instead, use the wchar -> utf8 conversion functions
		// and pass in utf8 strings
		TORRENT_DEPRECATED
		void move_storage(std::wstring const& save_path, int flags = 0) const;
		TORRENT_DEPRECATED
		void rename_file(int index, std::wstring const& new_name) const;
#endif // TORRENT_USE_WSTRING
#endif // TORRENT_NO_DEPRECATE

		// Enables or disabled super seeding/initial seeding for this torrent.
		// The torrent needs to be a seed for this to take effect.
		void super_seeding(bool on) const;

		// ``info_hash()`` returns the info-hash of the torrent. If this handle
		// is to a torrent that hasn't loaded yet (for instance by being added)
		// by a URL, the returned value is undefined.
		sha1_hash info_hash() const;

		// comparison operators. The order of the torrents is unspecified
		// but stable.
		bool operator==(const torrent_handle& h) const
		{ return m_torrent.lock() == h.m_torrent.lock(); }
		bool operator!=(const torrent_handle& h) const
		{ return m_torrent.lock() != h.m_torrent.lock(); }
		bool operator<(const torrent_handle& h) const
		{ return m_torrent.lock() < h.m_torrent.lock(); }

		boost::uint32_t id() const
		{
			uintptr_t ret = reinterpret_cast<uintptr_t>(m_torrent.lock().get());
			// a torrent object is about 1024 bytes, so
			// it's safe to shift 11 bits
			return boost::uint32_t(ret >> 11);
		}

		// This function is intended only for use by plugins and the alert
		// dispatch function. This type does not have a stable API and should
		// be relied on as little as possible.
		boost::shared_ptr<torrent> native_handle() const;

	private:

		torrent_handle(boost::weak_ptr<torrent> const& t)
		{ if (!t.expired()) m_torrent = t; }

		boost::weak_ptr<torrent> m_torrent;

	};

}

#endif // TORRENT_TORRENT_HANDLE_HPP_INCLUDED

