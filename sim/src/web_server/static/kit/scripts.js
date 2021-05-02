/* If you add or edit page / lister / tabmenu / chooser or whatever make sure
   that:
   - centerize_modal() is put everywhere where needed
   - timed_hide_show() is also put everywhere where appropriate (to check you
       could change the timed_hide_delay to something big and see if everything
       show as soon as it is loaded)
   - all tabs (from tab-menus) and deeper / subsequent tabs works well with page
       refreshing
*/
function elem_with_text(tag, text) {
	var elem = document.createElement(tag);
	elem.innerText = text;
	return elem;
}
function elem_with_class(tag, classes) {
	var elem = document.createElement(tag);
	elem.className = classes;
	return elem;
}
function elem_with_class_and_text(tag, classes, text) {
	var elem = document.createElement(tag);
	elem.innerText = text;
	elem.className = classes;
	return elem;
}
function append_children(elem, elements) {
	for (var i in elements)
		elem.appendChild(elements[i]);
}
function text_to_safe_html(str) { // This is deprecated because DOM elements have innerText property (see elem_with_text() function)
	var x = document.createElement('span');
	x.innerText = str;
	return x.innerHTML;
}
function is_logged_in() {
	return (document.querySelector('.navbar .user + ul > a:first-child') !== null);
}
function logged_user_id() {
	var x = document.querySelector('.navbar .user + ul > a:first-child').href;
	return x.substring(x.lastIndexOf('/') + 1);
}
function logged_user_is_admin() { // This is deprecated
	return (document.querySelector('.navbar .user[user-type="A"]') !== null);
}
function logged_user_is_teacher() { // This is deprecated
	return (document.querySelector('.navbar .user[user-type="T"]') !== null);
}
function logged_user_is_teacher_or_admin() { // This is deprecated
	return logged_user_is_teacher() || logged_user_is_admin();
}

// Viewport
function get_viewport_dimensions() {
	// document.documentElement.client(Height|Width) is not enough -- see: https://stackoverflow.com/a/37113430
	// window.inner(Height|Width) includes scrollbars, but they reduce viewport
	var viewport_prober = document.createElement('span');
	viewport_prober.style.visibility = 'hidden';
	viewport_prober.style.position = 'fixed';
	viewport_prober.style.top = '0';
	viewport_prober.style.bottom = '0';
	viewport_prober.style.left = '0';
	viewport_prober.style.right = '0';
	document.documentElement.appendChild(viewport_prober);
	var res = {
		height: viewport_prober.clientHeight,
		width: viewport_prober.clientWidth,
	};
	viewport_prober.remove();
	return res;
}
// Scroll: overflowed elements
function is_overflowed_elem_scrolled_down(elem) {
	var scroll_distance_to_bottom = elem.scrollHeight - elem.scrollTop - elem.clientHeight;
	return scroll_distance_to_bottom <= 1; // As of time of writing I managed to get value 1 in firefox with 80% zoom
}
function is_overflowed_elem_scrolled_up(elem) {
	return elem.scrollTop === 0;
}
// Scroll: relative to viewport
function how_much_is_viewport_top_above_elem_top(elem) {
	return elem.getBoundingClientRect().top;
}
function how_much_is_viewport_bottom_above_elem_bottom(elem) {
	return elem.getBoundingClientRect().bottom - get_viewport_dimensions().height;
}
function copy_to_clipboard(get_text_to_copy) {
	var elem = document.createElement('textarea');
	elem.value = get_text_to_copy();
	elem.setAttribute('readonly', '');
	elem.style.position = 'absolute';
	elem.style.left = '-9999px';
	document.body.appendChild(elem);
	elem.select();
	document.execCommand('copy');
	document.body.removeChild(elem);
}
// Calculates the actual server time
function server_time() {
	if (server_time.time_difference === undefined)
		server_time.time_difference = window.performance.timing.responseStart - start_time;

	var time = new Date();
	time.setTime(time.getTime() - server_time.time_difference);
	return time;
}
// Returns value of cookie @p name or empty string
function get_cookie(name) {
	name = name + '=';
	var arr = document.cookie.split(';');
	for (var i = 0; i < arr.length; ++i) {
		var j = 0;
		while (arr[i].charAt(j) == ' ')
			++j;
		if (arr[i].indexOf(name, j) == j)
			return arr[i].substring(j + name.length, arr[i].length);
	}

	return '';
}

function StaticMap() {
	var this_ = this;
	this.data = [];

	this.cmp = function(a, b) { return a[0] - b[0]; };

	this.add = function(key, val) {
		this_.data.push([key, val]);
	};

	this.prepare = function() { this_.data.sort(this_.cmp); };

	this.get = function(key) {
		if (this_.data.length === 0)
			return null;

		var l = 0, r = this_.data.length - 1;
		while (l < r) {
			var m = (l + r) >> 1;
			if (this_.data[m][0] < key)
				l = m + 1;
			else
				r = m;
		}

		return (this_.data[l][0] == key ? this_.data[l][1] : null);
	};
}

/**
 * @brief Converts @p size, so that it human readable
 * @details It adds proper suffixes, for example:
 *   1 -> "1 byte"
 *   1023 -> "1023 bytes"
 *   1024 -> "1.0 KiB"
 *   129747 -> "127 KiB"
 *   97379112 -> "92.9 MiB"
 *
 * @param size size to humanize
 *
 * @return humanized file size
 */
function humanize_file_size(size) {
	var MIN_KIB = 1024;
	var MIN_MIB = 1048576;
	var MIN_GIB = 1073741824;
	var MIN_TIB = 1099511627776;
	var MIN_PIB = 1125899906842624;
	var MIN_EIB = 1152921504606846976;
	var MIN_3DIGIT_KIB = 102349;
	var MIN_3DIGIT_MIB = 104805172;
	var MIN_3DIGIT_GIB = 107320495309;
	var MIN_3DIGIT_TIB = 109896187196212;
	var MIN_3DIGIT_PIB = 112533595688920269;

	// Bytes
	if (size < MIN_KIB)
		return (size == 1 ? "1 byte" : size + " bytes");

	// KiB
	if (size < MIN_3DIGIT_KIB)
		return parseFloat(size / MIN_KIB).toFixed(1) + " KiB";
	if (size < MIN_MIB)
		return Math.round(size / MIN_KIB) + " KiB";
	// MiB
	if (size < MIN_3DIGIT_MIB)
		return parseFloat(size / MIN_MIB).toFixed(1) + " MiB";
	if (size < MIN_GIB)
		return Math.round(size / MIN_MIB) + " MiB";
	// GiB
	if (size < MIN_3DIGIT_GIB)
		return parseFloat(size / MIN_GIB).toFixed(1) + " GiB";
	if (size < MIN_TIB)
		return Math.round(size / MIN_GIB) + " GiB";
	// TiB
	if (size < MIN_3DIGIT_TIB)
		return parseFloat(size / MIN_TIB).toFixed(1) + " TiB";
	if (size < MIN_PIB)
		return Math.round(size / MIN_TIB) + " TiB";
	// PiB
	if (size < MIN_3DIGIT_PIB)
		return parseFloat(size / MIN_PIB).toFixed(1) + " PiB";
	if (size < MIN_EIB)
		return Math.round(size / MIN_PIB) + " PiB";
	// EiB
	return parseFloat(size / MIN_EIB).toFixed(1) + " EiB";
}

/* ============================ URLs ============================ */
function url_enter_contest(contest_entry_token) {
	return '/enter_contest/' + contest_entry_token;
}
function url_api_contest_entry_tokens_view(contest_id) {
	return '/api/contest/' + contest_id + '/entry_tokens';
}
function url_api_contest_entry_tokens_add(contest_id) {
	return '/api/contest/' + contest_id + '/entry_tokens/add';
}
function url_api_contest_entry_tokens_regen(contest_id) {
	return '/api/contest/' + contest_id + '/entry_tokens/regen';
}
function url_api_contest_entry_tokens_delete(contest_id) {
	return '/api/contest/' + contest_id + '/entry_tokens/delete';
}
function url_api_contest_entry_tokens_add_short(contest_id) {
	return '/api/contest/' + contest_id + '/entry_tokens/add_short';
}
function url_api_contest_entry_tokens_regen_short(contest_id) {
	return '/api/contest/' + contest_id + '/entry_tokens/regen_short';
}
function url_api_contest_entry_tokens_delete_short(contest_id) {
	return '/api/contest/' + contest_id + '/entry_tokens/delete_short';
}
function url_api_contest_name_for_contest_entry_token(contest_entry_token) {
	return '/api/contest_entry_token/' + contest_entry_token + '/contest_name';
}
function url_api_use_contest_entry_token(contest_entry_token) {
	return '/api/contest_entry_token/' + contest_entry_token + '/use';
}
function url_api_user(user_id) {
	return '/api/user/' + user_id;
}
function url_api_users(query_suffix) {
	return '/api/users' + query_suffix;
}

/* ============================ URL hash parser ============================ */
var url_hash_parser = {};
(function () {
	var args = window.location.hash; // Must begin with '#'
	var beg = 0; // Points to the '#' just before the next argument

	url_hash_parser.next_arg  = function() {
		var pos = args.indexOf('#', beg + 1);
		if (pos === -1)
			return args.substring(beg + 1);

		return args.substring(beg + 1, pos);
	};

	url_hash_parser.extract_next_arg  = function() {
		var pos = args.indexOf('#', beg + 1), res;
		if (pos === -1) {
			if (beg >= args.length)
				return '';

			res = args.substring(beg + 1);
			beg = args.length;
			return res;
		}

		res = args.substring(beg + 1, pos);
		beg = pos;
		return res;
	};

	url_hash_parser.empty = function() { return (beg >= args.length); };

	url_hash_parser.assign = function(new_hash) {
		beg = 0;
		if (new_hash.charAt(0) !== '#')
			args = '#' + new_hash;
		else
			args = new_hash;
	};

	url_hash_parser.assign_as_parsed = function(new_hash) {
		if (new_hash.charAt(0) !== '#')
			args = '#' + new_hash;
		else
			args = new_hash;
		beg = args.length;
	};


	url_hash_parser.append = function(next_args) {
		if (next_args.charAt(0) !== '#')
			args += '#' + next_args;
		else
			args += next_args;
	};

	url_hash_parser.parsed_prefix = function() { return args.substring(0, beg); };
}).call();


///////////////////////////////// Below code requires jQuery /////////////////////////////////
function tz_marker() {
	var tzo = -(new Date()).getTimezoneOffset();
	return String().concat('<sup>UTC', (tzo >= 0 ? '+' : ''), tzo / 60,
		'</sup>');
}
function add_tz_marker(elem) {
	elem = $(elem);
	elem.children('sup').remove();
	elem.append(tz_marker);
}
function date_to_datetime_str(date, trim_zero_seconds /*= false*/) {
	var month = date.getMonth() + 1;
	var day = date.getDate();
	var hours = date.getHours();
	var minutes = date.getMinutes();
	var seconds = date.getSeconds();
	month = (month < 10 ? '0' : '') + month;
	day = (day < 10 ? '0' : '') + day;
	hours = (hours < 10 ? '0' : '') + hours;
	minutes = (minutes < 10 ? '0' : '') + minutes;

	if (trim_zero_seconds && seconds === 0)
		return String().concat(date.getFullYear(), '-', month, '-', day, ' ', hours, ':', minutes);

	seconds = (seconds < 10 ? '0' : '') + seconds;
	return String().concat(date.getFullYear(), '-', month, '-', day, ' ', hours, ':', minutes, ':', seconds);
}
function utcdt_or_tm_to_Date(time) {
	if (isNaN(time)) {
		// Convert infinities to infinities so they are comparable with other Dates
		if (time === '-inf')
			return -Infinity;
		if (time == '+inf')
			return Infinity;

		var args = time.split(/[- :]/);
		--args[1]; // fit month in [0, 11]
		return new Date(Date.UTC.apply(this, args));
	}

	return new Date(time * 1000);
}
// Converts datetimes to local
function normalize_datetime(elem, add_tz /*= false*/, trim_zero_seconds /*= false*/) {
	elem = $(elem);
	if (elem.attr('datetime') === undefined)
		return elem;

	// Add the timezone part
	elem.text(date_to_datetime_str(utcdt_or_tm_to_Date(elem.attr('datetime')), trim_zero_seconds));
	if (add_tz)
		elem.append(tz_marker());

	return elem;
}
function infdatetime_to(elem, infdt, neg_inf_text, inf_text) {
	if (infdt[0] == '-')
		return elem.text(neg_inf_text);
	else if (infdt[0] == '+')
		return elem.text(inf_text);
	else
		return normalize_datetime(elem.text(infdt)
			.attr('datetime', infdt));
}
// Produces a span that will update every second and show remaining time
function countdown_clock(target_date) {
	var span = $('<span>');
	var update_countdown = function() {
		var diff = target_date - server_time();
		if (!$.contains(document.documentElement, span[0]))
			return;

		if (diff < 0)
			diff = 0;
		else
			setTimeout(update_countdown, (diff + 999) % 1000 + 1);

		diff = Math.round(diff / 1000); // To mitigate little latency that may occur
		var diff_text = '';
		var make_diff_text = function(n, singular, plural) {
			return n + (n == 1 ? singular : plural);
		};

		if (diff >= 3600)
			diff_text += make_diff_text(Math.floor(diff / 3600), ' hour', ' hours');
		if (diff >= 60)
			diff_text += make_diff_text(Math.floor((diff % 3600) / 60), ' minute', ' minutes');
		diff_text += ' and ' + make_diff_text(diff % 60, ' second', ' seconds');
		span.text(diff_text);
	}

	setTimeout(update_countdown);
	return span;
}

// Dropdowns
$(document).ready(function(){
	$(document).click(function(event) {
		if ($(event.target).parent('.dropmenu .dropmenu-toggle') !== 0) {
			var dropmenu = $(event.target).closest('.dropmenu');
			if (dropmenu.is('.open'))
				dropmenu.removeClass('open');
			else {
				$('.dropmenu.open').removeClass('open');
				dropmenu.addClass('open');
			}
		} else
			$('.dropmenu.open').removeClass('open');
	});
});
function append_btn_tooltip(elem, text) {
	var tooltip = $('<span>', {
		class: 'btn-tooltip',
		text: text,
		click: function(e) {
			e.stopPropagation();
			tooltip.remove();
		}
	});
	tooltip.appendTo(elem);
	setTimeout(function() {
		tooltip.fadeOut('slow', function() {
			tooltip.remove()
		});
	}, 750);
}
function copy_to_clipboard_btn(small, btn_text, get_text_to_copy) {
	if (!document.queryCommandSupported('copy'))
		return $();

	return $('<a>', {
		class: small ? 'btn-small' : 'btn',
		style: 'position: relative',
		text: btn_text,
		click: function() {
			copy_to_clipboard(get_text_to_copy);
			append_btn_tooltip($(this), 'Copied to clipboard!');
		}
	});
}
// Handle navbar correct size
function normalize_navbar() {
	var navbar = $('.navbar');
	navbar.css('position', 'fixed');

	if (navbar.outerWidth() > $(window).width())
		navbar.css('position', 'absolute');
}
$(document).ready(normalize_navbar);
$(window).resize(normalize_navbar);

// Handle menu correct size
function normalize_menu() {
	var menu = $('.menu');
	menu.css('height', 'auto');

	if (menu.outerHeight() <= $(window).height()) {
		menu.css('height', '100%');
		menu.css('position', 'fixed');
	} else {
		menu.css('position', 'absolute');
		menu.outerHeight($(document).height());
	}
}
$(document).ready(normalize_menu);
$(window).resize(normalize_menu);

// Adding csrf token to a form; This is deprecated
function add_csrf_token_to(form) {
	var x = $(form);
	x.find('input[name="csrf_token"]').remove(); // Avoid duplication
	x.append('<input type="hidden" name="csrf_token" value="' +
		get_cookie('csrf_token') + '">');
	return x;
}

// Adding csrf token just before submitting a form
$(document).ready(function() {
	$('form').submit(function() { add_csrf_token_to(this); });
});


/* =========================== History management =========================== */
var History = {};
(function() {
	History.waiting_for_popstate = false;

	var queue = new Array();
	History.process_queue = function() {
		if (queue.length === 0)
			return;

		if (!History.waiting_for_popstate)
			queue.shift()();

		setTimeout(History.process_queue);
	};

	History.deferUntilFullyUpdated = function(func) {
		queue.push(func);
		if (queue.length === 1) // No one is processing
			History.process_queue();
	};

	History.pushState = function(egid, new_location) {
		History.deferUntilFullyUpdated(function() {
			window.history.pushState({egid: egid}, '', new_location);
			url_hash_parser.assign(window.location.hash);
		});
	};

	History.back = function(egid, new_location) {
		History.deferUntilFullyUpdated(function() {
			History.waiting_for_popstate = true;
			window.history.back();
		});
	}

	History.forth = function(egid, new_location) {
		History.deferUntilFullyUpdated(function() {
			History.waiting_for_popstate = true;
			window.history.go(1);
		});
	}

	History.replaceState = function(egid, new_location) {
		History.deferUntilFullyUpdated(function() {
			window.history.replaceState({egid: egid}, '', new_location);
			url_hash_parser.assign(window.location.hash);
		});
	};
}).call();

window.onpopstate = function(event) {
	History.waiting_for_popstate = false;

	if (event.state === null)
		return window.location.reload();

	var elem = $('*[egid="' + event.state.egid + '"]');
	if (elem.length === 0)
		return window.location.reload();

	var modals;
	if (elem.is('body'))
		modals = elem.children('.modal');
	else
		modals = elem.nextAll('.modal');

	// Remove other modals that cover the elem
	var view_element_removed = false;
	modals.each(function() {
		var elem = $(this);
		if (elem.hasClass('view') || view_element_removed) {
			view_element_removed = true;
			remove_modals(elem);
		}
	});
};

var egid = {};
(function () {
	var id = 0;
	egid.next_from = function(egid) {
		var egid_id = String(egid).replace(/.*-/, '');
		return String(egid).replace(/[^-]*$/, ++egid_id);
	};

	egid.get_next = function() {
		return ''.concat(window.performance.timing.responseStart, '-', id++);
	};
}).call();

function give_body_egid() {
	if ($('body').attr('egid') !== undefined)
		return;

	var body_egid = egid.get_next();
	$('body').attr('egid', body_egid);

	History.replaceState(body_egid, document.URL);
}
$(document).ready(give_body_egid);

/* const */ var fade_in_duration = 50; // milliseconds

// For best effect, the values below should be the same
/* const */ var loader_show_delay = 400; // milliseconds
/* const */ var timed_hide_delay = 400; // milliseconds

/* ================================= Loader ================================= */
function remove_loader(elem) {
	elem.removeChild(elem.querySelector('.loader'));
}
function try_remove_loader(elem) {
	var child = elem.querySelector('.loader');
	if (child != null) {
		elem.removeChild(child);
	}
}
function try_remove_loader_info(elem) {
	var child = elem.querySelector('.loader-info');
	if (child != null) {
		elem.removeChild(child);
	}
}
function append_loader(elem) {
	try_remove_loader_info(elem);
	var loader;
	if (elem.style.animationName === undefined && elem.style.WebkitAnimationName == undefined) {
		loader = elem_with_class('img', 'loader');
		loader.setAttribute('src', '/kit/img/loader.gif');
	} else {
		loader = elem_with_class('span', 'loader');
		loader.style.display = 'none';
		loader.appendChild(elem_with_class('div', 'spinner'));
		setTimeout(() => {
			$(loader).fadeIn(fade_in_duration);
		}, loader_show_delay);
	}
	elem.appendChild(loader);
}
function show_success_via_loader(elem, html) {
	try_remove_loader(elem);
	var loader_info = elem_with_class('span', 'loader-info success');
	loader_info.style.display = 'none';
	loader_info.innerHTML = html;
	$(loader_info).fadeIn(fade_in_duration);
	elem.appendChild(loader_info);
	timed_hide_show(elem.closest('.modal'));
}
function show_error_via_loader(elem, response, err_status, try_again_handler) {
	if (err_status == 'success' || err_status == 'error' || err_status === undefined)
		err_status = '';
	else
		err_status = '; ' + err_status;

	elem = $(elem);
	try_remove_loader(elem[0]);
	elem.append($('<span>', {
		class: 'loader-info error',
		style: 'display:none',
		html: $('<span>', {
			text: "Error: " + response.status + ' ' + response.statusText + err_status
		}).add(try_again_handler === undefined ? '' : $('<center>', {
			html: $('<a>', {
				text: 'Try again',
				click: try_again_handler
			})
		}))
	}).fadeIn(fade_in_duration));

	timed_hide_show(elem.closest('.modal'));

	// Additional message
	var x = elem.find('.loader-info > span');
	try {
		var msg = $($.parseHTML(response.responseText)).text();

		if (msg != '')
			x.text(x.text().concat("\n", msg));

	} catch (err) {
		if (response.responseText != '' && // There is a message
			response.responseText.lastIndexOf('<!DOCTYPE html>', 0) !== 0 && // Message is not a whole HTML page
			response.responseText.lastIndexOf('<!doctype html>', 0) !== 0) // Message is not a whole HTML page
		{
			x.text(x.text().concat("\n", response.responseText));
		}
	}
}

var get_unique_id = function() {
	var id = 0;
	return function() {
		return id++;
	};
}();

/* ================================= Form ================================= */
var Form = {}; // This became deprecated, use AjaxForm
Form.field_group = function(label_text_or_html_content, input_context_or_html_elem) {
	var input;
	if (input_context_or_html_elem instanceof jQuery)
		input = input_context_or_html_elem;
	else if (input_context_or_html_elem instanceof HTMLElement)
		input = $(input_context_or_html_elem);
	else
		input = $('<input>', input_context_or_html_elem);

	var id;
	if (input.is('input[type="checkbox"]')) {
		id = 'checkbox' + get_unique_id();
		input.attr('id', id);
	}

	var is_label_html_content = (Array.isArray(label_text_or_html_content) || label_text_or_html_content instanceof jQuery || input_context_or_html_elem instanceof HTMLElement);

	return $('<div>', {
		class: 'field-group',
		html: [
			$('<label>', {
				text: (is_label_html_content ? undefined : label_text_or_html_content),
				html: (is_label_html_content ? label_text_or_html_content : undefined),
				for: id
			}),
			input
		]
	});
};
// This became deprecated, use AjaxForm
Form.send_via_ajax = function(form, url, success_msg_or_handler /*= 'Success'*/, loader_parent)
{
	if (success_msg_or_handler === undefined)
		success_msg_or_handler = 'Success';
	if (loader_parent === undefined)
		loader_parent = $(form);

	form = $(form);
	add_csrf_token_to(form);
	append_loader(loader_parent[0]);

	// Transform data before sending
	var form_data = new FormData(form[0]);
	form.find('[trim_before_send="true"]').each(function() {
		if (this.name != null) {
			form_data.set(this.name, form_data.get(this.name).trim());
		}
	});
	// Prepare and send form
	$.ajax({
		url: url,
		type: 'POST',
		processData: false,
		contentType: false,
		data: form_data,
		success: function(resp) {
			if (typeof success_msg_or_handler === "function") {
				success_msg_or_handler.call(form, resp, loader_parent);
			} else
				show_success_via_loader(loader_parent[0], success_msg_or_handler);
		},
		error: function(resp, status) {
			show_error_via_loader(loader_parent, resp, status);
		}
	});
	return false;
};
// This became deprecated, use AjaxForm
function ajax_form(title, target, html, success_msg_or_handler, classes) {
	return $('<div>', {
		class: 'form-container' + (classes === undefined ? '' : ' ' + classes),
		html: $('<h1>', {text: title})
		.add('<form>', {
			method: 'post',
			html: html
		}).submit(function() {
			return Form.send_via_ajax(this, target, success_msg_or_handler);
		})
	});
}

/* ================================= Modals ================================= */
function remove_modals(modal) {
	$(modal).remove();
}
function close_modal(modal) {
	modal = $(modal);
	// Run pre-close callbacks
	modal.each(function() {
		if (this.onmodalclose !== undefined && this.onmodalclose() === false)
			return;

		if ($(this).is('.view'))
			History.back();
		else
			remove_modals(this);
	});
}
$(document).click(function(event) {
	if ($(event.target).is('.modal'))
		close_modal($(event.target));
	else if ($(event.target).is('.modal .close'))
		close_modal($(event.target).parent().parent());
});
$(document).keydown(function(event) {
	if (event.key == 'Escape')
		close_modal($('body').children('.modal').last());
});
function modal(modal_body, before_callback /*= undefined*/) {
	var mod = $('<div>', {
		class: 'modal',
		egid: egid.get_next(),
		style: 'display: none',
		html: $('<div>', {
			html: $('<span>', { class: 'close' }).add(modal_body)
		})
	});

	if (before_callback !== undefined)
		before_callback(mod);

	mod.appendTo('body').fadeIn(fade_in_duration);
	centerize_modal(mod);
	return mod;
}
function centerize_modal(modal, allow_lowering /*= true*/) {
	var m = $(modal);
	if (m.length === 0)
		return;

	if (allow_lowering === undefined)
		allow_lowering = true;

	var modal_css = m.css(['display', 'position', 'left']);
	if (modal_css.display === 'none') {
		// Make visible for a while to get sane width properties
		m.css({
			display: 'block',
			position: 'fixed',
			left: '200%'
		});
	}

	// Update padding-top
	var new_padding = (m.innerHeight() - m.children('div').innerHeight()) / 2;
	if (modal_css.display === 'none')
		m.css(modal_css);

	if (!allow_lowering) {
		var old_padding = m.css('padding-top');
		if (old_padding !== undefined && parseInt(old_padding.slice(0, -2)) < new_padding)
			return;
	}

	m.css({'padding-top': Math.max(new_padding, 0)});
}
/// Sends ajax form and shows modal with the result
function modal_request(title, form, target_url, success_msg) {
	var elem = modal($('<h2>', {text: title}));
	Form.send_via_ajax(form, target_url, success_msg, elem.children());
}
function dialogue_modal_request(title, info_html, go_text, go_classes, target_url, success_msg, cancel_text, remove_buttons_on_click) {
	modal($('<h2>', {text: title})
		.add(info_html).add('<div>', {
			style: 'margin: 12px auto 0',
			html: $('<a>', {
				class: go_classes,
				text: go_text,
				click: function() {
					var loader_parent = $(this).parent().parent();
					var form = loader_parent.find('form');
					if (form.length === 0)
						form = $('<form>');

					if (remove_buttons_on_click)
						$(this).parent().remove();

					Form.send_via_ajax(form, target_url, success_msg,
						loader_parent);
				}
			}).add((cancel_text === undefined ? '' : $('<a>', {
				class: 'btn-small',
				text: cancel_text,
				click: function() {
					$(this).parent().parent().parent().remove();
				}
			})))
		})
	);
}

function parse_api_resp(data) {
	var names = data[0];
	if (names === undefined || (names.columns === undefined && names.fields === undefined))
		return data;

	function transform(names, data) {
		var to_obj = function(names, array) {
			var obj = {};
			var e = Math.min(names.length, array.length);
			for (var j = 0; j < e; ++j) {
				if (names[j].name === undefined)
					obj[names[j]] = array[j];
				else
					obj[names[j].name] = transform(names[j], array[j]);
			}

			return obj;
		};

		if (names.columns !== undefined) {
			var res = [];
			for (var i = 0; i < data.length; ++i)
				res.push(to_obj(names.columns, data[i]));
			return res;
		}

		// names.fields
		return to_obj(names.fields, data);
	}

	return transform(names, data.slice(1));
}

function old_API_call(ajax_url, success_handler, loader_parent) {
	var self = this;
	append_loader(loader_parent[0]);
	$.ajax({
		url: ajax_url,
		type: 'POST',
		processData: false,
		contentType: false,
		data: new FormData(add_csrf_token_to($('<form>')).get(0)),
		dataType: 'json',
		success: function(data, status, jqXHR) {
			remove_loader(loader_parent[0]);
			success_handler.call(this, parse_api_resp(data), status, jqXHR);
		},
		error: function(resp, status) {
			show_error_via_loader(loader_parent, resp, status,
				setTimeout.bind(null, old_API_call.bind(self, ajax_url, success_handler, loader_parent))); // Avoid recursion
		}
	});
}

function API_get(url, success_handler, loader_parent) {
	var self = this;
	append_loader(loader_parent[0]);
	$.ajax({
		url: url,
		type: 'GET',
		dataType: 'json',
		success: function(data, status, jqXHR) {
			remove_loader(loader_parent[0]);
			success_handler.call(this, data, status, jqXHR);
		},
		error: function(resp, status) {
			show_error_via_loader(loader_parent, resp, status,
				setTimeout.bind(null, API_get.bind(self, url, success_handler, loader_parent))); // Avoid recursion
		}
	});
}

/* ================================ Tab menu ================================ */
function tabname_to_hash(tabname) {
	return tabname.toLowerCase().replace(/ /g, '_');
}
function default_tabmenu_attacher(x) {
	x.on('tabmenuTabHasChanged', function() { x.nextAll().remove(); });
	x.appendTo(this);
}
function tabmenu_attacher_with_change_callback(tabmenu_changed_callback, x) {
	x.on('tabmenuTabHasChanged', tabmenu_changed_callback);
	default_tabmenu_attacher.call(this, x);
}
/// Triggers 'tabmenuTabHasChanged' event on the result every time an active tab is changed
function tabmenu(attacher, tabs) {
	var res = $('<div>', {class: 'tabmenu'});
	/*const*/ var prior_hash = url_hash_parser.parsed_prefix();

	var set_min_width = function(elem) {
		var tabm = $(elem).parent();
		var mdiv = tabm.closest('.modal > div');
		if (mdiv.length === 0)
			return; // this is not in the modal

		var modal_css = mdiv.parent().css(['display', 'position', 'left']);
		if (modal_css.display === 'none') {
			// Make visible for a while to get sane width properties
			mdiv.parent().css({
				display: 'block',
				position: 'fixed',
				left: '200%'
			});
		}
		var mdiv_owidth = mdiv.outerWidth();

		// Get the client width
		var cw = mdiv.parent()[0].clientWidth;
		if (cw === undefined) {
			mdiv.css({
				'min-width': '100%',
				'max-width': '100%'
			});
			cw = mdiv.outerWidth();
		}

		if (modal_css.display === 'none')
			mdiv.parent().css(modal_css);

		var p = Math.min(100, 100 * (mdiv_owidth + 2) / cw);
		mdiv.css({
			'min-width': 'calc('.concat(p + '% - 2px)'),
			'max-width': ''
		});
	}

	var click_handler = function(handler) {
		return function(e) {
			if (!$(this).hasClass('active')) {
				set_min_width(this);
				$(this).parent().children('.active').removeClass('active');
				$(this).addClass('active');

				var elem = this;
				History.deferUntilFullyUpdated(function(){
					window.history.replaceState(window.history.state, '',
						document.URL.substring(0, document.URL.length - window.location.hash.length) + prior_hash + '#' + tabname_to_hash($(elem).text()));
					url_hash_parser.assign_as_parsed(window.location.hash);
					res.trigger('tabmenuTabHasChanged', elem);
					handler.call(elem);
					centerize_modal($(elem).parents('.modal'), false);
				});
			}
			e.preventDefault();
		};
	};
	for (var i = 0; i < tabs.length; i += 2)
		res.append($('<a>', {
			href: document.URL.substring(0, document.URL.length - window.location.hash.length) + prior_hash + '#' + tabname_to_hash(tabs[i]),
			text: tabs[i],
			click: click_handler(tabs[i + 1])
		}));

	attacher(res);

	var arg = url_hash_parser.extract_next_arg();
	var rc = res.children();
	for (i = 0; i < rc.length; ++i) {
		var elem = $(rc[i]);
		if (tabname_to_hash(elem.text()) === arg) {
			set_min_width(this);
			elem.addClass('active');
			res.trigger('tabmenuTabHasChanged', elem);

			tabs[i << 1 | 1].call(elem);
			centerize_modal(elem.parents('.modal'), false);
			return;
		}
	}

	rc.first().click();
}

/* ================================== View ================================== */
function view_base(as_modal, new_window_location, func, no_modal_elem) {
	var next_egid;
	if (as_modal) {
		next_egid = egid.get_next();
		var elem = $('<div>', {
			class: 'modal view',
			egid: next_egid,
			style: 'display: none',
			html: $('<div>', {
				html: $('<span>', { class: 'close'})
				.add('<div>', {style: 'display:block'})
			})
		});

		$(elem).attr('egid', next_egid);
		History.pushState(next_egid, new_window_location);

		elem.appendTo('body').fadeIn(fade_in_duration);
		func.call(elem.find('div > div'));
		centerize_modal(elem);

	// Use body as the parent element
	} else if (no_modal_elem === undefined || $(no_modal_elem).is('body')) {
		give_body_egid();
		History.replaceState($('body').attr('egid'), new_window_location);

		func.call($('body'));

	// Use @p no_modal_elem as the parent element
	} else {
		next_egid = egid.get_next();
		$(no_modal_elem).attr('egid', next_egid);
		History.pushState(next_egid, new_window_location);

		func.call($(no_modal_elem));
	}
}
function timed_hide(elem, hide_time /*= timed_hide_delay [milliseconds] */) {
	if (hide_time === undefined)
		hide_time = timed_hide_delay;

	$(elem).hide();
	$(elem).each(function() {
		var xx = this;
		xx.show = function() {
			window.clearTimeout(timer);
			$(xx).fadeIn(fade_in_duration);
		};
		var timer = window.setTimeout(xx.show, hide_time);
	});
}
function timed_hide_show(elem) {
	$(elem).each(function() {
		if (this.show !== undefined)
			this.show();
	});
}
function old_view_ajax(as_modal, ajax_url, success_handler, new_window_location, no_modal_elem /*= document.body*/, show_on_success /*= true */) {
	view_base(as_modal, new_window_location, function() {
		var elem = $(this);
		var modal = elem.parent().parent();
		if (as_modal)
			timed_hide(modal);
		old_API_call(ajax_url, function() {
			if (as_modal && (show_on_success !== false))
				timed_hide_show(modal);
			success_handler.apply(elem, arguments);
			if (as_modal)
				centerize_modal(modal);
		}, elem);
	}, no_modal_elem);
}
function view_ajax(as_modal, ajax_url, success_handler, new_window_location, no_modal_elem /*= document.body*/, show_on_success /*= true */) {
	view_base(as_modal, new_window_location, function() {
		var elem = $(this);
		var modal = elem.parent().parent();
		if (as_modal)
			timed_hide(modal);
		API_get(ajax_url, function() {
			if (as_modal && (show_on_success !== false))
				timed_hide_show(modal);
			success_handler.apply(elem, arguments);
			if (as_modal)
				centerize_modal(modal);
		}, elem);
	}, no_modal_elem);
}
function a_view_button(href, text, classes, func) {
	var a = document.createElement('a');
	if (href !== undefined)
		a.href = href;
	a.innerText = text;
	if (classes !== undefined)
		a.className = classes;
	a.onclick = function(event) {
		if (event.ctrlKey)
			return true; // Allow the link to open in a new tab
		func();
		return false;
	};
	return a;
}
function api_request_with_password_to_job(elem, title, api_url, message_html, confirm_text, success_msg, form_elements) {
	var as_modal = elem.closest('.modal').length !== 0;
	elem.append(ajax_form(title, api_url,
		$('<p>', {
			style: 'margin: 0 0 20px; text-align: center; max-width: 420px',
			html: message_html
		}).add(form_elements)
		.add(Form.field_group('Your password', {
			type: 'password',
			name: 'password',
			size: 24,
		})).add('<div>', {
			html: $('<input>', {
				class: 'btn red',
				type: 'submit',
				value: confirm_text
			}).add('<a>', {
				class: 'btn',
				href: (as_modal ? undefined : '/'),
				text: 'Go back',
				click: function() {
					var modal = $(this).closest('.modal');
					if (modal.length === 0)
						history.back();
					else
						close_modal(modal);
				}
			})
		}), function(resp, loader_parent) {
			if (as_modal) {
				show_success_via_loader($(this)[0], success_msg);
				view_job(true, resp);
			} else {
				this.parent().remove();
				window.location.href = '/jobs/' + resp;
			}
		}
	));
}
function delete_with_password_to_job(elem, title, api_url, message_html, confirm_text) {
	return api_request_with_password_to_job(elem, title, api_url, message_html, confirm_text, 'Deletion has been scheduled.');
}

/* ================================= Lister ================================= */
function OldLister(elem) {
	var this_ = this;
	this.elem = $(elem);
	var lock = false;

	this.need_to_fetch_more = function () {
		// Iff scrolling down is (almost) not possible, true is returned
		return how_much_is_viewport_bottom_above_elem_bottom(this_.elem[0]) <= 300;
	}

	this.fetch_more = function() {
		if (lock || !$.contains(document.documentElement, this_.elem[0]))
			return;

		lock = true;
		append_loader(this_.elem.parent()[0]);

		$.ajax({
			url: this_.query_url + this_.query_suffix,
			type: 'POST',
			processData: false,
			contentType: false,
			data: new FormData(add_csrf_token_to($('<form>')).get(0)),
			dataType: 'json',
			success: function(data) {
				var modal = this_.elem.parents('.modal');
				data = parse_api_resp(data);
				this_.process_api_response(data, modal);

				remove_loader(this_.elem.parent()[0]);
				timed_hide_show(modal);
				centerize_modal(modal, false);

				if ((Array.isArray(data) && data.length === 0) || (Array.isArray(data.rows) && data.rows.length === 0))
					return; // No more data to load

				lock = false;
				if (this_.need_to_fetch_more()) {
					setTimeout(this_.fetch_more, 0); // avoid recursion
				}
			},
			error: function(resp, status) {
				show_error_via_loader(this_.elem.parent(), resp, status,
					function () {
						lock = false;
						this_.fetch_more();
					});
			}
		});
	};

	this.monitor_scroll = function() {
		var scres_handler;
		var elem_to_listen_on_scroll;
		var scres_unhandle_if_detatched = function() {
			if (!$.contains(document.documentElement, this_.elem[0])) {
				elem_to_listen_on_scroll.off('scroll', scres_handler);
				$(window).off('resize', scres_handler);
				return;
			}
		};
		var modal_parent = this_.elem.closest('.modal');
		elem_to_listen_on_scroll = (modal_parent.length === 1 ? modal_parent : $(document));

		scres_handler = function() {
			scres_unhandle_if_detatched();
			if (this_.need_to_fetch_more())
				this_.fetch_more();
		};

		elem_to_listen_on_scroll.on('scroll', scres_handler);
		$(window).on('resize', scres_handler);
	};
}

function Lister(elem, query_url, initial_next_query_suffix) {
	var self = this;
	self.elem = elem;
	self.next_query_suffix = initial_next_query_suffix;

	var modal = self.elem.closest('.modal');
	var fetch_lock = false;
	var is_first_fetch = true;
	var shutdown = false;

	var is_lister_detached = function() {
		return !$.contains(document.documentElement, self.elem);
	};
	var do_shutdown;

	// Checks whether scrolling down is (almost) impossible
	var need_to_fetch_more = function () {
		return how_much_is_viewport_bottom_above_elem_bottom(self.elem) <= 300;
	}

	self.fetch_more = function() {
		if (fetch_lock || shutdown) {
			return;
		}
		if (is_lister_detached()) {
			do_shutdown();
			return;
		}

		fetch_lock = true;
		append_loader(self.elem.parentNode);

		$.ajax({
			url: query_url + self.next_query_suffix,
			type: 'GET',
			dataType: 'json',
			success: function(data) {
				if (is_first_fetch) {
					is_first_fetch = false;
					self.process_first_api_response(data.list);
				} else if (data.list.length != 0) {
					self.process_api_response(data.list);
				}

				remove_loader(self.elem.parentNode);
				timed_hide_show(modal);
				centerize_modal(modal, false);

				if (!data.may_be_more) {
					do_shutdown(); // No more data to load
					fetch_lock = false;
					return;
				}

				fetch_lock = false;
				if (need_to_fetch_more()) {
					setTimeout(self.fetch_more, 0); // schedule fetching more
				}
			},
			error: function(resp, status) {
				show_error_via_loader(self.elem.parentNode, resp, status, function() {
					fetch_lock = false;
					self.fetch_more();
				});
			}
		});
	};

	var scroll_or_resize_event_handler = function() {
		if (is_lister_detached()) {
			do_shutdown();
			return;
		}
		if (need_to_fetch_more()) {
			setTimeout(self.fetch_more, 0); // schedule fetching more without blocking
		}
	}

	// Start listening for scroll and resize events
	var elem_to_listen_on_scroll = modal === null ? document : modal_parent;
	elem_to_listen_on_scroll.addEventListener('scroll', scroll_or_resize_event_handler, {passive: true});
	window.addEventListener('resize', scroll_or_resize_event_handler, {passive: true});

	do_shutdown = function() {
		shutdown = true;
		elem_to_listen_on_scroll.removeEventListener('scroll', scroll_or_resize_event_handler);
		window.removeEventListener('resize', scroll_or_resize_event_handler);
	};

	if (need_to_fetch_more()) {
		self.fetch_more();
	}
}

////////////////////////////////////////////////////////////////////////////////

/* ================================== Logs ================================== */
function HexToUtf8Parser() {
	var state = 0;
	var value = 0;

	this.feed = function(hexstr) {
		hexstr = hexstr.toString(); // force conversion
		if (hexstr.length % 2 !== 0)
			throw new Error("hexstr has to have even length");

		var res = '';
		for (var i = 0; i < hexstr.length; i += 2) {
			var char = parseInt(hexstr.substring(i, i + 2), 16);
			if (state == 0) {
				// No sequence has started yet
				if (char < 128) { // 128 = 0b10000000
					res += String.fromCharCode(char);
					continue;
				}

				if (char < 192) // 192 == 0b11000000
					continue; // Ignore invalid character

				if (char < 224) { // 224 == 0b11100000
					// 0b110xxxxx <--> one more byte to read
					state = 1;
					value = char & ((1 << 5) - 1);
					continue;
				}

				if (char < 240) { // 240 == 0b11110000
					// 0b1110xxxx <--> two more bytes to read
					state = 2;
					value = char & ((1 << 4) - 1);
					continue;
				}

				if (char < 248) { // 248 == 0b11111000
					// 0b11110xxx <--> two more bytes to read
					state = 3;
					value = char & ((1 << 3) - 1);
					continue;
				}

				continue; // Ignore invalid character
			}

			if (char < 128 || char >= 192) // 192 = 0b11000000
				continue; // Ignore invalid character

			value = (value << 6) | (char & ((1 << 6) - 1));
			if (--state == 0)
				res += String.fromCharCode(value);
		}

		return res;
	}
};
function colorize(log, end) {
	if (end === undefined || end > log.length)
		end = log.length;

	var res = "";
	var opened = null;

	function close_last_tag() {
		if (opened == 'span')
			res += '</span>';
		else if (opened == 'b')
			res += '</b>';
		opened = null;
	}

	function contains(idx, str) {
		return (log.substring(idx, idx + str.length) == str);
	}

	for (var i = 0; i < end; ++i) {
		if (contains(i, '\033[30m')) {
			close_last_tag();
			res += '<span class="gray">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[31m')) {
			close_last_tag();
			res += '<span class="red">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[32m')) {
			close_last_tag();
			res += '<span class="green">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[33m')) {
			close_last_tag();
			res += '<span class="yellow">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[34m')) {
			close_last_tag();
			res += '<span class="blue">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[35m')) {
			close_last_tag();
			res += '<span class="magentapink">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[36m')) {
			close_last_tag();
			res += '<span class="turquoise">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[1;30m')) {
			close_last_tag();
			res += '<b class="gray">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[1;31m')) {
			close_last_tag();
			res += '<b class="red">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[1;32m')) {
			close_last_tag();
			res += '<b class="green">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[1;33m')) {
			close_last_tag();
			res += '<b class="yellow">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[1;34m')) {
			close_last_tag();
			res += '<b class="blue">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[1;35m')) {
			close_last_tag();
			res += '<b class="pink">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[1;36m')) {
			close_last_tag();
			res += '<b class="turquoise">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[m') && opened !== null) {
			close_last_tag();
			opened = null;
			i += 2;
		} else
			res += log[i];
	}
	close_last_tag();

	return res + log.substring(end);
}
function Logs(type, elem, auto_refresh_checkbox) {
	var this_ = this;
	this.type = type;
	this.elem = $(elem);
	this.offset = undefined;
	var lock = false; // allow only manual unlocking
	var offset, first_offset;
	var content = $('<span>').appendTo(elem);
	var hex_parser = new HexToUtf8Parser();

	var process_data = function(data) {
		data[0] = parseInt(data[0]);
		if (first_offset === undefined)
			first_offset = data[0];
		else if (data[0] > first_offset) { // Newer logs arrived
			first_offset = data[0];
			content.empty();
		}

		offset = data[0];
		data = hex_parser.feed(data[1]);

		var prev_height = content[0].scrollHeight;
		var bottom_dist = prev_height - content[0].scrollTop;

		remove_loader(this_.elem[0]);
		var html_data = text_to_safe_html(data);
		content.html(colorize(html_data + content.html(), html_data.length + 2000));
		var curr_height = content[0].scrollHeight;
		content.scrollTop(curr_height - bottom_dist);

		lock = false;

		// Load more logs unless scrolling up became impossible
		if (offset > 0 && (content.innerHeight() >= curr_height || prev_height == curr_height)) {
			// avoid recursion
			setTimeout(this_.fetch_more, 0);
		}
	};

	this.fetch_more = function() {
		if (lock || !$.contains(document.documentElement, this_.elem[0]))
			return;

		lock = true;

		append_loader(this_.elem[0]);
		$.ajax({
			url: '/api/logs/' + this_.type +
				(offset === undefined ? '' : '?' + offset),
			type: 'POST',
			processData: false,
			contentType: false,
			data: new FormData(add_csrf_token_to($('<form>')).get(0)),
			success: function (data) {
				process_data(String(data).split('\n'));
			},
			error: function(resp, status) {
				show_error_via_loader(this_.elem, resp, status, function () {
					lock = false; // allow only manual unlocking
					this_.fetch_more();
				});
			}
		});
	};

	this.try_fetching_newest = function() {
		if (lock)
			return;
		lock = true;

		append_loader(this_.elem[0]);
		$.ajax({
			url: '/api/logs/' + this_.type,
			type: 'POST',
			processData: false,
			contentType: false,
			data: new FormData(add_csrf_token_to($('<form>')).get(0)),
			success: function(data) {
				data = String(data).split('\n');
				if (parseInt(data[0]) !== first_offset)
					return process_data(data);

				remove_loader(this_.elem[0]);
				lock = false;
			},
			error: function(resp, status) {
				show_error_via_loader(this_.elem, resp, status, function () {
					lock = false; // allow only manual unlocking
					this_.fetch_more();
				});
			}
		});
	};

	var refreshing_interval_id = setInterval(function() {
		if (!$.contains(document.documentElement, this_.elem[0])) {
			clearInterval(refreshing_interval_id);
			return;
		}

		var elem = content[0];
		if (is_overflowed_elem_scrolled_down(elem) && auto_refresh_checkbox.prop('checked')) {
			this_.try_fetching_newest();
		}
	}, 2000);

	this.monitor_scroll = function() {
		var scres_handler;
		var scres_unhandle_if_detatched = function() {
			if (!$.contains(document.documentElement, this_.elem[0])) {
				content.off('scroll', scres_handler);
				$(window).off('resize', scres_handler);
				return;
			}
		};

		scres_handler = function() {
			scres_unhandle_if_detatched();
			if (content[0].scrollTop <= 300)
				this_.fetch_more();
		};

		content.on('scroll', scres_handler);
		$(window).on('resize', scres_handler);
	};

	this.fetch_more();
}
function tab_logs_view(parent_elem) {
	// Select job server log by default
	if (url_hash_parser.next_arg() === '')
		url_hash_parser.assign('#job_server');

	parent_elem = $(parent_elem);
	function retab(log_type, log_name) {
		var checkbox = $('<input>', {
			type: 'checkbox',
			checked: (log_type !== 'web')
		});

		$('<div>', {
			class: 'logs-header',
			html: [
				$('<h2>', {text: log_name + ' log:'}),
				$('<div>', {html:
					$('<label>', {html: [
						checkbox, 'auto-refresh'
					]})
				})
			]
		}).appendTo(parent_elem);

		parent_elem.addClass('logs-parent');
		new Logs(log_type, $('<pre>', {class: 'logs'}).appendTo(parent_elem), checkbox).monitor_scroll();
	}

	var tabs = [
		'Server (web)', retab.bind(null, 'web', "Server's"),
		'Server error (web)', retab.bind(null, 'web_err', "Server's error"),
		'Job server', retab.bind(null, 'jobs', "Job server's"),
		'Job server error', retab.bind(null, 'jobs_err', "Job server's error")
	];

	tabmenu(default_tabmenu_attacher.bind(parent_elem), tabs);
}

/* ============================ Actions buttons ============================ */
var ActionsToHTML = {};
ActionsToHTML.job = function(job_id, actions_str, problem_id, job_view /*= false*/) {
	if (job_view == undefined)
		job_view = false;

	var res = [];
	if (!job_view && actions_str.indexOf('v') !== -1)
		res.push(a_view_button('/jobs/' + job_id, 'View', 'btn-small',
			view_job.bind(null, true, job_id)));

	if (actions_str.indexOf('u') !== -1 || actions_str.indexOf('r') !== -1)
		res.push($('<div>', {
			class: 'dropmenu down',
			html: $('<a>', {
				class: 'btn-small dropmenu-toggle',
				text: 'Download'
			}).add('<ul>', {
				html: [actions_str.indexOf('r') === -1 ? '' :  $('<a>', {
						href: '/api/download/job/' + job_id + '/log',
						text: 'Job log'
					}), actions_str.indexOf('u') === -1 ? '' : $('<a>', {
						href: '/api/download/job/' + job_id + '/uploaded-package',
						text: 'Uploaded package'
					}), actions_str.indexOf('s') === -1 ? '' : $('<a>', {
						href: '/api/download/job/' + job_id + '/uploaded-statement',
						text: 'Uploaded statement'
					})
				]
			})
		}));

	if (actions_str.indexOf('p') !== -1)
		res.push(a_view_button('/p/' + problem_id, 'View problem',
			'btn-small green', view_problem.bind(null, true, problem_id)));

	if (actions_str.indexOf('C') !== -1)
		res.push($('<a>', {
			class: 'btn-small red',
			text: 'Cancel job',
			click: cancel_job.bind(null, job_id)
		}));

	if (actions_str.indexOf('R') !== -1)
		res.push(a_view_button(undefined, 'Restart job', 'btn-small orange',
			restart_job.bind(null, job_id)));

	return res;
};

ActionsToHTML.user = function(user, user_view /*= false*/) {
	if (user_view === undefined)
		user_view = false;
	var res = [];
	if (!user_view && user.capabilities.view) {
		res.push(a_view_button('/u/' + user.id, 'View', 'btn-small',
			view_user.bind(null, true, user.id)));
	}
	if (user.capabilities.edit) {
		res.push(a_view_button('/u/' + user.id + '/edit', 'Edit',
			'btn-small blue', edit_user.bind(null, true, user.id)));
	}
	if (user.capabilities.delete) {
		res.push(a_view_button('/u/' + user.id + '/delete', 'Delete',
			'btn-small red', delete_user.bind(null, true, user.id)));
	}
	if (user.capabilities.merge) {
		res.push(a_view_button('/u/' + user.id + '/merge_into_another',
			'Merge', 'btn-small red',
			merge_user.bind(null, true, user.id)));
	}
	if (user.capabilities.change_password || user.capabilities.change_password_without_old_password) {
		res.push(a_view_button('/u/' + user.id + '/change-password',
			'Change password', 'btn-small orange',
			change_user_password.bind(null, true, user.id)));
	}
	return res;
};

ActionsToHTML.submission = function(submission_id, actions_str, submission_type, submission_view /*= false*/) {
	if (submission_view === undefined)
		submission_view = false;

	var res = [];
	if (!submission_view && actions_str.indexOf('v') !== -1)
		res.push(a_view_button('/s/' + submission_id, 'View', 'btn-small',
			view_submission.bind(null, true, submission_id)));

	if (actions_str.indexOf('s') !== -1) {
		if (!submission_view)
			res.push(a_view_button('/s/' + submission_id + '#source', 'Source',
				'btn-small', view_submission.bind(null, true, submission_id, '#source')));

		if (submission_view) {
			var a = document.createElement('a');
			a.className = 'btn-small';
			a.href = '/api/download/submission/' + submission_id;
			a.innerText = 'Download';
			res.push(a);
		}
	}

	if (actions_str.indexOf('C') !== -1)
		res.push(a_view_button(undefined, 'Change type', 'btn-small orange',
			submission_chtype.bind(null, submission_id, submission_type)));

	if (actions_str.indexOf('R') !== -1)
		res.push(a_view_button(undefined, 'Rejudge', 'btn-small blue',
			rejudge_submission.bind(null, submission_id)));

	if (actions_str.indexOf('D') !== -1)
		res.push(a_view_button(undefined, 'Delete', 'btn-small red',
			delete_submission.bind(null, submission_id)));

	return res;
};

ActionsToHTML.problem = function(problem, problem_view /*= false*/) {
	if (problem_view === undefined)
		problem_view = false;

	var res = [];
	if (!problem_view && problem.actions.indexOf('v') !== -1)
		res.push(a_view_button('/p/' + problem.id, 'View', 'btn-small',
			view_problem.bind(null, true, problem.id)));

	if (problem.actions.indexOf('V') !== -1)
		res.push($('<a>', {
			class: 'btn-small',
			href: '/api/download/statement/problem/' + problem.id + '/' + encodeURIComponent(problem.name),
			text: 'Statement'
		}));

	if (problem.actions.indexOf('S') !== -1)
		res.push(a_view_button('/p/' + problem.id + '/submit', 'Submit',
			'btn-small blue', add_problem_submission.bind(null, true, problem)));

	if (problem.actions.indexOf('s') !== -1)
		res.push(a_view_button('/p/' + problem.id + '#all_submissions#solutions',
			'Solutions', 'btn-small', view_problem.bind(null, true, problem.id,
				'#all_submissions#solutions')));

	if (problem_view && problem.actions.indexOf('d') !== -1)
		res.push($('<a>', {
			class: 'btn-small',
			href: '/api/download/problem/' + problem.id,
			text: 'Download'
		}));

	if (problem.actions.indexOf('E') !== -1)
		res.push(a_view_button('/p/' + problem.id + '/edit', 'Edit',
			'btn-small blue', edit_problem.bind(null, true, problem.id)));

	// if (problem_view && problem.actions.indexOf('R') !== -1)
	// 	res.push(a_view_button('/p/' + problem.id + '/reupload', 'Reupload',
	// 		'btn-small orange', reupload_problem.bind(null, true, problem.id)));

	if (problem_view && problem.actions.indexOf('D') !== -1)
		res.push(a_view_button('/p/' + problem.id + '/delete', 'Delete',
			'btn-small red', delete_problem.bind(null, true, problem.id)));

	if (problem_view && problem.actions.indexOf('M') !== -1)
		res.push(a_view_button('/p/' + problem.id + '/merge', 'Merge',
			'btn-small red', merge_problem.bind(null, true, problem.id)));

	if (problem_view && problem.actions.indexOf('J') !== -1)
		res.push($('<a>', {
			class: 'btn-small blue',
			text: 'Rejudge all submissions',
			click: rejudge_problem_submissions.bind(null, problem.id, problem.name)
		}));

	if (problem_view && problem.actions.indexOf('L') !== -1)
		res.push(a_view_button('/p/' + problem.id + '/reset_time_limits',
			'Reset time limits', 'btn-small blue',
			reset_problem_time_limits.bind(null, true, problem.id)));

	return res;
};

ActionsToHTML.contest = function(contest_id, actions_str, contest_view /*= false*/) {
	if (contest_view === undefined)
		contest_view = false;

	var res = [];
	if (!contest_view && actions_str.indexOf('v') !== -1)
		res.push($('<a>', {
			class: 'btn-small',
			href: '/c/c' + contest_id,
			text: 'View'
		}), $('<a>', {
			class: 'btn-small',
			href: '/c/c' + contest_id + '#ranking',
			text: 'Ranking'
		}));

	if (contest_view && actions_str.indexOf('E') !== -1)
		res.push(a_view_button('/c/c' + contest_id + '/edit', 'Edit',
			'btn-small blue', edit_contest.bind(null, true, contest_id)));

	if (contest_view && actions_str.indexOf('D') !== -1)
		res.push(a_view_button('/c/c' + contest_id + '/delete', 'Delete',
			'btn-small red', delete_contest.bind(null, true, contest_id)));

	return res;
};

ActionsToHTML.contest_user = function(user_id, contest_id, actions_str) {
	var res = [];
	var make_contestant = (actions_str.indexOf('Mc') !== -1);
	var make_moderator = (actions_str.indexOf('Mm') !== -1);
	var make_owner = (actions_str.indexOf('Mo') !== -1);

	if (make_contestant + make_moderator + make_owner > 1)
		res.push(a_view_button('/c/c' + contest_id + '/contest_user/' + user_id + '/change_mode', 'Change mode', 'btn-small orange',
			change_contest_user_mode.bind(null, true, contest_id, user_id)));

	if (actions_str.indexOf('E') !== -1)
		res.push(a_view_button('/c/c' + contest_id + '/contest_user/' + user_id + '/expel',
			'Expel', 'btn-small red',
			expel_contest_user.bind(null, true, contest_id, user_id)));

	return res;
};

ActionsToHTML.contest_users = function(contest_id, overall_actions_str) {
	var add_contestant = (overall_actions_str.indexOf('Ac') !== -1);
	var add_moderator = (overall_actions_str.indexOf('Am') !== -1);
	var add_owner = (overall_actions_str.indexOf('Ao') !== -1);

	if (add_contestant || add_moderator || add_owner)
		return a_view_button('/c/c' + contest_id + '/contest_user/add',
			'Add user', 'btn', add_contest_user.bind(null, true, contest_id));

	return [];
};

ActionsToHTML.contest_file = function(contest_file_id, actions_str) {
	var res = [];

	if (actions_str.indexOf('O') !== -1) {
		var a = document.createElement('a');
		a.className = 'btn-small';
		a.href = '/api/download/contest_file/' + contest_file_id;
		a.innerText = 'Download';
		res.push(a);
	}

	if (actions_str.indexOf('E') !== -1)
		res.push(a_view_button('/contest_file/' + contest_file_id + '/edit',
			'Edit', 'btn-small blue', edit_contest_file.bind(null, true, contest_file_id)));

	if (actions_str.indexOf('D') !== -1)
		res.push(a_view_button('/contest_file/' + contest_file_id + '/delete',
			'Delete', 'btn-small red', delete_contest_file.bind(null, true, contest_file_id)));

	return res;
};

ActionsToHTML.contest_files = function(contest_id, overall_actions_str) {
	if (overall_actions_str.indexOf('A') !== -1)
		return a_view_button('/c/c' + contest_id + '/files/add',
			'Add file', 'btn', add_contest_file.bind(null, true, contest_id));

	return [];
};

/* ================================= Users ================================= */
function add_user(as_modal) {
	view_base(as_modal, '/u/add', function() {
		this.append(ajax_form('Add user', '/api/user/add',
			Form.field_group('Username', {
				type: 'text',
				name: 'username',
				size: 24,
				// maxlength: 'TODO...',
				trim_before_send: true,
				required: true
			}).add(Form.field_group('Type',
				$('<select>', {
					name: 'type',
					required: true,
					html: $('<option>', {
						value: 'A',
						text: 'Admin'
					}).add('<option>', {
						value: 'T',
						text: 'Teacher'
					}).add('<option>', {
						value: 'N',
						text: 'Normal',
						selected: true
					})
				})
			)).add(Form.field_group('First name', {
				type: 'text',
				name: 'first_name',
				size: 24,
				// maxlength: 'TODO...',
				trim_before_send: true,
				required: true
			})).add(Form.field_group('Last name', {
				type: 'text',
				name: 'last_name',
				size: 24,
				// maxlength: 'TODO...',
				trim_before_send: true,
				required: true
			})).add(Form.field_group('Email', {
				type: 'email',
				name: 'email',
				size: 24,
				// maxlength: 'TODO...',
				trim_before_send: true,
				required: true
			})).add(Form.field_group('Password', {
				type: 'password',
				name: 'pass',
				size: 24,
			})).add(Form.field_group('Password (repeat)', {
				type: 'password',
				name: 'pass1',
				size: 24,
			})).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Submit'
				})
			}), 'User was added'
		));
	});
}
function view_user(as_modal, user_id, opt_hash /*= ''*/) {
	view_ajax(as_modal, url_api_user(user_id), function(user) {
		this.append($('<div>', {
			class: 'header',
			html: $('<span>', {
				style: 'margin: auto 0',
				html: $('<a>', {
					href: '/u/' + user.id,
					text: user.username
				})
			}).append(text_to_safe_html(' (' + user.first_name + ' ' + user.last_name + ')'))
			.add('<div>', {
				html: ActionsToHTML.user(user, true)
			})
		})).append($('<center>', {
			class: 'always_in_view',
			html: $('<div>', {
				class: 'user-info',
				html: $('<div>', {
					class: 'first-name',
					html: $('<label>', {text: 'First name'})
				}).append(text_to_safe_html(user.first_name))
				.add($('<div>', {
					class: 'last-name',
					html: $('<label>', {text: 'Last name'})
				}).append(text_to_safe_html(user.last_name)))
				.add($('<div>', {
					class: 'username',
					html: $('<label>', {text: 'Username'})
				}).append(text_to_safe_html(user.username)))
				.add($('<div>', {
					class: 'type',
					html: $('<label>', {text: 'Account type'}).add('<span>', {
						class: user.type,
						text: user.type[0].toUpperCase() + user.type.slice(1),
					})
				}))
				.add($('<div>', {
					class: 'email',
					html: $('<label>', {text: 'Email'})
				}).append(text_to_safe_html(user.email)))
			})
		}));

		var main = $(this);

		tabmenu(default_tabmenu_attacher.bind(main), [
			'Submissions', function() {
				main.append($('<div>', {html: "<h2>User's submissions</h2>"}));
				tab_submissions_lister(main.children().last(), '/u' + user_id, false, !logged_user_is_admin());
			},
			'Jobs', function() {
				var j_table = $('<table>', {
					class: 'jobs'
				});
				main.append($('<div>', {
					html: ["<h2>User's jobs</h2>", j_table]
				}));
				new JobsLister(j_table, '/u' + user_id).monitor_scroll();
			}
		]);

	}, '/u/' + user_id + (opt_hash === undefined ? '' : opt_hash), undefined, false);
}
function edit_user(as_modal, user_id) {
	old_view_ajax(as_modal, '/api/users/=' + user_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		var user = data[0];
		var actions = user.actions;
		if (actions.indexOf('E') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Edit account', '/api/user/' + user_id + '/edit',
			Form.field_group('Username', {
				type: 'text',
				name: 'username',
				value: user.username,
				size: 24,
				// maxlength: 'TODO...',
				trim_before_send: true,
				required: true
			}).add(Form.field_group('Type',
				$('<select>', {
					name: 'type',
					required: true,
					html: function() {
						var res = [];
						if (actions.indexOf('A') !== -1)
							res.push($('<option>', {
								value: 'A',
								text: 'Admin',
								selected: ('Admin' === user.type ? true : undefined)
							}));

						if (actions.indexOf('T') !== -1)
							res.push($('<option>', {
								value: 'T',
								text: 'Teacher',
								selected: ('Teacher' === user.type ? true : undefined)
							}));

						if (actions.indexOf('N') !== -1)
							res.push($('<option>', {
								value: 'N',
								text: 'Normal',
								selected: ('Normal' === user.type ? true : undefined)
							}));

						return res;
					}()
				})
			)).add(Form.field_group('First name', {
				type: 'text',
				name: 'first_name',
				value: user.first_name,
				size: 24,
				// maxlength: 'TODO...',
				trim_before_send: true,
				required: true
			})).add(Form.field_group('Last name', {
				type: 'text',
				name: 'last_name',
				value: user.last_name,
				size: 24,
				// maxlength: 'TODO...',
				trim_before_send: true,
				required: true
			})).add(Form.field_group('Email', {
				type: 'email',
				name: 'email',
				value: user.email,
				size: 24,
				// maxlength: 'TODO...',
				trim_before_send: true,
				required: true
			})).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Update'
				})
			})
		));

	}, '/u/' + user_id + '/edit');
}
function delete_user(as_modal, user_id) {
	old_view_ajax(as_modal, '/api/users/=' + user_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		var user = data[0];
		var actions = user.actions;
		if (actions.indexOf('D') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		delete_with_password_to_job(this, 'Delete user ' + user_id,
			'/api/user/' + user_id + '/delete', [
				'You are going to delete ', (user_id == logged_user_id() ?
					'your account' : 'the user '), (user_id == logged_user_id() ?
					'' : a_view_button('/u/' + user_id, user.username, undefined,
						view_user.bind(null, true, user_id))),
					'. As it cannot be undone, you have to confirm it with ',
					(user_id == logged_user_id() ? 'your' : 'YOUR'), ' password.'
			], (user_id == logged_user_id() ? 'Delete account' : 'Delete user'));
	}, '/u/' + user_id + "/delete");
}
function merge_user(as_modal, user_id) {
	old_view_ajax(as_modal, '/api/users/=' + user_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
					status: '404',
					statusText: 'Not Found'
				});

		var user = data[0];
		if (user.actions.indexOf('M') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		api_request_with_password_to_job(this, 'Merge into another user',
			'/api/user/' + user_id + '/merge_into_another', [
				'The user ',
				a_view_button('/u/' + user.id, user.username,
					undefined, view_user.bind(null, true, user.id)),
				' is going to be deleted. All their problems, submissions, jobs and accesses to contests will be transfered to the target user.',
				'<br>',
				'As this cannot be undone, you have to confirm this with your password.'
			], 'Merge user', 'Merging has been scheduled.',
			Form.field_group('Target user ID', {
				type: 'text',
				name: 'target_user',
				size: 6,
				trim_before_send: true,
			}));
	}, '/u/' + user_id + '/merge');
}
function change_user_password(as_modal, user_id) {
	old_view_ajax(as_modal, '/api/users/=' + user_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		var user = data[0];
		var actions = user.actions;
		if (actions.indexOf('P') === -1 && actions.indexOf('p') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		if (actions.indexOf('P') === -1 && $('.navbar .user > strong').text() != user.username)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Change password',
			'/api/user/' + user_id + '/change-password',
			(actions.indexOf('P') !== -1 ? $() : Form.field_group('Old password', {
				type: 'password',
				name: 'old_pass',
				size: 24,
			})).add(Form.field_group('New password', {
				type: 'password',
				name: 'new_pass',
				size: 24,
			})).add(Form.field_group('New password (repeat)', {
				type: 'password',
				name: 'new_pass1',
				size: 24,
			})).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Update'
				})
			}), 'Password changed'));

	}, '/u/' + user_id + "/change-password");
}
function UsersLister(elem, query_url) {
	var self = this;
	Lister.call(self, elem, query_url, '');

	self.process_first_api_response = function(list) {
		if (list.length === 0) {
			self.elem.parentNode.appendChild(elem_with_text('p', 'There are no users to show...'));
			return;
		}

		var thead = document.createElement('thead');
		thead.appendChild(elem_with_text('th', 'Id'));
		thead.appendChild(elem_with_class_and_text('th', 'username', 'Username'));
		thead.appendChild(elem_with_class_and_text('th', 'first-name', 'First name'));
		thead.appendChild(elem_with_class_and_text('th', 'last-name', 'Last name'));
		thead.appendChild(elem_with_class_and_text('th', 'email', 'Email'));
		thead.appendChild(elem_with_class_and_text('th', 'type', 'Type'));
		thead.appendChild(elem_with_class_and_text('th', 'actions', 'Actions'));
		self.elem.appendChild(thead);
		self.tbody = document.createElement('tbody');
		self.elem.appendChild(self.tbody);

		self.process_api_response(list);
	}

	self.process_api_response = function(list) {
		self.next_query_suffix = '/id>/' + list[list.length - 1].id;

		for (var user of list) {
			var row = document.createElement('tr');
			row.appendChild(elem_with_text('td', user.id));
			row.appendChild(elem_with_text('td', user.username));
			row.appendChild(elem_with_text('td', user.first_name));
			row.appendChild(elem_with_text('td', user.last_name));
			row.appendChild(elem_with_text('td', user.email));
			row.appendChild(elem_with_class_and_text('td', user.type, user.type[0].toUpperCase() + user.type.slice(1)));

			var td = document.createElement('td');
			append_children(td, ActionsToHTML.user(user, false));
			row.appendChild(td);

			self.tbody.appendChild(row);
		}
	};
}
function tab_users_lister(parent_elem) {
	parent_elem = $(parent_elem);
	function retab(tab_qsuff) {
		var table = elem_with_class('table', 'users stripped');
		$(parent_elem)[0].appendChild(table);
		new UsersLister(table, url_api_users(tab_qsuff));
	}

	var tabs = [
		'All', retab.bind(null, ''),
		'Admins', retab.bind(null, '/type=/admin'),
		'Teachers', retab.bind(null, '/type=/teacher'),
		'Normal', retab.bind(null, '/type=/normal')
	];

	tabmenu(default_tabmenu_attacher.bind(parent_elem), tabs);
}
function user_chooser(as_modal /*= true*/, opt_hash /*= ''*/) {
	view_base((as_modal === undefined ? true : as_modal),
		'/u' + (opt_hash === undefined ? '' : opt_hash), function() {
			timed_hide($(this).parent().parent().filter('.modal'));
			$(this).append($('<h1>', {text: 'Users'}));
			if (logged_user_is_admin())
				$(this).append(a_view_button('/u/add', 'Add user', 'btn',
					add_user.bind(null, true)));

			tab_users_lister($(this));
		});
}

/* ================================== Jobs ================================== */
function view_job(as_modal, job_id, opt_hash /*= ''*/) {
	old_view_ajax(as_modal, '/api/jobs/=' + job_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		var job = data[0];
		function info_html(info) {
			var td = $('<td>', {
				style: 'text-align:left'
			});
			for (var name in info) {
				td.append($('<label>', {text: name}));

				if (name == "submission")
					td.append(a_view_button('/s/' + info[name], info[name],
						undefined, view_submission.bind(null, true, info[name])));
				else if (name == "user" || name == "deleted user" || name == "target user")
					td.append(a_view_button('/u/' + info[name], info[name],
						undefined, view_user.bind(null, true, info[name])));
				else if (name == "problem" || name == "deleted problem" || name == "target problem")
					td.append(a_view_button('/p/' + info[name], info[name],
						undefined, view_problem.bind(null, true, info[name])));
				else if (name == "contest")
					td.append(a_view_button('/c/c' + info[name], info[name],
						undefined, view_contest.bind(null, true, info[name])));
				else if (name == "contest round")
					td.append(a_view_button('/c/r' + info[name], info[name],
						undefined, view_contest_round.bind(null, true, info[name])));
				else if (name == "contest problem")
					td.append(a_view_button('/c/p' + info[name], info[name],
						undefined, view_contest_problem.bind(null, true, info[name])));
				else
					td.append(info[name]);

				td.append('<br>');
			}

			return td;
		}

		this.append($('<div>', {
			class: 'job-info',
			html: $('<div>', {
				class: 'header',
				html: $('<h1>', {
					text: 'Job ' + job_id
				}).add('<div>', {
					html: ActionsToHTML.job(job_id, job.actions, job.info.problem, true)
				})
			}).add('<table>', {
				html: $('<thead>', {html: '<tr>' +
					'<th style="min-width:120px">Type</th>' +
					'<th style="min-width:150px">Added</th>' +
					'<th style="min-width:150px">Status</th>' +
					'<th style="min-width:120px">Owner</th>' +
					'<th style="min-width:90px">Info</th>' +
					'</tr>'
				}).add('<tbody>', {
					html: $('<tr>', {
						html: $('<td>', {
							text: job.type
						}).add(normalize_datetime($('<td>', {
								datetime: job.added,
								text: job.added
							}), true)
						).add('<td>', {
							class: 'status ' + job.status.class,
							text: job.status.text
						}).add('<td>', {
							html: job.creator_id === null ? 'System' :
								(job.creator_username == null ? 'Deleted (id: ' + job.creator_id + ')'
								: a_view_button(
								'/u/' + job.creator_id, job.creator_username, undefined,
								view_user.bind(null, true, job.creator_id)))
						}).add('<td>', {
							html: info_html(job.info)
						})
					})
				})
			})
		}));

		if (job.actions.indexOf('r') !== -1) {
			this.append('<h2>Job log</h2>')
			.append($('<pre>', {
				class: 'job-log',
				html: colorize(text_to_safe_html(job.log.text))
			}));

			if (job.log.is_incomplete)
				this.append($('<p>', {
					text: 'The job log is too large to show it entirely here. If you want to see the whole, click: '
				}).append($('<a>', {
					class: 'btn-small',
					href: '/api/download/job/' + job_id + '/log',
					text: 'Download the full job log'
				})));
		}
	}, '/jobs/' + job_id + (opt_hash === undefined ? '' : opt_hash));
}
function cancel_job(job_id) {
	modal_request('Canceling job ' + job_id, $('<form>'),
		'/api/job/' + job_id + '/cancel', 'The job has been canceled.');
}
function restart_job(job_id) {
	dialogue_modal_request('Restart job', $('<label>', {
			html: [
				'Are you sure to restart the ',
				a_view_button('/jobs/' + job_id, 'job ' + job_id, undefined,
					view_job.bind(null, true, job_id)),
				'?'
			]
		}), 'Restart job', 'btn-small orange', '/api/job/' + job_id + '/restart',
		'The job has been restarted.', 'No, go back');
}
function JobsLister(elem, query_suffix /*= ''*/) {
	var this_ = this;
	if (query_suffix === undefined)
		query_suffix = '';

	OldLister.call(this, elem);
	this.query_url = '/api/jobs' + query_suffix;
	this.query_suffix = '';

	this.process_api_response = function(data, modal) {
		if (this_.elem.children('thead').length === 0) {
			if (data.length == 0) {
				this_.elem.parent().append($('<center>', {
					class: 'jobs always_in_view',
					// class: 'jobs',
					html: '<p>There are no jobs to show...</p>'
				}));
				remove_loader(this_.elem.parent()[0]);
				timed_hide_show(modal);
				return;
			}

			this_.elem.html('<thead><tr>' +
					'<th>Id</th>' +
					'<th class="type">Type</th>' +
					'<th class="priority">Priority</th>' +
					'<th class="added">Added</th>' +
					'<th class="status">Status</th>' +
					'<th class="owner">Owner</th>' +
					'<th class="info">Info</th>' +
					'<th class="actions">Actions</th>' +
				'</tr></thead><tbody></tbody>');
			add_tz_marker(this_.elem.find('thead th.added'));
		}

		for (var x in data) {
			x = data[x];
			this_.query_suffix = '/<' + x.id;

			var row = $('<tr>');
			row.append($('<td>', {text: x.id}));
			row.append($('<td>', {text: x.type}));
			row.append($('<td>', {text: x.priority}));

			var avb = a_view_button('/jobs/' + x.id, x.added, undefined,
				view_job.bind(null, true, x.id));
			avb.setAttribute('datetime', x.added);
			row.append($('<td>', {
				html: normalize_datetime(avb, false)
			}));
			row.append($('<td>', {
				class: 'status ' + x.status.class,
				text: x.status.text
			}));
			row.append($('<td>', {
				html: x.creator_id === null ? 'System' : (x.creator_username == null ? x.creator_id
					: a_view_button('/u/' + x.creator_id, x.creator_username, undefined,
						view_user.bind(null, true, x.creator_id)))
			}));
			// Info
			var info = x.info;
			{
				/* jshint loopfunc: true */
				var td = $('<td>');
				var div = $('<div>', {
					class: 'info'
				}).appendTo(td);
				var append_tag = function(name, val) {
					div.append($('<label>', {text: name}));
					div.append(val);
				};

				if (info.submission !== undefined)
					append_tag('submission',
						a_view_button('/s/' + info.submission,
							info.submission, undefined,
							view_submission.bind(null, true, info.submission)));

				if (info.user !== undefined)
					append_tag('user',
						a_view_button('/u/' + info.user,
							info.user, undefined,
							view_user.bind(null, true, info.user)));

				if (info["deleted user"] !== undefined)
					append_tag('deleted user',
						a_view_button('/u/' + info["deleted user"],
							info["deleted user"], undefined,
							view_user.bind(null, true, info["deleted user"])));

				if (info["target user"] !== undefined)
					append_tag('target user',
						a_view_button('/u/' + info["target user"],
							info["target user"], undefined,
							view_user.bind(null, true, info["target user"])));

				if (info.problem !== undefined)
					append_tag('problem',
						a_view_button('/p/' + info.problem,
							info.problem, undefined,
							view_problem.bind(null, true, info.problem)));

				if (info['deleted problem'] !== undefined)
					append_tag('deleted problem',
						a_view_button('/c/p' + info['deleted problem'],
							info['deleted problem'], undefined,
							view_problem.bind(null, true, info['deleted problem'])));

				if (info['target problem'] !== undefined)
					append_tag('target problem',
						a_view_button('/c/p' + info['target problem'],
							info['target problem'], undefined,
							view_problem.bind(null, true, info['target problem'])));

				if (info.contest !== undefined)
					append_tag('contest',
						a_view_button('/c/c' + info.contest,
							info.contest, undefined,
							view_contest.bind(null, true, info.contest)));

				if (info['contest problem'] !== undefined)
					append_tag('contest problem',
						a_view_button('/c/p' + info['contest problem'],
							info['contest problem'], undefined,
							view_contest_problem.bind(null, true, info['contest problem'])));

				if (info['contest round'] !== undefined)
					append_tag('contest round',
						a_view_button('/c/r' + info['contest round'],
							info['contest round'], undefined,
							view_contest_round.bind(null, true, info['contest round'])));

				var names = ['name', 'memory limit', 'problem type'];
				for (var idx in names) {
					var name = names[idx];
					if (info[name] !== undefined)
						append_tag(name, info[name]);
				}

				row.append(td);
			}

			// Actions
			row.append($('<td>', {
				html: ActionsToHTML.job(x.id, x.actions, info.problem)
			}));

			this_.elem.children('tbody').append(row);
		}
	};

	this.fetch_more();
}
function tab_jobs_lister(parent_elem, query_suffix /*= ''*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	parent_elem = $(parent_elem);
	function retab(tab_qsuff) {
		var table = $('<table class="jobs"></table>').appendTo(parent_elem);
		new JobsLister(table, query_suffix + tab_qsuff).monitor_scroll();
	}

	var tabs = [
		'All', retab.bind(null, ''),
		'My', retab.bind(null, '/u' + logged_user_id())
	];

	tabmenu(default_tabmenu_attacher.bind(parent_elem), tabs);
}

/* ============================== Submissions ============================== */
function add_submission_impl(as_modal, url, api_url, problem_field_elem, maybe_ignored, ignore_by_default, no_modal_elem) {
	view_base(as_modal, url, function() {
		this.append(ajax_form('Submit a solution', api_url,
			Form.field_group('Problem', problem_field_elem
			).add(Form.field_group('Language',
				$('<select>', {
					name: 'language',
					required: true,
					html: $('<option>', {
						value: 'c11',
						text: 'C11'
					}).add('<option>', {
						value: 'cpp11',
						text: 'C++11',
					}).add('<option>', {
						value: 'cpp14',
						text: 'C++14'
					}).add('<option>', {
						value: 'cpp17',
						text: 'C++17',
						selected: true
					}).add('<option>', {
						value: 'pascal',
						text: 'PASCAL'
					})
				})
			)).add(Form.field_group('Solution', {
				type: 'file',
				name: 'solution',
			})).add(Form.field_group('Code',
				$('<textarea>', {
					class: 'monospace',
					name: 'code',
					rows: 8,
					cols: 50
				})
			)).add(maybe_ignored ? Form.field_group('Ignored submission', {
				type: 'checkbox',
				name: 'ignored',
				checked: ignore_by_default
			}) : $()).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Submit'
				})
			}), function(resp) {
				if (as_modal) {
					show_success_via_loader($(this)[0], 'Submitted');
					view_submission(true, resp);
				} else {
					this.parent().remove();
					window.location.href = '/s/' + resp;
				}
			})
		);
	}, no_modal_elem);
}
function add_problem_submission(as_modal, problem, no_modal_elem /*= undefined*/) {
	if (problem.name === undefined) { // Request server for all needed data
		old_view_ajax(as_modal, "/api/problems/=" + problem.id, function(data) {
			if (data.length === 0)
				return show_error_via_loader(this, {
					status: '404',
					statusText: 'Not Found'
				});

			if (as_modal)
				close_modal($(this).closest('.modal'));

			add_problem_submission(as_modal, data[0]);
		}, '/p/' + problem.id + '/submit');
		return;
	}

	add_submission_impl(as_modal, '/p/' + problem.id + '/submit',
		'/api/submission/add/p' + problem.id,
		a_view_button('/p/' + problem.id, problem.name, undefined, view_problem.bind(null, true, problem.id)),
		(problem.actions.indexOf('i') !== -1), false,
		no_modal_elem);
}
function add_contest_submission(as_modal, contest, round, problem, no_modal_elem /*= undefined*/) {
	if (contest === undefined) { // Request server for all needed data
		old_view_ajax(as_modal, "/api/contest/p" + problem.id, function(data) {
			if (as_modal)
				close_modal($(this).closest('.modal'));

			add_contest_submission(as_modal, data.contest, data.rounds[0], data.problems[0]);
		}, '/c/p' + problem.id + '/submit');
		return;
	}

	add_submission_impl(as_modal, '/c/p' + problem.id + '/submit',
		'/api/submission/add/p' + problem.problem_id + '/cp' + problem.id,
		$('<div>', {
			class: 'contest-path',
			html: [
				a_view_button('/c/c' + contest.id, contest.name, '',
					view_contest.bind(null, true, contest.id)),
				' / ',
				a_view_button('/c/r' + round.id, round.name, '',
					view_contest_round.bind(null, true, round.id)),
				' / ',
				a_view_button('/c/p' + problem.id, problem.name, '',
					view_contest_problem.bind(null, true, problem.id))
			]
		}), (contest.actions.indexOf('A') !== -1),
		(contest.actions.indexOf('A') !== -1), no_modal_elem);
}

function rejudge_submission(submission_id) {
	modal_request('Scheduling submission rejudge ' + submission_id, $('<form>'),
		'/api/submission/' + submission_id + '/rejudge',
		'The rejudge has been scheduled.');
}
function submission_chtype(submission_id, submission_type) {
	dialogue_modal_request('Change submission type', $('<form>', {
		html: [
			$('<label>', {
				html: [
					'New type of the ',
					a_view_button('/s/' + submission_id, 'submission ' + submission_id,
						undefined, view_submission.bind(null, true, submission_id)),
					': '
				]
			}),
			$('<select>', {
				style: 'margin-left: 4px',
				name: 'type',
				html: [
					$('<option>', {
						value: 'N',
						text: 'Normal / final',
						selected: (submission_type !== 'Ignored' ? true : undefined)
					}),
					$('<option>', {
						value: 'I',
						text: 'Ignored',
						selected: (submission_type === 'Ignored' ? true : undefined)
					})
				]
			})
		]
	}), 'Change type', 'btn-small orange', '/api/submission/' + submission_id + '/chtype',
		'Type of the submission has been updated.', 'No, go back');
}
function delete_submission(submission_id) {
	dialogue_modal_request('Delete submission', $('<label>', {
			html: [
				'Are you sure to delete the ',
				a_view_button('/s/' + submission_id, 'submission ' + submission_id,
					undefined, view_submission.bind(null, true, submission_id)),
				'?'
			]
		}), 'Yes, delete it', 'btn-small red', '/api/submission/' + submission_id + '/delete',
		'The submission has been deleted.', 'No, go back');
}
function view_submission(as_modal, submission_id, opt_hash /*= ''*/) {
	old_view_ajax(as_modal, '/api/submissions/=' + submission_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		var s = data[0];

		if (s.contest_id !== null)
			this.append($('<div>', {
				class: 'contest-path',
				html: [
					a_view_button('/c/c' + s.contest_id, s.contest_name, '',
						view_contest.bind(null, true, s.contest_id)),
					' / ',
					a_view_button('/c/r' + s.contest_round_id, s.contest_round_name, '',
						view_contest_round.bind(null, true, s.contest_round_id)),
					' / ',
					a_view_button('/c/p' + s.contest_problem_id, s.contest_problem_name, '',
						view_contest_problem.bind(null, true, s.contest_problem_id)),
					$('<a>', {
						class: 'btn-small',
						href: '/api/download/statement/contest/p' + s.contest_problem_id + '/' +
							encodeURIComponent(s.contest_problem_name),
						text: 'View statement'
					})
				]
			}));

		this.append($('<div>', {
			class: 'submission-info',
			html: [
				$('<div>', {
					class: 'header',
					html: [
						$('<h1>', {
							text: 'Submission ' + submission_id
						}),
						$('<div>', {
							html: ActionsToHTML.submission(submission_id, s.actions,
								s.type, true)
						})
					]
				}),
				$('<table>', {
					html: [
						$('<thead>', {html: '<tr>' +
							'<th style="min-width:90px">Lang</th>' +
							(s.owner_id === null ? ''
								: '<th style="min-width:120px">User</th>') +
							'<th style="min-width:120px">Problem</th>' +
							'<th style="min-width:150px">Submission time</th>' +
							'<th style="min-width:150px">Status</th>' +
							'<th style="min-width:90px">Score</th>' +
							'<th style="min-width:90px">Type</th>' +
							'</tr>'
						}),
						$('<tbody>', {
							html: $('<tr>', {
								class: (s.type === 'Ignored' ? 'ignored' : undefined),
								html: [
									$('<td>', {text: s.language}),
									(s.owner_id === null ? '' : $('<td>', {
										html: [a_view_button('/u/' + s.owner_id, s.owner_username,
												undefined, view_user.bind(null, true, s.owner_id)),
											' (' + text_to_safe_html(s.owner_first_name) + ' ' +
												text_to_safe_html(s.owner_last_name) + ')'
										]
									})),
									$('<td>', {
										html: a_view_button('/p/' + s.problem_id, s.problem_name,
											undefined, view_problem.bind(null, true, s.problem_id))
									}),
									normalize_datetime($('<td>', {
										datetime: s.submit_time,
										text: s.submit_time
									}), true),
									$('<td>', {
										class: 'status ' + s.status.class,
										text: (s.status.class.lastIndexOf('initial') === -1 ?
											'' : 'Initial: ') + s.status.text
									}),
									$('<td>', {text: s.score}),
									$('<td>', {text: s.type})

								]
							})
						})
					]
				})
			]
		}));

		var elem = $(this);
		var tabs = [
			'Reports', function() {
				timed_hide_show(elem.parents('.modal'));
				elem.append($('<div>', {
					class: 'results',
					html: function() {
						var res = [];
						if (s.initial_report != '') {
							res.push($('<h2>', {text: 'Initial judgment protocol'}),
								s.initial_report);
						}
						if (s.final_report != '') {
							res.push($('<h2>', {text: 'Full judgment protocol'}),
								s.final_report);
						}
						if (s.final_report === null) {
							var message_to_show;
							if (utcdt_or_tm_to_Date(s.full_results) === Infinity) {
								message_to_show = $('<p>', {text: 'Full judgment protocol (of all tests) will not be available.'});
							} else {
								message_to_show = $('<p>', {text: 'Full judgment protocol (of all tests) will be visible since: '}).append(normalize_datetime($('<span>', {datetime: s.full_results})));
							}
							res.push(message_to_show);
						}
						return res;
					}()
				}));
			}
		];

		var cached_source;
		if (s.actions.indexOf('s') !== -1)
			tabs.push('Source', function() {
				timed_hide_show(elem.parents('.modal'));
				if (cached_source !== undefined) {
					elem.append(copy_to_clipboard_btn(false, 'Copy to clipboard', function() {
						return $(cached_source).text();
					}));
					elem.append(cached_source);
				} else {
					append_loader(elem[0]);
					$.ajax({
						url: '/api/submission/' + submission_id + '/source',
						type: 'POST',
						processData: false,
						contentType: false,
						data: new FormData(add_csrf_token_to($('<form>')).get(0)),
						dataType: 'html',
						success: function(data) {
							cached_source = data;
							elem.append(copy_to_clipboard_btn(false, 'Copy to clipboard', function() {
								return $(cached_source).text();
							}));
							elem.append(cached_source);
							remove_loader(elem[0]);
							centerize_modal(elem.closest('.modal'), false);
						},
						error: function(resp, status) {
							show_error_via_loader(elem, resp, status);
						}
					});
				}
			});

		if (s.actions.indexOf('j') !== -1)
			tabs.push('Related jobs', function() {
				elem.append($('<table>', {class: 'jobs'}));
				new JobsLister(elem.children().last(), '/s' + submission_id).monitor_scroll();
			});

		if (s.owner_id !== null) {
			tabs.push((s.owner_id == logged_user_id() ? 'My' : "User's") + ' submissions to this problem', function() {
				elem.append($('<div>'));
				tab_submissions_lister(elem.children().last(), '/u' + s.owner_id + (s.contest_id === null ? '/p' + s.problem_id : '/P' + s.contest_problem_id));
			});

			if (s.contest_id !== null && s.owner_id != logged_user_id()) {
				tabs.push("User's submissions to this round", function() {
					elem.append($('<div>'));
					tab_submissions_lister(elem.children().last(), '/u' + s.owner_id + '/R' + s.contest_round_id);
				}, "User's submissions to this contest", function() {
					elem.append($('<div>'));
					tab_submissions_lister(elem.children().last(), '/u' + s.owner_id + '/C' + s.contest_id);
				});
			}
		}

		tabmenu(default_tabmenu_attacher.bind(elem), tabs);

	}, '/s/' + submission_id + (opt_hash === undefined ? '' : opt_hash), undefined, false);
}
function SubmissionsLister(elem, query_suffix /*= ''*/, show_submission /*= function(){return true;}*/) {
	var this_ = this;
	if (query_suffix === undefined)
		query_suffix = '';
	if (show_submission === undefined)
		show_submission = function() { return true; };

	this.show_user = (query_suffix.indexOf('/u') === -1 &&
		query_suffix.indexOf('/tS') === -1);

	this.show_contest = (query_suffix.indexOf('/C') === -1 &&
		query_suffix.indexOf('/R') === -1 &&
		query_suffix.indexOf('/P') === -1);

	OldLister.call(this, elem);
	this.query_url = '/api/submissions' + query_suffix;
	this.query_suffix = '';

	this.process_api_response = function(data, modal) {
		var thead = this_.elem[0].querySelector('thead');
		if (thead === null) {
			if (data.length === 0) {
				var center = document.createElement('center');
				center.className = 'submissions always_in_view';
				center.innerHTML = '<p>There are no submissions to show...</p>';
				this.elem[0].parentNode.appendChild(center);
				return;
			}

			this.elem[0].innerHTML = '<thead><tr>' +
					'<th>Id</th>' +
					'<th>Lang</th>' +
					(this_.show_user ? '<th class="user">User</th>' : '') +
					'<th class="time">Added</th>' +
					'<th class="problem">Problem</th>' +
					'<th class="status">Status</th>' +
					'<th class="score">Score</th>' +
					'<th class="type">Type</th>' +
					'<th class="actions">Actions</th>' +
				'</tr></thead><tbody></tbody>';
			add_tz_marker(this_.elem[0].querySelector('thead th.time'));
		}

		for (var x in data) {
			x = data[x];
			this_.query_suffix = '/<' + x.id;

			if (!show_submission(x))
				continue;

			var row = document.createElement('tr');
			var td;
			if (x.type === 'Ignored')
				row.className = 'ignored';

			row.appendChild(elem_with_text('td', x.id));
			row.appendChild(elem_with_text('td', x.language));

			// Username
			if (this_.show_user) {
				if (x.owner_id === null)
					row.appendChild(elem_with_text('td', 'System'));
				else if (x.owner_first_name === null || x.owner_last_name === null)
					row.appendChild(elem_with_text('td', x.owner_id));
				else {
					td = document.createElement('td');
					td.appendChild(a_view_button('/u/' + x.owner_id, x.owner_first_name + ' ' + x.owner_last_name, undefined,
							view_user.bind(null, true, x.owner_id)));
					row.appendChild(td);
				}

			}

			// Submission time
			td = document.createElement('td');
			var avb = a_view_button('/s/' + x.id, x.submit_time, undefined,
				view_submission.bind(null, true, x.id));
			avb.setAttribute('datetime', x.submit_time);
			td.appendChild(normalize_datetime(avb, false)[0]);
			row.appendChild(td);

			// Problem
			if (x.contest_id == null) { // Not in the contest
				td = document.createElement('td');
				td.appendChild(a_view_button('/p/' + x.problem_id, x.problem_name, undefined,
					view_problem.bind(null, true, x.problem_id)));
			} else {
				td = document.createElement('td');
				// Contest
				if (this_.show_contest) {
					td.appendChild(a_view_button('/c/c' + x.contest_id, x.contest_name,
						undefined, view_contest.bind(null, true, x.contest_id)));
					td.appendChild(document.createTextNode(' / '));
				}
				td.appendChild(a_view_button('/c/r' + x.contest_round_id,
					x.contest_round_name, undefined,
					view_contest_round.bind(null, true, x.contest_round_id)));
				td.appendChild(document.createTextNode(' / '));
				td.appendChild(a_view_button('/c/p' + x.contest_problem_id,
					x.contest_problem_name, undefined,
					view_contest_problem.bind(null, true, x.contest_problem_id)));
			}
			row.appendChild(td);

			// Status
			td = document.createElement('td');
			td.className = 'status ' + x.status.class;
			td.innerText = (x.status.class.lastIndexOf('initial') === -1 ? ''
					: 'Initial: ') + x.status.text;
			row.appendChild(td);

			// Score
			row.appendChild(elem_with_text('td', x.score));

			// Type
			row.appendChild(elem_with_text('td', x.type));

			// Actions
			td = document.createElement('td');
			append_children(td, ActionsToHTML.submission(x.id, x.actions, x.type));
			row.appendChild(td);

			this_.elem[0].querySelector('tbody').appendChild(row);
		}
	};

	this.fetch_more();
}
function tab_submissions_lister(parent_elem, query_suffix /*= ''*/, show_solutions_tab /* = false*/, filter_normal_instead_of_querying_final /*= false*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	parent_elem = $(parent_elem);
	function retab(tab_qsuff, show_submission) {
		var table = $('<table class="submissions"></table>').appendTo(parent_elem);
		new SubmissionsLister(table, query_suffix + tab_qsuff, show_submission).monitor_scroll();
	}

	var final_binding = (filter_normal_instead_of_querying_final ?
		retab.bind(null, '', function(s) {
			return (s.type === 'Final' || s.type === 'Initial final');
		}) : retab.bind(null, '/tF'));

	var tabs = [
		'All', retab.bind(null, ''),
		'Final', final_binding,
		'Ignored', retab.bind(null, '/tI')
	];
	if (show_solutions_tab)
		tabs.push('Solutions', retab.bind(null, '/tS'));

	tabmenu(default_tabmenu_attacher.bind(parent_elem), tabs);
}

/* ================================ Problems ================================ */
function add_problem(as_modal) {
	view_base(as_modal, '/p/add', function() {
		this.append(ajax_form('Add problem', '/api/problem/add',
			Form.field_group("Problem's name", {
				type: 'text',
				name: 'name',
				size: 25,
				// maxlength: 'TODO...',
				placeholder: 'Take from Simfile',
			}).add(Form.field_group("Problem's label", {
				type: 'text',
				name: 'label',
				size: 25,
				// maxlength: 'TODO...',
				placeholder: 'Take from Simfile or make from name',
			})).add(Form.field_group("Problem's type",
				$('<select>', {
					name: 'type',
					required: true,
					html: $('<option>', {
						value: 'PUB',
						text: 'Public',
					}).add('<option>', {
						value: 'PRI',
						text: 'Private',
						selected: true
					}).add('<option>', {
						value: 'CON',
						text: 'Contest only',
					})
				})
			)).add(Form.field_group('Memory limit [MiB]', {
				type: 'text',
				name: 'mem_limit',
				size: 25,
				// maxlength: 'TODO...',
				trim_before_send: true,
				placeholder: 'Take from Simfile',
			})).add(Form.field_group('Global time limit [s] (for each test)', {
				type: 'text',
				name: 'global_time_limit',
				size: 25,
				// maxlength: 'TODO...',
				trim_before_send: true,
				placeholder: 'No global time limit',
			})).add(Form.field_group('Reset time limits using model solution', {
				type: 'checkbox',
				name: 'reset_time_limits',
				checked: true
			})).add(Form.field_group('Seek for new tests', {
				type: 'checkbox',
				name: 'seek_for_new_tests',
				checked: true
			})).add(Form.field_group('Reset scoring', {
				type: 'checkbox',
				name: 'reset_scoring'
			})).add(Form.field_group('Ignore Simfile', {
				type: 'checkbox',
				name: 'ignore_simfile',
			})).add(Form.field_group('Zipped package', {
				type: 'file',
				name: 'package',
				required: true
			})).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Submit'
				})
			}), function(resp) {
				if (as_modal) {
					show_success_via_loader($(this)[0], 'Added');
					view_job(true, resp);
				} else {
					this.parent().remove();
					window.location.href = '/jobs/' + resp;
				}
			}, 'add-problem')
		);
	});
}
function append_reupload_problem(elem, as_modal, problem) {
	elem.append(ajax_form('Reupload problem', '/api/problem/' + problem.id + '/reupload',
		Form.field_group("Problem's name", {
			type: 'text',
			name: 'name',
			value: problem.name,
			size: 25,
			// maxlength: 'TODO...',
			placeholder: 'Take from Simfile',
		}).add(Form.field_group("Problem's label", {
			type: 'text',
			name: 'label',
			value: problem.label,
			size: 25,
			// maxlength: 'TODO...',
			placeholder: 'Take from Simfile or make from name',
		})).add(Form.field_group("Problem's type",
			$('<select>', {
				name: 'type',
				required: true,
				html: $('<option>', {
					value: 'PUB',
					text: 'Public',
					selected: ('Public' == problem.type ? true : undefined)
				}).add('<option>', {
					value: 'PRI',
					text: 'Private',
					selected: ('Private' == problem.type ? true : undefined)
				}).add('<option>', {
					value: 'CON',
					text: 'Contest only',
					selected: ('Contest only' == problem.type ? true : undefined)
				})
			})
		)).add(Form.field_group('Memory limit [MiB]', {
			type: 'text',
			name: 'mem_limit',
			value: problem.memory_limit,
			size: 25,
			// maxlength: 'TODO...',
			trim_before_send: true,
			placeholder: 'Take from Simfile',
		})).add(Form.field_group('Global time limit [s] (for each test)', {
			type: 'text',
			name: 'global_time_limit',
			size: 25,
			// maxlength: 'TODO...',
			trim_before_send: true,
			placeholder: 'No global time limit',
		})).add(Form.field_group('Reset time limits using model solution', {
			type: 'checkbox',
			name: 'reset_time_limits',
			checked: true
		})).add(Form.field_group('Seek for new tests', {
			type: 'checkbox',
			name: 'seek_for_new_tests',
			checked: true
		})).add(Form.field_group('Reset scoring', {
			type: 'checkbox',
			name: 'reset_scoring'
		})).add(Form.field_group('Ignore Simfile', {
			type: 'checkbox',
			name: 'ignore_simfile',
		})).add(Form.field_group('Zipped package', {
			type: 'file',
			name: 'package',
			required: true
		})).add('<div>', {
			html: $('<input>', {
				class: 'btn blue',
				type: 'submit',
				value: 'Submit'
			})
		}), function(resp) {
			if (as_modal) {
				show_success_via_loader($(this)[0], 'Reuploaded');
				view_job(true, resp);
			} else {
				this.parent().remove();
				window.location.href = '/jobs/' + resp;
			}
		}, 'reupload-problem')
	);
}
function reupload_problem(as_modal, problem_id) {
	old_view_ajax(as_modal, '/api/problems/=' + problem_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		problem = data[0];
		var actions = problem.actions;
		if (actions.indexOf('R') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		append_reupload_problem(this, as_modal, problem);

	}, '/p/' + problem_id + '/reupload');
}
function append_problem_tags(elem, problem_id, problem_tags) {
	elem = $(elem);

	var list_tags = function(hidden) {
		var tags = (hidden ? problem_tags.hidden : problem_tags.public);
		var tbody = $('<tbody>');

		var name_input = $('<input>', {
				type: 'text',
				name: 'name',
				size: 24,
				// maxlength: 'TODO...'
				autofocus: true,
				required: true
		});

		var add_form = [
			Form.field_group('Name', name_input),
			$('<input>', {
				type: 'hidden',
				name: 'hidden',
				value: hidden
			}),
			$('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Add tag'
				})
			})
		];

		// Add button
		elem.append($('<center>', {html: [
			$('<table>', {class: 'tags-edit'}).append($('<thead>', {
				html: '<tr>' +
					'<th class="name">Name</th>' +
					'<th class="actions">Actions</th>' +
				'</tr>'
			})).append(tbody),
			a_view_button(undefined, 'Add tag', 'btn', function() {
				modal(ajax_form('Add tag',
					'/api/problem/' + problem_id + '/edit/tags/add_tag',
					add_form, function() {
						show_success_via_loader($(this)[0], 'The tag has been added.');
						// Add tag
						var input = add_form[0].children('input');
						tags.push(input.val());
						make_row(input.val()).appendTo(tbody);
						input.val('');
						// Close modal
						var modal = $(this).closest('.modal');
						modal.fadeOut(150, close_modal.bind(null, modal));
					}));

				// Focus tag name
				name_input.focus();
			})
		]}));

		var make_row = function(tag) {
			var name_input = $('<input>', {
				type: 'text',
				name: 'name',
				size: 24,
				value: tag,
				// maxlength: 'TODO...'
				required: true
			});

			var edit_form = [
				Form.field_group('Name', name_input),
				$('<input>', {
					type: 'hidden',
					name: 'old_name',
					value: tag
				}),
				$('<input>', {
					type: 'hidden',
					name: 'hidden',
					value: hidden
				}),
				$('<div>', {
					html: $('<input>', {
						class: 'btn blue',
						type: 'submit',
						value: 'Edit'
					})
				})
			];

			var delete_from = $('<form>', {html: [
				$('<input>', {
					type: 'hidden',
					name: 'name',
					value: tag
				}),
				$('<input>', {
					type: 'hidden',
					name: 'hidden',
					value: hidden
				})
			]}).add('<label>', {html: [
				'Are you sure to delete the tag: ',
				$('<label>', {
					class: 'problem-tag' + (hidden ? ' hidden' : ''),
					text: tag
				}),
				'?'
			]});

			var row = $('<tr>', {html: [
				$('<td>', {text: tag}),
				$('<td>', {html: [
					a_view_button(undefined, 'Edit', 'btn-small blue', function() {
						edit_form[0].children('input').val(tag); // If one opened edit, typed something and closed edit, it would appear in the next edit without this change
						modal(ajax_form('Edit tag',
							'/api/problem/' + problem_id + '/edit/tags/edit_tag',
							edit_form, function() {
								show_success_via_loader($(this)[0], 'done.');
								// Update tag
								var old_tag = tag;
								tag = edit_form[0].children('input').val();
								tags[tags.indexOf(old_tag)] = tag;
								edit_form[1].children('input').val(tag);

								delete_from.find('label').text(tag);
								delete_from.find('input').val(tag);

								row.children().eq(0).text(tag).css({
									'background-color': '#a3ffa3',
									transition: 'background-color 0s cubic-bezier(0.55, 0.06, 0.68, 0.19)'
								});
								// Without this it does not change to green immediately
								setTimeout(function() {
									row.children().eq(0).css({
										'background-color': 'initial',
										transition: 'background-color 2s cubic-bezier(0.55, 0.06, 0.68, 0.19)'
									});
								}, 50);
								// Close modal
								var modal = $(this).closest('.modal');
								modal.fadeOut(150, close_modal.bind(null, modal));
							}));

						// Focus tag name
						name_input.focus();
					}),
					a_view_button(undefined, 'Delete', 'btn-small red',
						dialogue_modal_request.bind(null, 'Delete tag',
							delete_from, 'Yes, delete it', 'btn-small red',
							'/api/problem/' + problem_id + '/edit/tags/delete_tag',
							function(_, loader_parent) {
								show_success_via_loader(loader_parent[0], 'The tag has been deleted.');
								row.fadeOut(800);
								setTimeout(function() { row.remove(); }, 800);
								// Delete tag
								tags.splice(tags.indexOf(tag), 1);
								// Close modal
								var modal = $(this).closest('.modal');
								modal.fadeOut(250, close_modal.bind(null, modal));
							}, 'No, go back'))
				]})
			]});
			return row;
		};

		for (var x in tags)
			make_row(tags[x]).appendTo(tbody);
	};

	tabmenu(default_tabmenu_attacher.bind(elem), [
		'Public', list_tags.bind(null, false),
		'Hidden', list_tags.bind(null, true)
	]);
}
function append_change_problem_statement_form(elem, as_modal, problem_id) {
	$(elem).append(ajax_form('Change statement',
		'/api/problem/' + problem_id + '/change_statement',
		Form.field_group("New statement's path", {
			type: 'text',
			name: 'path',
			size: 24,
			placeholder: 'The same as the old statement path'
			// maxlength: 'TODO...'
		}).add(Form.field_group('File', {
			type: 'file',
			name: 'statement',
			required: true
		})).add('<div>', {
			html: $('<input>', {
				class: 'btn blue',
				type: 'submit',
				value: 'Change statement'
			})
		}), function(resp) {
			if (as_modal) {
				show_success_via_loader($(this)[0], 'Statement change was scheduled');
				view_job(true, resp);
			} else {
				this.parent().remove();
				window.location.href = '/jobs/' + resp;
			}
		}));
}
function change_problem_statement(as_modal, problem_id) {
	old_view_ajax(as_modal, '/api/problems/=' + problem_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
					status: '404',
					statusText: 'Not Found'
				});

		var problem = data[0];
		if (problem.actions.indexOf('C') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		append_change_problem_statement_form(this, problem_id);

	}, '/p/' + problem_id + '/change_statement');
}
function edit_problem(as_modal, problem_id, opt_hash) {
	old_view_ajax(as_modal, '/api/problems/=' + problem_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		var problem = data[0];
		var actions = problem.actions;

		if (actions.indexOf('E') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		var elem = $(this);
		elem.append($('<h1>', {
			class: 'align-center',
			html: [
				'Edit problem ', a_view_button('/p/' + problem_id, problem.name, '',
					view_problem.bind(null, true, problem_id))
			]
		}));

		var tabs = [
			'Edit tags', append_problem_tags.bind(null, elem, problem_id, problem.tags)
		];

		if (problem.actions.indexOf('C') !== -1) {
			tabs.push('Change statement',
				append_change_problem_statement_form.bind(null, elem, as_modal, problem_id));
		}

		if (problem.actions.indexOf('R') !== -1) {
			tabs.push('Reupload',
				append_reupload_problem.bind(null, elem, as_modal, problem));
		}

		tabmenu(default_tabmenu_attacher.bind(elem), tabs);

	}, '/p/' + problem_id + '/edit' + (opt_hash === undefined ? '' : opt_hash), undefined, false);
}
function delete_problem(as_modal, problem_id) {
	old_view_ajax(as_modal, '/api/problems/=' + problem_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
					status: '404',
					statusText: 'Not Found'
				});

		var problem = data[0];
		if (problem.actions.indexOf('D') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		delete_with_password_to_job(this, 'Delete problem',
			'/api/problem/' + problem_id + '/delete', [
				'You are going to delete the problem ',
				a_view_button('/p/' + problem.id, problem.name,
					undefined, view_problem.bind(null, true, problem.id)),
				' and all submissions to it. As this cannot be undone, you have to confirm this with your password.'
			], 'Delete problem');
	}, '/p/' + problem_id + '/delete');
}
function merge_problem(as_modal, problem_id) {
	old_view_ajax(as_modal, '/api/problems/=' + problem_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
					status: '404',
					statusText: 'Not Found'
				});

		var problem = data[0];
		if (problem.actions.indexOf('M') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		api_request_with_password_to_job(this, 'Merge into another problem',
			'/api/problem/' + problem_id + '/merge_into_another', [
				'The problem ',
				a_view_button('/p/' + problem.id, problem.name,
					undefined, view_problem.bind(null, true, problem.id)),
				' is going to be deleted. All its tags and submissions will be transfered to the target problem. Every contest problem that attaches this problem will be updated to attach the target problem.',
				'<br>',
				'Be aware that problem packages will not be merged - this behaves exactly as problem deletion but first transfers everything that uses this problem to use the target problem.',
				'<br>',
				'As this cannot be undone, you have to confirm this with your password.'
			], 'Merge problem', 'Merging has been scheduled.',
			Form.field_group('Target problem ID', {
				type: 'text',
				name: 'target_problem',
				size: 6,
				trim_before_send: true,
			}).add(Form.field_group('Rejudge transferred submissions', {
				type: 'checkbox',
				name: 'rejudge_transferred_submissions'
			})));
	}, '/p/' + problem_id + '/merge');
}
function rejudge_problem_submissions(problem_id, problem_name) {
	dialogue_modal_request("Rejudge all problem's submissions", $('<label>', {
			html: [
				'Are you sure to rejudge all submissions to the problem ',
				a_view_button('/p/' + problem_id, problem_name, undefined,
					view_problem.bind(null, true, problem_id)),
				'?'
			]
		}), 'Rejudge all', 'btn-small blue',
		'/api/problem/' + problem_id + '/rejudge_all_submissions',
		'The rejudge jobs has been scheduled.', 'No, go back', true);
}
function reset_problem_time_limits(as_modal, problem_id) {
	old_view_ajax(as_modal, '/api/problems/=' + problem_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		var problem = data[0];
		var actions = problem.actions;
		if (actions.indexOf('L') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Reset problem time limits using model solution', '/api/problem/' + problem_id + '/reset_time_limits',
			$('<p>', {
				style: 'margin: 0 0 12px; text-align: center',
				html: [
					'Do you really want to reset problem ',
					a_view_button('/p/' + problem_id, problem.name, undefined, view_problem.bind(null, true, problem_id)),
					' time limits using the model solution?'
				]
			}).add('<center>', {
				style: 'margin: 12px auto 0',
				html: $('<input>', {
					class: 'btn-small blue',
					type: 'submit',
					value: 'Reset time limits'
				}).add('<a>', {
					class: 'btn-small',
					href: (as_modal ? undefined : '/'),
					text: 'Go back',
					click: function() {
						var modal = $(this).closest('.modal');
						if (modal.length === 0)
							history.back();
						else
							close_modal(modal);
					}
				})
			}), function(resp, loader_parent) {
				if (as_modal) {
					show_success_via_loader($(this)[0], 'Reseting time limits has been scheduled.');
					view_job(true, resp);
				} else {
					this.parent().remove();
					window.location.href = '/jobs/' + resp;
				}
			}
		));
	}, '/p/' + problem_id + '/reset_time_limits');
}
function view_problem(as_modal, problem_id, opt_hash /*= ''*/) {
	old_view_ajax(as_modal, '/api/problems/=' + problem_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		var problem = data[0];
		var actions = problem.actions;

		this.append($('<div>', {
			class: 'header',
			html: $('<h1>', {
					text: problem.name
				}).add('<div>', {
					html: ActionsToHTML.problem(problem, true)
				})
		})).append($('<center>', {
			class: 'always_in_view',
			html: $('<div>', {
				class: 'problem-info',
				html: $('<div>', {
					class: 'type',
					html: $('<label>', {text: 'Type'}).add('<span>', {
						text: problem.type[0].toLowerCase() + problem.type.slice(1)
					})
				})
				.add($('<div>', {
					class: 'name',
					html: $('<label>', {text: 'Name'})
				}).append(text_to_safe_html(problem.name)))
				.add($('<div>', {
					class: 'label',
					html: $('<label>', {text: 'Label'})
				}).append(text_to_safe_html(problem.label)))
				.add($('<div>', {
					class: 'tags-row',
					html: $('<label>', {text: 'Tags'})
				}).append($('<div>', {
					class: 'tags',
					html: function() {
						var tags = [];
						for (var i in problem.tags.public)
							tags.push($('<label>', {text: problem.tags.public[i]}));
						for (i in problem.tags.hidden)
							tags.push($('<label>', {
								class: 'hidden',
								text: problem.tags.hidden[i]
							}));

						return tags;
					}()
				})))
			})
		}));

		if (actions.indexOf('o') !== -1)
			$(this).find('.problem-info').append($('<div>', {
					class: 'owner',
					html: [
						$('<label>', {text: 'Owner'}),
						(problem.owner_id === null ? '(Deleted)'
							: a_view_button('/u/' + problem.owner_id,
								problem.owner_username, undefined,
								view_user.bind(null, true, problem.owner_id)))
					]
				}));

		if (actions.indexOf('a') !== -1)
			$(this).find('.problem-info').append($('<div>', {
					class: 'added',
					html: $('<label>', {text: 'Added'})
				}).append(normalize_datetime($('<span>', {
					datetime: problem.added,
					text: problem.added
				}), true)));

		var elem = $(this);
		var tabs = [];
		if (actions.indexOf('s') !== -1)
			tabs.push('All submissions', function() {
					elem.append($('<div>'));
					tab_submissions_lister(elem.children().last(), '/p' + problem_id,
						true);
				});

		if (is_logged_in())
			tabs.push('My submissions', function() {
					elem.append($('<div>'));
					tab_submissions_lister(elem.children().last(), '/p' + problem_id + '/u' + logged_user_id());
				});

		if (actions.indexOf('f') !== -1)
			tabs.push('Simfile', function() {
				timed_hide_show(elem.parents('.modal'));
				elem.append($('<pre>', {
					class: 'simfile',
					style: 'text-align: initial',
					text: problem.simfile
				}));
			});

		if (actions.indexOf('j') !== -1)
			tabs.push('Related jobs', function() {
				var j_table = $('<table>', {
					class: 'jobs'
				});
				elem.append($('<div>', {
					html: j_table
				}));
				new JobsLister(j_table, '/p' + problem_id).monitor_scroll();
			});

		if (actions.indexOf('c') !== -1)
			tabs.push('Attaching contest problems', function() {
				var acp_table = $('<table>', {
					class: 'attaching-contest-problems stripped'
				});
				elem.append($('<div>', {
					html: acp_table
				}));
				new AttachingContestProblemsLister(acp_table, problem_id).monitor_scroll();
			});

		tabmenu(default_tabmenu_attacher.bind(elem), tabs);

	}, '/p/' + problem_id + (opt_hash === undefined ? '' : opt_hash), undefined, false);
}
function ProblemsLister(elem, query_suffix /*= ''*/) {
	var this_ = this;
	if (query_suffix === undefined)
		query_suffix = '';

	this.show_owner = logged_user_is_teacher_or_admin();
	this.show_added = logged_user_is_teacher_or_admin() ||
		(query_suffix.indexOf('/u') !== -1);

	OldLister.call(this, elem);
	this.query_url = '/api/problems' + query_suffix;
	this.query_suffix = '';

	this.process_api_response = function(data, modal) {
		if (this_.elem.children('thead').length === 0) {
			if (data.length === 0) {
				this_.elem.parent().append($('<center>', {
					class: 'problems always_in_view',
					// class: 'problems',
					html: '<p>There are no problems to show...</p>'
				}));
				remove_loader(this_.elem.parent()[0]);
				timed_hide_show(modal);
				return;
			}

			this_.elem.html('<thead><tr>' +
					'<th>Id</th>' +
					'<th class="type">Type</th>' +
					'<th class="label">Label</th>' +
					'<th class="name_and_tags">Name and tags</th>' +
					(this_.show_owner ? '<th class="owner">Owner</th>' : '') +
					(this_.show_added ? '<th class="added">Added</th>' : '') +
					'<th class="actions">Actions</th>' +
				'</tr></thead><tbody></tbody>');
			add_tz_marker(this_.elem.find('thead th.added'));
		}

		for (var x in data) {
			x = data[x];
			this_.query_suffix = '/<' + x.id;

			var row = $('<tr>');
			if (x.color_class !== null)
				row.addClass('status ' + x.color_class);

			// Id
			row.append($('<td>', {text: x.id}));
			// Type
			row.append($('<td>', {text: x.type}));
			// Label
			row.append($('<td>', {text: x.label}));

			// Name and tags
			var tags = [];
			for (var i in x.tags.public)
				tags.push($('<label>', {text: x.tags.public[i]}));
			for (i in x.tags.hidden)
				tags.push($('<label>', {
					class: 'hidden',
					text: x.tags.hidden[i]
				}));

			row.append($('<td>', {html: $('<div>', {
				class: 'name-and-tags',
				html: [
					$('<span>', {text: x.name}),
					$('<div>', {
						class: 'tags',
						html: tags
					})
				]
			})}));

			// Owner
			if (this_.show_owner)
				row.append($('<td>', {
					html: (x.owner_id === null ? '(Deleted)'
						: a_view_button('/u/' + x.owner_id, x.owner_username,
							undefined, view_user.bind(null, true, x.owner_id)))
				}));

			// Added
			if (this_.show_added)
				row.append(normalize_datetime($('<td>', {
					datetime: x.added,
					text: x.added
				})));

			// Actions
			row.append($('<td>', {
				html: ActionsToHTML.problem(x)
			}));

			this_.elem.children('tbody').append(row);
		}
	};

	this.fetch_more();
}
function tab_problems_lister(parent_elem, query_suffix /*= ''*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	function retab(tab_qsuff) {
		var table = $('<table class="problems stripped"></table>').appendTo(parent_elem);
		new ProblemsLister(table, query_suffix + tab_qsuff).monitor_scroll();
	}

	var tabs = [
		'All', retab.bind(null, '')
	];

	if (is_logged_in())
		tabs.push('My', retab.bind(null, '/u' + logged_user_id()));

	tabmenu(default_tabmenu_attacher.bind(parent_elem), tabs);
}
function problem_chooser(as_modal /*= true*/, opt_hash /*= ''*/) {
	view_base((as_modal === undefined ? true : as_modal),
		'/p' + (opt_hash === undefined ? '' : opt_hash), function() {
			timed_hide($(this).parent().parent().filter('.modal'));
			$(this).append($('<h1>', {text: 'Problems'}));
			if (logged_user_is_teacher_or_admin())
				$(this).append(a_view_button('/p/add', 'Add problem', 'btn',
					add_problem.bind(null, true)));

			tab_problems_lister($(this));
		});
}
function AttachingContestProblemsLister(elem, problem_id, query_suffix /*= ''*/) {
	var this_ = this;
	if (query_suffix === undefined)
		query_suffix = '';

	OldLister.call(this, elem);
	this.query_url = '/api/problem/' + problem_id + '/attaching_contest_problems' + query_suffix;
	this.query_suffix = '';

	this.process_api_response = function(data, modal) {
		if (this_.elem.children('thead').length === 0) {
			if (data.length === 0) {
				this_.elem.parent().append($('<center>', {
					class: 'attaching-contest-problems always_in_view',
					// class: 'attaching-contest-problems',
					html: '<p>There are no contest problems using (attaching) this problem...</p>'
				}));
				remove_loader(this_.elem.parent()[0]);
				timed_hide_show(modal);
				return;
			}

			this_.elem.html('<thead><tr>' +
					'<th>Contest problem</th>' +
				'</tr></thead><tbody></tbody>');
		}

		for (var x in data) {
			x = data[x];
			this_.query_suffix = '/<' + x.problem_id;

			var row = $('<tr>');
			row.append($('<td>', {
				html: [
					a_view_button('/c/c' + x.contest_id, x.contest_name, undefined,
						view_contest.bind(null, true, x.contest_id)),
					' / ',
					a_view_button('/c/r' + x.round_id, x.round_name, undefined,
						view_contest_round.bind(null, true, x.round_id)),
					' / ',
					a_view_button('/c/p' + x.problem_id, x.problem_name, undefined,
						view_contest_problem.bind(null, true, x.problem_id))
				]
			}));

			this_.elem.children('tbody').append(row);
		}
	};

	this.fetch_more();
}
/* ================================ Contests ================================ */
function append_create_contest(elem, as_modal) {
	elem.append(ajax_form('Create contest', '/api/contest/create',
		Form.field_group("Contest's name", {
			type: 'text',
			name: 'name',
			size: 24,
			// maxlength: 'TODO...',
			trim_before_send: true,
			required: true
		}).add(logged_user_is_admin() ? Form.field_group('Make public', {
			type: 'checkbox',
			name: 'public'
		}) : $()).add('<div>', {
			html: $('<input>', {
				class: 'btn blue',
				type: 'submit',
				value: 'Create'
			})
		}), function(resp) {
			if (as_modal) {
				show_success_via_loader($(this)[0], 'Created');
				view_contest(true, resp);
			} else {
				this.parent().remove();
				window.location.href = '/c/c' + resp;
			}
		})
	);
}
function append_clone_contest(elem, as_modal) {
	var source_contest_input = $('<input>', {
		type: 'hidden',
		name: 'source_contest'
	});

	elem.append(ajax_form('Clone contest', '/api/contest/clone',
		Form.field_group("Contest's name", {
			type: 'text',
			name: 'name',
			size: 24,
			// maxlength: 'TODO...',
			trim_before_send: true,
		}).add(source_contest_input
		).add(Form.field_group("Contest to clone (ID or URL)",
			$('<input>', {
				type: 'text',
				required: true
			}).on('input', function () {
				var val = $(this).val().trim();
				if (!isNaN(val)) {
					source_contest_input.val(val);
				} else {
					// Assume that input is an URL
					var match = val.match(/\/c(\d+)\b/);
					if (match != null)
						source_contest_input.val(match[1]);
					else
						source_contest_input.val(''); // unset on invalid value
				}
			})
		)).add(logged_user_is_admin() ? Form.field_group('Make public', {
			type: 'checkbox',
			name: 'public'
		}) : $()).add('<div>', {
			html: $('<input>', {
				class: 'btn blue',
				type: 'submit',
				value: 'Clone'
			})
		}), function(resp) {
			if (as_modal) {
				show_success_via_loader($(this)[0], 'Cloned');
				view_contest(true, resp);
			} else {
				this.parent().remove();
				window.location.href = '/c/c' + resp;
			}
		})
	);
}
function add_contest(as_modal, opt_hash /*= undefined*/) {
	view_base(as_modal, '/c/add' + (opt_hash === undefined ? '' : opt_hash), function() {
		this.append($('<h1>', {
			class: 'align-center',
			text: 'Add contest'
		}));

		tabmenu(default_tabmenu_attacher.bind(this), [
			'Create new', append_create_contest.bind(null, this, as_modal),
			'Clone existing', append_clone_contest.bind(null, this, as_modal),
		]);
	});
}
function append_create_contest_round(elem, as_modal, contest_id) {
	elem.append(ajax_form('Create contest round', '/api/contest/c' + contest_id + '/create_round',
		Form.field_group("Round's name", {
			type: 'text',
			name: 'name',
			size: 25,
			// maxlength: 'TODO...',
			trim_before_send: true,
			required: true
		}).add(Form.field_group('Begin time',
			dt_chooser_input('begins', true, true, true, undefined, 'The Big Bang', 'Never')
		)).add(Form.field_group('End time',
			dt_chooser_input('ends', true, true, true, Infinity, 'The Big Bang', 'Never')
		)).add(Form.field_group('Full results time',
			dt_chooser_input('full_results', true, true, true, -Infinity, 'Immediately', 'Never')
		)).add(Form.field_group('Show ranking since',
			dt_chooser_input('ranking_expo', true, true, true, -Infinity, 'The Big Bang', "Don't show")
		)).add('<div>', {
			html: $('<input>', {
				class: 'btn blue',
				type: 'submit',
				value: 'Create'
			})
		}), function(resp) {
			if (as_modal) {
				show_success_via_loader($(this)[0], 'Created');
				view_contest_round(true, resp);
			} else {
				this.parent().remove();
				window.location.href = '/c/r' + resp;
			}
		})
	);
}
function append_clone_contest_round(elem, as_modal, contest_id) {
	var source_contest_round_input = $('<input>', {
		type: 'hidden',
		name: 'source_contest_round'
	});

	elem.append(ajax_form('Clone contest round', '/api/contest/c' + contest_id + '/clone_round',
		Form.field_group("Round's name", {
			type: 'text',
			name: 'name',
			size: 25,
			// maxlength: 'TODO...',
			trim_before_send: true,
			placeholder: "The same as name of the cloned round"
		}).add(source_contest_round_input
		).add(Form.field_group("Contest round to clone (ID or URL)",
			$('<input>', {
				type: 'text',
				required: true
			}).on('input', function () {
				var val = $(this).val().trim();
				if (!isNaN(val)) {
					source_contest_round_input.val(val);
				} else {
					// Assume that input is an URL
					var match = val.match(/\/r(\d+)\b/);
					if (match != null)
						source_contest_round_input.val(match[1]);
					else
						source_contest_round_input.val(''); // unset on invalid value
				}
			})
		)).add(Form.field_group('Begin time',
			dt_chooser_input('begins', true, true, true, undefined, 'The Big Bang', 'Never')
		)).add(Form.field_group('End time',
			dt_chooser_input('ends', true, true, true, Infinity, 'The Big Bang', 'Never')
		)).add(Form.field_group('Full results time',
			dt_chooser_input('full_results', true, true, true, -Infinity, 'Immediately', 'Never')
		)).add(Form.field_group('Show ranking since',
			dt_chooser_input('ranking_expo', true, true, true, -Infinity, 'The Big Bang', "Don't show")
		)).add('<div>', {
			html: $('<input>', {
				class: 'btn blue',
				type: 'submit',
				value: 'Clone'
			})
		}), function(resp) {
			if (as_modal) {
				show_success_via_loader($(this)[0], 'Cloned');
				view_contest_round(true, resp);
			} else {
				this.parent().remove();
				window.location.href = '/c/r' + resp;
			}
		})
	);
}
function add_contest_round(as_modal, contest_id, opt_hash /*= undefined*/) {
	view_base(as_modal, '/c/c' + contest_id + '/add_round' + (opt_hash === undefined ? '' : opt_hash), function() {
		this.append($('<h1>', {
			class: 'align-center',
			text: 'Add contest round'
		}));

		tabmenu(default_tabmenu_attacher.bind(this), [
			'Create new', append_create_contest_round.bind(null, this, as_modal, contest_id),
			'Clone existing', append_clone_contest_round.bind(null, this, as_modal, contest_id),
		]);
	});
}
function add_contest_problem(as_modal, contest_round_id) {
	view_base(as_modal, '/c/r' + contest_round_id + '/attach_problem', function() {
		this.append(ajax_form('Attach problem', '/api/contest/r' + contest_round_id + '/attach_problem',
			Form.field_group("Problem's name", {
				type: 'text',
				name: 'name',
				size: 25,
				// maxlength: 'TODO...',
				trim_before_send: true,
				placeholder: 'The same as in Problems',
			}).add(Form.field_group('Problem ID', {
				type: 'text',
				name: 'problem_id',
				size: 6,
				// maxlength: 'TODO...',
				trim_before_send: true,
				required: true
			}).append(a_view_button('/p', 'Search problems', '', problem_chooser))
			).add(Form.field_group('Final submission to select',
				$('<select>', {
					name: 'method_of_choosing_final_submission',
					required: true,
					html: $('<option>', {
						value: 'latest_compiling',
						text: 'Latest compiling'
					}).add('<option>', {
						value: 'highest_score',
						text: 'The one with the highest score',
						selected: true
					})
				})
			)).add(Form.field_group('Score revealing',
				$('<select>', {
					name: 'score_revealing',
					required: true,
					html: $('<option>', {
						value: 'none',
						text: 'None',
						selected: true
					}).add('<option>', {
						value: 'only_score',
						text: 'Only score'
					}).add('<option>', {
						value: 'score_and_full_status',
						text: 'Score and full status'
					})
				})
			)).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Add'
				})
			}), function(resp) {
				if (as_modal) {
					show_success_via_loader($(this)[0], 'Added');
					view_contest_problem(true, resp);
				} else {
					this.parent().remove();
					window.location.href = '/c/p' + resp;
				}
			})
		);
	});
}
function edit_contest(as_modal, contest_id) {
	old_view_ajax(as_modal, '/api/contests/=' + contest_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		data = data[0];

		var actions = data.actions;
		if (actions.indexOf('A') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		var make_submitters_contestants = Form.field_group("Add users who submitted solutions as contestants", {
			type: 'checkbox',
			name: 'make_submitters_contestants',
			checked: true,
			disabled: data.is_public,
		});
		if (data.is_public)
			make_submitters_contestants.hide(0);

		this.append(ajax_form('Edit contest', '/api/contest/c' + contest_id + '/edit',
			Form.field_group("Contest's name", {
				type: 'text',
				name: 'name',
				value: data.name,
				size: 24,
				// maxlength: 'TODO...',
				trim_before_send: true,
				required: true
			}).add(Form.field_group('Public', {
				type: 'checkbox',
				name: 'public',
				checked: data.is_public,
				disabled: (data.is_public || actions.indexOf('M') !== -1 ? undefined : true),
				click: function () {
					make_submitters_contestants.find('input').prop('disabled', this.checked);
					if (this.checked) {
						make_submitters_contestants.hide(0);
					} else {
						make_submitters_contestants.show(0);
					}
				}
			})).add(make_submitters_contestants)
			.add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Update'
				})
			})
		));

	}, '/c/c' + contest_id + '/edit');
}
function edit_contest_round(as_modal, contest_round_id) {
	old_view_ajax(as_modal, '/api/contest/r' + contest_round_id, function(data) {
		if (data.contest.actions.indexOf('A') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		var round = data.rounds[0];
		this.append(ajax_form('Edit round', '/api/contest/r' + contest_round_id + '/edit',
			Form.field_group("Round's name", {
				type: 'text',
				name: 'name',
				value: round.name,
				size: 25,
				// maxlength: 'TODO...',
				trim_before_send: true,
				required: true
			}).add(Form.field_group('Begin time',
				dt_chooser_input('begins', true, true, true, utcdt_or_tm_to_Date(round.begins), 'The Big Bang', 'Never')
			)).add(Form.field_group('End time',
				dt_chooser_input('ends', true, true, true, utcdt_or_tm_to_Date(round.ends), 'The Big Bang', 'Never')
			)).add(Form.field_group('Full results time',
				dt_chooser_input('full_results', true, true, true, utcdt_or_tm_to_Date(round.full_results), 'Immediately', 'Never')
			)).add(Form.field_group('Show ranking since',
				dt_chooser_input('ranking_expo', true, true, true, utcdt_or_tm_to_Date(round.ranking_exposure), 'The Big Bang', "Don't show")
			)).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Update'
				})
			})
		));
	}, '/c/r' + contest_round_id + '/edit');
}
function edit_contest_problem(as_modal, contest_problem_id) {
	old_view_ajax(as_modal, '/api/contest/p' + contest_problem_id, function(data) {
		if (data.contest.actions.indexOf('A') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		var problem = data.problems[0];
		this.append(ajax_form('Edit problem', '/api/contest/p' + contest_problem_id + '/edit',
			Form.field_group("Problem's name", {
				type: 'text',
				name: 'name',
				value: problem.name,
				size: 25,
				// maxlength: 'TODO...',
				trim_before_send: true,
				required: true
			}).add(Form.field_group('Score revealing',
				$('<select>', {
					name: 'score_revealing',
					required: true,
					html: $('<option>', {
						value: 'none',
						text: 'None',
						selected: (problem.score_revealing === 'none')
					}).add('<option>', {
						value: 'only_score',
						text: 'Only score',
						selected: (problem.score_revealing === 'only_score')
					}).add('<option>', {
						value: 'score_and_full_status',
						text: 'Score and full status',
						selected: (problem.score_revealing === 'score_and_full_status')
					})
				})
			)).add(Form.field_group("Final submission to select",
				$('<select>', {
					name: 'method_of_choosing_final_submission',
					required: true,
					html: $('<option>', {
						value: 'latest_compiling',
						text: 'Latest compiling',
						selected: (problem.method_of_choosing_final_submission === 'latest_compiling')
					}).add('<option>', {
						value: 'highest_score',
						text: 'The one with the highest score',
						selected: (problem.method_of_choosing_final_submission === 'highest_score')
					})
				})
			)).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Update'
				})
			}), function(resp, loader_parent) {
				if (resp === '')
					show_success_via_loader($(this)[0], 'Updated');
				else if (as_modal) {
					show_success_via_loader($(this)[0], 'Updated');
					view_job(true, resp);
				} else {
					this.parent().remove();
					window.location.href = '/jobs/' + resp;
				}
			}
		));
	}, '/c/p' + contest_problem_id + '/edit');
}
function rejudge_contest_problem_submissions(contest_problem_id, contest_problem_name) {
	dialogue_modal_request("Rejudge all contest problem's submissions", $('<label>', {
			html: [
				'Are you sure to rejudge all submissions to the contest problem ',
				a_view_button('/c/p' + contest_problem_id, contest_problem_name, undefined,
					view_contest_problem.bind(null, true, contest_problem_id)),
				'?'
			]
		}), 'Rejudge all', 'btn-small blue',
		'/api/contest/p' + contest_problem_id + '/rejudge_all_submissions',
		'The rejudge jobs has been scheduled.', 'No, go back', true);
}
function delete_contest(as_modal, contest_id) {
	old_view_ajax(as_modal, '/api/contests/=' + contest_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
					status: '404',
					statusText: 'Not Found'
				});

		var contest = data[0];
		if (contest.actions.indexOf('D') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		delete_with_password_to_job(this, 'Delete contest',
			'/api/contest/c' + contest_id + '/delete', [
				'You are going to delete the contest ',
				a_view_button('/c/c' + contest.id, contest.name,
					undefined, view_contest.bind(null, true, contest.id)),
				' all submissions to it and all attached files. As it cannot be undone, you have to confirm it with your password.'
			], 'Delete contest');
	}, '/c/c' + contest_id + '/delete');
}
function delete_contest_round(as_modal, contest_round_id) {
	old_view_ajax(as_modal, '/api/contest/r' + contest_round_id, function(data) {
		var contest = data.contest;
		var round = data.rounds[0];
		if (contest.actions.indexOf('A') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		delete_with_password_to_job(this, 'Delete contest round',
			'/api/contest/r' + contest_round_id + '/delete', [
					'You are going to delete the contest round ',
					a_view_button('/c/r' + round.id, round.name,
						undefined, view_contest_round.bind(null, true, round.id)),
					' and all submissions to it. As it cannot be undone, you have to confirm it with your password.'
				], 'Delete round');
	}, '/c/r' + contest_round_id + '/delete');
}
function delete_contest_problem(as_modal, contest_problem_id) {
	old_view_ajax(as_modal, '/api/contest/p' + contest_problem_id, function(data) {
		var contest = data.contest;
		var problem = data.problems[0];
		if (contest.actions.indexOf('A') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		delete_with_password_to_job(this, 'Delete contest problem',
			'/api/contest/p' + contest_problem_id + '/delete', [
					'You are going to delete the contest problem ',
					a_view_button('/c/p' + problem.id, problem.name,
						undefined, view_contest_problem.bind(null, true, problem.id)),
					' and all submissions to it. As it cannot be undone, you have to confirm it with your password.'
				], 'Delete contest problem');
	}, '/c/p' + contest_problem_id + '/delete');
}
function view_contest_impl(as_modal, id_for_api, opt_hash /*= ''*/) {
	old_view_ajax(as_modal, '/api/contest/' + id_for_api, function(data) {
		var contest = data.contest;
		var rounds = data.rounds;
		var problems = data.problems;
		var actions = contest.actions;

		// Sort rounds by -items
		rounds.sort(function(a, b) { return b.item - a.item; });

		// Sort problems by (round_id, item)
		problems.sort(function(a, b) {
			return (a.round_id == b.round_id ? a.item - b.item :
				a.round_id - b.round_id);
		});

		// Header
		var header = $('<div>', {class: 'header', html:
			$('<h2>', {
				class: 'contest-path',
				html: function() {
					var res = [
						(id_for_api[0] === 'c' ? $('<span>', {text: contest.name}) :
							$('<a>', {
								href: '/c/c' + contest.id,
								text: contest.name
							}))
					];

					if (id_for_api[0] !== 'c')
						res.push(' / ', (id_for_api[0] === 'r' ?
							$('<span>', {text: rounds[0].name}) :
							$('<a>', {
								href: '/c/r' + rounds[0].id,
								text: rounds[0].name
							})));

					if (id_for_api[0] === 'p') {
						res.push(' / ', $('<span>', {text: problems[0].name}));
						// res.push($('<a>', {
						// 	class: 'btn-small',
						// 	href: '/api/contest/p' + problems[0][0] + '/statement/' +
						// 		encodeURIComponent(problems[0][4]),
						// 	text: 'View statement'
						// }));
					}
					return res;
				}()
		})}).appendTo(this);

		// Contest buttons
		if (id_for_api[0] === 'c')
			header.append($('<div>', {html: [
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/c' + contest.id + '/add_round', 'Add round', 'btn-small', add_contest_round.bind(null, true, contest.id))),
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/c' + contest.id + '/edit', 'Edit', 'btn-small blue', edit_contest.bind(null, true, contest.id))),
				(actions.indexOf('D') === -1 ? '' : a_view_button('/c/c' + contest.id + '/delete', 'Delete', 'btn-small red', delete_contest.bind(null, true, contest.id))),
			]}));

		// Round buttons
		else if (id_for_api[0] === 'r')
			header.append($('<div>', {html: [
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/r' + rounds[0].id + '/attach_problem', 'Attach problem', 'btn-small', add_contest_problem.bind(null, true, rounds[0].id))),
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/r' + rounds[0].id + '/edit', 'Edit', 'btn-small blue', edit_contest_round.bind(null, true, rounds[0].id))),
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/r' + rounds[0].id + '/delete', 'Delete', 'btn-small red', delete_contest_round.bind(null, true, rounds[0].id)))
			]}));

		// Problem buttons
		else if (id_for_api[0] === 'p')
			header.append($('<div>', {html: [
				(problems[0].can_view_problem ? a_view_button('/p/' + problems[0].problem_id, 'View in Problems', 'btn-small green', view_problem.bind(null, true, problems[0].problem_id)) : ''),
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/p' + problems[0].id + '/edit', 'Edit', 'btn-small blue', edit_contest_problem.bind(null, true, problems[0].id))),
				(actions.indexOf('A') === -1 ? '' :
					$('<a>', {
						class: 'btn-small blue',
						text: 'Rejudge submissions',
						click: rejudge_contest_problem_submissions.bind(null, problems[0].id, problems[0].name)
					})),
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/p' + problems[0].id + '/delete', 'Delete', 'btn-small red', delete_contest_problem.bind(null, true, problems[0].id)))
			]}));

		var contest_dashboard;
		var elem = $(this);
		var tabs = [
			'Dashboard', function() {
				timed_hide_show(elem.parents('.modal'));
				// If already generated, then just show it
				if (contest_dashboard !== undefined) {
					contest_dashboard.appendTo(elem);
					return;
				}

				var dashboard = $('<div>', {class: 'contest-dashboard'})
					.appendTo($('<div>', {style: 'margin: 0 16px'}).appendTo(elem));
				contest_dashboard = dashboard.parent();

				var wrap_arrows = function(c, elem) {
					if (id_for_api[0] === c)
						return $('<div>', {
							style: 'position: relative',
							html: [
								elem,
								'<svg class="left" width="16" height="16"><polygon points="0,0 16,8 0,16 4,8"></polygon></svg>',
								'<svg class="right" width="16" height="16"><polygon points="16,0 0,8 16,16 12,8"></polygon></svg>'
							]
						});

					return elem;
				};
				// Contest
				dashboard.append(wrap_arrows('c', $('<a>', {
					class: 'contest',
					href: '/c/c' + contest.id,
					text: contest.name
				})));

				var dashboard_rounds = $('<div>', {
					class: 'rounds'
				}).appendTo(dashboard);

				// Rounds and problems
				function append_round(round) {
					return $('<div>', {
						class: 'round',
						html: [
							wrap_arrows('r', $('<a>', {
								href: '/c/r' + round.id,
								html: function() {
									var res = [$('<span>', {text: round.name})];
									if (rounds.length !== 1)
										res.push($('<div>', {html: [
											$('<label>', {text: "B:"}),
											infdatetime_to($('<span>'), round.begins,
												'The Big Bang', 'never')
										]}),
										$('<div>', {html: [
											$('<label>', {text: "E:"}),
											infdatetime_to($('<span>'), round.ends,
												'The Big Bang', 'never')
										]}),
										$('<div>', {html: [
											$('<label>', {text: "R:"}),
											infdatetime_to($('<span>'), round.full_results,
												'immediately', 'never')
										]}));
									else
										res.push($('<table>', {html: [
											$('<tr>', {html: [
												$('<td>', {text: 'Beginning'}),
												infdatetime_to($('<td>'), round.begins,
													'The Big Bang', 'never')
											]}),
											$('<tr>', {html: [
												$('<td>', {text: 'End'}),
												infdatetime_to($('<td>'), round.ends,
													'The Big Bang', 'never')
											]}),
											$('<tr>', {html: [
												$('<td>', {text: "Full results"}),
												infdatetime_to($('<td>'), round.full_results,
													'immediately', 'never')
											]}),
											$('<tr>', {html: [
												$('<td>', {text: "Ranking since"}),
												infdatetime_to($('<td>'), round.ranking_exposure,
													'The Big Bang', 'never')
											]}),
										]}));

									return res;
								}()
							}))
						]
					}).appendTo(dashboard_rounds);
				}

				function append_problem(dashboard_round, problem, cannot_submit) {
					// Problem buttons and statement preview
					if (id_for_api[0] === 'p') {
						var statement_url = '/api/download/statement/contest/p' + problem.id + '/' + encodeURIComponent(problem.name);
						$('<center>', {html: [
							$('<a>', {
								class: 'btn-small',
								href: statement_url,
								text: 'Statement'
							}),
							(cannot_submit ? '' : a_view_button('/c/p' + problem.id + '/submit', 'Submit', 'btn-small blue',
								add_contest_submission.bind(null, true, contest, round, problem))),
						]}).appendTo(dashboard.parent());

						$('<object>').prop({
							class: 'statement',
							data: statement_url
						}).appendTo(dashboard.parent().parent());
					}

					var elem = $('<a>', {
						href: '/c/p' + problem.id,
						style: (id_for_api[0] === 'p' ?
							'padding: 7px; line-height: 12px' : undefined),
						class: (problem.color_class === null ? undefined : 'status ' + problem.color_class),
						html: [
							$('<span>', {text: problem.name}),
							(id_for_api[0] === 'p' ? $('<table>', {html: [
								$('<tr>', {html: [
									$('<td>', {text: 'Final submission'}),
									$('<td>', {text: problem.method_of_choosing_final_submission === 'latest_compiling' ?
										'Latest compiling' : 'One with the highest score'})
								]}),
								(problem.score_revealing !== 'none' ? $('<tr>', {html: [
									$('<td>', {text: 'Score revealing'}),
									$('<td>', {text: problem.score_revealing === 'only_score' ? 'Only score' : 'Score and full status'})
								]}) : $())
							]}) : $())
						]
					});

					wrap_arrows('p', elem).appendTo(dashboard_round);
				}

				for (var i = 0; i < rounds.length; ++i) {
					var round = rounds[i];
					var dashboard_round = append_round(round);
					// Round's problems
					// Bin-search first problem
					var l = 0, r = problems.length;
					while (l < r) {
						var mid = (l + r) >> 1;
						var p = problems[mid];
						if (p.round_id < round.id)
							l = mid + 1;
						else
							r = mid;
					}
					// Check whether the round has ended
					var cannot_submit = (actions.indexOf('p') === -1);
					if (actions.indexOf('A') === -1) {
						var curr_time = date_to_datetime_str(new Date());
						var end_time = utcdt_or_tm_to_Date(round.ends);
						cannot_submit |= (end_time <= curr_time);
					}
					// Append problems
					while (l < problems.length && problems[l].round_id == round.id)
						append_problem(dashboard_round, problems[l++], cannot_submit);
				}

				if (is_logged_in()) {
					// All problems have been colored
					// Mark rounds as green (only the ones that contain a problem)
					var rounds_to_check = dashboard_rounds.children().filter(function() { return $(this).children().length > 1; });
					var green_contest = rounds_to_check.length > 0;

					rounds_to_check.each(function() {
						var round = $(this);
						var round_a = round.find('a').eq(0);
						if (round_a.hasClass('green'))
							return;

						var problems = round.find('a').slice(1);
						if (problems.filter(function() {
								return $(this).hasClass('green');
							}).length === problems.length)
						{
							round_a.addClass('status green');
						} else
							green_contest = false;
					});

					if (green_contest)
						dashboard.find('.contest').addClass('status green');
				}
			}
		];

		if (actions.indexOf('A') !== -1)
			tabs.push('All submissions', function() {
				tab_submissions_lister($('<div>').appendTo(elem), '/' + id_for_api.toUpperCase());
			});

		if (actions.indexOf('p') !== -1)
			tabs.push('My submissions', function() {
				tab_submissions_lister($('<div>').appendTo(elem), '/' + id_for_api.toUpperCase() + '/u' + logged_user_id(), false, (actions.indexOf('A') === -1));
			});

		if (actions.indexOf('v') !== -1)
			tabs.push('Ranking', function() {
				contest_ranking($('<div>').appendTo(elem), id_for_api);
			});

		if (actions.indexOf('A') !== -1)
			tabs.push('Users', function() {
				var entry_link_elem = $('<div>').appendTo(elem);
				var render_entry_link_panel = function() {
					var this_ = this;
					entry_link_elem.empty();

					API_get(url_api_contest_entry_tokens_view(contest.id), function(data) {
						if (data.token != null) {
							if (data.token.value === null) {
								if (data.token.capabilities.create) {
									entry_link_elem.append(a_view_button(undefined,
										'Add entry link', 'btn', modal_request.bind(null,
											'Add entry link', $('<form>'),
											url_api_contest_entry_tokens_add(contest.id), function(resp, loader_parent) {
												show_success_via_loader(loader_parent[0], 'Added');
												render_entry_link_panel.call(this_);
											})));
								}
							} else {
								entry_link_elem.append(copy_to_clipboard_btn(false, 'Copy link', function() {
									return window.location.origin + url_enter_contest(data.token.value);
								}));
								if (data.token.capabilities.regen) {
									entry_link_elem.append(a_view_button(undefined,
										'Regenerate link', 'btn blue', dialogue_modal_request.bind(null,
											'Regenerate link', $('<center>', {
												html: [
												'Are you sure to regenerate the entry link: ',
												$('<br>'),
												a_view_button(url_enter_contest(data.token.value), window.location.origin + url_enter_contest(data.token.value), undefined, enter_contest_using_token.bind(null, true, data.token.value)),
												$('<br>'),
												'?'
												]
											}), 'Yes, regenerate it', 'btn-small blue', url_api_contest_entry_tokens_regen(contest.id),
											function(resp, loader_parent) {
												show_success_via_loader(loader_parent[0], 'Regenerated');
												render_entry_link_panel.call(this_);
											}, 'No, take me back', true)));
								}
								if (data.token.capabilities.delete) {
									entry_link_elem.append(a_view_button(undefined,
										'Delete link', 'btn red', dialogue_modal_request.bind(null,
											'Delete link', $('<center>', {
												html: [
												'Are you sure to delete the entry link: ',
												$('<br>'),
												a_view_button(url_enter_contest(data.token.value), window.location.origin + url_enter_contest(data.token.value), undefined, enter_contest_using_token.bind(null, true, data.token.value)),
												$('<br>'),
												'?'
												]
											}), 'Yes, I am sure', 'btn-small red', url_api_contest_entry_tokens_delete(contest.id),
											function(resp, loader_parent) {
												show_success_via_loader(loader_parent[0], 'Deleted');
												render_entry_link_panel.call(this_);
											}, 'No, take me back', true)));
								}
								entry_link_elem.append($('<pre>', {
									text: window.location.origin + url_enter_contest(data.token.value)
								}));
							}
						}
						if (data.short_token != null) {
							if (data.short_token.value === null) {
								if (data.short_token.capabilities.create && data.token.value != null) {
									entry_link_elem.append(a_view_button(undefined,
										'Add short entry link', 'btn', modal_request.bind(null,
											'Add short entry link', $('<form>'),
											url_api_contest_entry_tokens_add_short(contest.id), function(resp, loader_parent) {
												show_success_via_loader(loader_parent[0], 'Added');
												render_entry_link_panel.call(this_);
											})));
								}
							} else {
								entry_link_elem.append(copy_to_clipboard_btn(false, 'Copy short link', function() {
									return window.location.origin + url_enter_contest(data.short_token.value);
								}));
								if (data.short_token.capabilities.regen) {
									entry_link_elem.append(a_view_button(undefined,
										'Regenerate short link', 'btn blue', dialogue_modal_request.bind(null,
											'Regenerate short link', $('<center>', {
												html: [
												'Are you sure to regenerate the entry short link: ',
												$('<br>'),
												a_view_button(url_enter_contest(data.short_token.value), window.location.origin + url_enter_contest(data.short_token.value), undefined, enter_contest_using_token.bind(null, true, data.short_token.value)),
												$('<br>'),
												'?'
												]
											}), 'Yes, regenerate it', 'btn-small blue', url_api_contest_entry_tokens_regen_short(contest.id),
											function(resp, loader_parent) {
												show_success_via_loader(loader_parent[0], 'Regenerated');
												render_entry_link_panel.call(this_);
											}, 'No, take me back', true)));
								}
								if (data.short_token.capabilities.delete) {
									entry_link_elem.append(a_view_button(undefined,
										'Delete short link', 'btn red', dialogue_modal_request.bind(null,
											'Delete short link', $('<center>', {
												html: [
												'Are you sure to delete the entry short link: ',
												$('<br>'),
												a_view_button(url_enter_contest(data.short_token.value), window.location.origin + url_enter_contest(data.short_token.value), undefined, enter_contest_using_token.bind(null, true, data.short_token.value)),
												$('<br>'),
												'?'
												]
											}), 'Yes, I am sure', 'btn-small red', url_api_contest_entry_tokens_delete_short(contest.id),
											function(resp, loader_parent) {
												show_success_via_loader(loader_parent[0], 'Deleted');
												render_entry_link_panel.call(this_);
											}, 'No, take me back', true)));
								}
								var exp_date = utcdt_or_tm_to_Date(data.short_token.expires_at);
								entry_link_elem.append($('<span>', {
									style: 'margin-left: 10px; font-size: 12px; color: #777',
									html: ['Short token will expire in ', countdown_clock(exp_date)]
								}));

								entry_link_elem.append($('<pre>', {
									text: window.location.origin + url_enter_contest(data.short_token.value)
								}));
							}
						}
					}, entry_link_elem);
				};

				render_entry_link_panel();
				tab_contest_users_lister($('<div>').appendTo(elem), '/c' + contest.id);
			});

		tabs.push('Files', function() {
			var table = $('<table class="contest-files stripped"></table>').appendTo($('<div>').appendTo(elem));
			new ContestFilesLister(table, '/c' + contest.id).monitor_scroll();
		});

		var header_links_updater = function(_, active_elem) {
			// Add / replace hashes in links in the contest-path
			elem.find('.contest-path').children('a:not(.btn-small)').each(function() {
				var xx = $(this);
				var href = xx.attr('href');
				var pos = href.indexOf('#');
				if (pos !== -1)
					href = href.substring(0, pos);
				xx.attr('href', href + window.location.hash);
			});
		};

		tabmenu(tabmenu_attacher_with_change_callback.bind(elem, header_links_updater), tabs);

	}, '/c/' + id_for_api + (opt_hash === undefined ? '' : opt_hash));
}
function view_contest(as_modal, contest_id, opt_hash /*= ''*/) {
	return view_contest_impl(as_modal, 'c' + contest_id, opt_hash);
}
function view_contest_round(as_modal, contest_round_id, opt_hash /*= ''*/) {
	return view_contest_impl(as_modal, 'r' + contest_round_id, opt_hash);
}
function view_contest_problem(as_modal, contest_problem_id, opt_hash /*= ''*/) {
	return view_contest_impl(as_modal, 'p' + contest_problem_id, opt_hash);
}
function contest_ranking(elem_, id_for_api) {
	var elem = elem_;
	old_API_call('/api/contest/' + id_for_api, function(cdata) {
		var contest = cdata.contest;
		var rounds = cdata.rounds;
		var problems = cdata.problems;
		var is_admin = (contest.actions.indexOf('A') !== -1);

		// Sort rounds by -items
		rounds.sort(function(a, b) { return b.item - a.item; });
		// Map rounds (by id) to their items
		var rid_to_item = new StaticMap();
		var curr_time = new Date();
		for (var i = 0; i < rounds.length; ++i) {
			if (!is_admin) {
				// Add only those rounds that have the ranking_exposure point has passed
				var ranking_exposure = utcdt_or_tm_to_Date(rounds[i].ranking_exposure);
				if (curr_time < ranking_exposure)
					continue; // Do not show the round
			}

			// Show the round
			rid_to_item.add(rounds[i].id, rounds[i].item);
		}
		rid_to_item.prepare();

		// Add round item to the every problem (and remove invalid problems -
		// the ones which don't belong to the valid rounds)
		var tmp_problems = [];
		for (i = 0; i < problems.length; ++i) {
			var x = rid_to_item.get(problems[i].round_id);
			if (x != null) {
				problems[i].round_item = x;
				tmp_problems.push(problems[i]);
			}
		}
		problems = tmp_problems;

		// Sort problems by (-round_item, item)
		problems.sort(function(a, b) {
			return (a.round_item == b.round_item ? a.item - b.item :
				b.round_item - a.round_item);
		});

		// Map problems (by id) to their indexes in the above array
		var problem_to_col_id = new StaticMap();
		for (i = 0; i < problems.length; ++i)
			problem_to_col_id.add(problems[i].id, i);
		problem_to_col_id.prepare();

		old_API_call('/api/contest/' + id_for_api + '/ranking', function(data) {
			var modal = elem.parents('.modal');
			if (data.length == 0) {
				timed_hide_show(modal);
				var message_to_show = '<p>There is no one in the ranking yet...</p>';
				if (rounds.length === 1) {
					var ranking_exposure = utcdt_or_tm_to_Date(rounds[0].ranking_exposure);
					if (ranking_exposure === Infinity)
						message_to_show = '<p>The current round will not be shown in the ranking.</p>';
					else if (ranking_exposure > new Date())
						message_to_show = $('<p>Ranking will be available since: </p>').append(normalize_datetime($('<span>', {datetime: rounds[0].ranking_exposure})));
				}

				return elem.append($('<center>', {
					class: 'always_in_view',
					html: message_to_show
				}));
			}

			// Construct table's head
			var tr = $('<tr>', {
				html: [
					$('<th>', {
						rowspan: 2,
						text: '#'
					}),
					$('<th>', {
						rowspan: 2,
						text: 'User'
					}),
					$('<th>', {
						rowspan: 2,
						text: 'Sum'
					}),
				]
			});
			// Add rounds
			var colspan = 0;
			var j = 0; // Index of the current round
			for (var i = 0; i < problems.length; ++i) {
				var problem = problems[i];
				while (rounds[j].id != problem.round_id)
					++j; // Skip rounds (that have no problems attached)

				++colspan;
				// If current round's problems end
				if (i + 1 == problems.length || problems[i + 1].round_id != problem.round_id) {
					tr.append($('<th>', {
						colspan: colspan,
						html: $('<a>', {
							href: '/c/r' + rounds[j].id + '#ranking',
							text: rounds[j].name
						})
					}));
					colspan = 0;
					++j;
				}
			}
			var thead = $('<thead>', {html: tr});
			tr = $('<tr>');
			// Add problems
			for (i = 0; i < problems.length; ++i)
				tr.append($('<th>', {
					html: $('<a>', {
						href: '/c/p' + problems[i].id + '#ranking',
						text: problems[i].problem_label
					})
				}));
			thead.append(tr);

			// Add score for each user add this to the user's info
			var submissions;
			for (i = 0; i < data.length; ++i) {
				submissions = data[i].submissions;
				var total_score = 0;
				// Count only valid problems (to fix potential discrepancies
				// between ranking submissions and the contest structure)
				for (j = 0; j < submissions.length; ++j) {
					if (problem_to_col_id.get(submissions[j].contest_problem_id) !== null) {
						total_score += submissions[j].score;
					}
				}

				data[i].score = total_score;
			}

			// Sort users (and their submissions) by their -score
			data.sort(function(a, b) { return b.score - a.score; });

			// Add rows
			var tbody = $('<tbody>');
			var prev_score = data[0].score + 1;
			var place;
			for (i = 0; i < data.length; ++i) {
				var user_row = data[i];
				tr = $('<tr>');
				// Place
				if (prev_score != user_row.score) {
					place = i + 1;
					prev_score = user_row.score;
				}
				tr.append($('<td>', {text: place}));
				// User
				if (user_row.id === null)
					tr.append($('<td>', {text: user_row.name}));
				else {
					tr.append($('<td>', {
						html: a_view_button('/u/' + user_row.id, user_row.name, '',
							view_user.bind(null, true, user_row.id))
					}));
				}
				// Score
				tr.append($('<td>', {text: user_row.score}));
				// Submissions
				var row = new Array(problems.length);
				submissions = data[i].submissions;
				for (j = 0; j < submissions.length; ++j) {
					var x = problem_to_col_id.get(submissions[j].contest_problem_id);
					if (x != null) {
						var score_text = (submissions[j].score != null ? submissions[j].score : '?');
						if (submissions[j].id === null) {
							row[x] = $('<td>', {
								class: 'status ' + submissions[j].status.class,
								text: score_text
							});
						} else {
							row[x] = $('<td>', {
								class: 'status ' + submissions[j].status.class,
								html: a_view_button('/s/' + submissions[j].id,
									score_text, '',
									view_submission.bind(null, true, submissions[j].id))
							});
						}
					}
				}
				// Construct the row
				for (j = 0; j < problems.length; ++j) {
					if (row[j] === undefined)
						$('<td>').appendTo(tr);
					else
						row[j].appendTo(tr);
				}

				tbody.append(tr);
			}

			elem.append($('<table>', {
				class: 'table ranking stripped',
				html: [thead, tbody]
			}));

			timed_hide_show(modal);
			centerize_modal(modal, false);

		}, elem);
	}, elem);
}
function ContestsLister(elem, query_suffix /*= ''*/) {
	var this_ = this;
	if (query_suffix === undefined)
		query_suffix = '';

	OldLister.call(this, elem);
	this.query_url = '/api/contests' + query_suffix;
	this.query_suffix = '';

	this.process_api_response = function(data, modal) {
		if (this_.elem.children('thead').length === 0) {
			if (data.length === 0) {
				this_.elem.parent().append($('<center>', {
					class: 'contests always_in_view',
					// class: 'contests',
					html: '<p>There are no contests to show...</p>'
				}));
				remove_loader(this_.elem.parent()[0]);
				timed_hide_show(modal);
				return;
			}

			this_.elem.html('<thead><tr>' +
					(logged_user_is_admin() ? '<th>Id</th>' : '') +
					'<th class="name">Name</th>' +
					'<th class="actions">Actions</th>' +
				'</tr></thead><tbody></tbody>');
			add_tz_marker(this_.elem.find('thead th.added'));
		}

		for (var x in data) {
			x = data[x];
			this_.query_suffix = '/<' + x.id;

			var row = $('<tr>',	{
				class: (x.is_public ? undefined : 'grayed')
			});

			// Id
			if (logged_user_is_admin())
				row.append($('<td>', {text: x.id}));
			// Name
			row.append($('<td>', {
				html: $('<a>', {
					href: '/c/c' + x.id,
					text: x.name
				})
			}));

			// Actions
			row.append($('<td>', {
				html: ActionsToHTML.contest(x.id, x.actions)
			}));

			this_.elem.children('tbody').append(row);
		}
	};

	this.fetch_more();
}
function tab_contests_lister(parent_elem, query_suffix /*= ''*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	parent_elem = $(parent_elem);
	function retab(tab_qsuff) {
		var table = $('<table class="contests"></table>').appendTo(parent_elem);
		new ContestsLister(table, query_suffix + tab_qsuff).monitor_scroll();
	}

	var tabs = [
		'All', retab.bind(null, '')
	];

	// TODO: implement it
	// if (is_logged_in())
		// tabs.push('My', retab.bind(null, '/u' + logged_user_id()));

	tabmenu(default_tabmenu_attacher.bind(parent_elem), tabs);
}
function contest_chooser(as_modal /*= true*/, opt_hash /*= ''*/) {
	view_base((as_modal === undefined ? true : as_modal),
		'/c' + (opt_hash === undefined ? '' : opt_hash), function() {
			timed_hide($(this).parent().parent().filter('.modal'));
			$(this).append($('<h1>', {text: 'Contests'}));
			if (logged_user_is_teacher_or_admin())
				$(this).append(a_view_button('/c/add', 'Add contest', 'btn',
					add_contest.bind(null, true)));

			tab_contests_lister($(this));
		});
}

/* ============================ Contest's users ============================ */
function ContestUsersLister(elem, query_suffix /*= ''*/) {
	var this_ = this;
	if (query_suffix === undefined)
		query_suffix = '';

	OldLister.call(this, elem);
	this.query_url = '/api/contest_users' + query_suffix;
	this.query_suffix = '';

	var x = query_suffix.indexOf('/c');
	var y = query_suffix.indexOf('/', x + 2);
	this.contest_id = query_suffix.substring(x + 2, y == -1 ? undefined : y);

	this.process_api_response = function(data, modal) {
		if (this_.elem.children('thead').length === 0) {
			// Overall actions
			elem.prev('.tabmenu').prevAll().remove();
			this_.elem.parent().prepend(ActionsToHTML.contest_users(this_.contest_id, data.overall_actions));

			if (data.rows.length == 0) {
				this_.elem.parent().append($('<center>', {
					class: 'contest-users always_in_view',
					// class: 'contest-users',
					html: '<p>There are no contest users to show...</p>'
				}));
				remove_loader(this_.elem.parent()[0]);
				timed_hide_show(modal);
				return;
			}

			this_.elem.html('<thead><tr>' +
					'<th>Id</th>' +
					'<th class="username">Username</th>' +
					'<th class="first-name">First name</th>' +
					'<th class="last-name">Last name</th>' +
					'<th class="mode">Mode</th>' +
					'<th class="actions">Actions</th>' +
				'</tr></thead><tbody></tbody>');
		}

		for (var x in data.rows) {
			x = data.rows[x];
			this_.query_suffix = '/<' + x.id;

			var row = $('<tr>');
			row.append($('<td>', {text: x.id}));
			row.append($('<td>', {html: a_view_button('/u/' + x.id, x.username,
				'', view_user.bind(null, true, x.id))}));
				// x.username}));
			row.append($('<td>', {text: x.first_name}));
			row.append($('<td>', {text: x.last_name}));
			row.append($('<td>', {
				class: x.mode[0].toLowerCase() + x.mode.slice(1),
				text: x.mode
			}));

			// Actions
			row.append($('<td>', {
				html: ActionsToHTML.contest_user(x.id, this_.contest_id, x.actions)
			}));

			this_.elem.children('tbody').append(row);
		}
	};

	this.fetch_more();
}
function tab_contest_users_lister(parent_elem, query_suffix /*= ''*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	parent_elem = $(parent_elem);
	function retab(tab_qsuff) {
		var table = $('<table class="contest-users stripped"></table>').appendTo(parent_elem);
		new ContestUsersLister(table, query_suffix + tab_qsuff).monitor_scroll();
	}

	var tabs = [
		'All', retab.bind(null, ''),
		'Owners', retab.bind(null, '/mO'),
		'Moderators', retab.bind(null, '/mM'),
		'Contestants', retab.bind(null, '/mC')
	];

	tabmenu(default_tabmenu_attacher.bind(parent_elem), tabs);
}
function add_contest_user(as_modal, contest_id) {
	old_view_ajax(as_modal, '/api/contest_users/c' + contest_id + '/<0', function(data) {
		if (data.overall_actions === undefined)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		var actions = data.overall_actions;
		var add_contestant = (actions.indexOf('Ac') !== -1);
		var add_moderator = (actions.indexOf('Am') !== -1);
		var add_owner = (actions.indexOf('Ao') !== -1);
		if (add_contestant + add_moderator + add_owner < 1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Add contest user', '/api/contest_user/c' + contest_id + '/add',
			Form.field_group('User ID', {
				type: 'text',
				name: 'user_id',
				size: 6,
				// maxlength: 'TODO...',
				trim_before_send: true,
				required: true
			}).append(a_view_button('/u', 'Search users', '', user_chooser))
			.add(Form.field_group('Mode', $('<select>', {
				name: 'mode',
				html: function() {
					var res = [];
					if (add_contestant)
						res.push($('<option>', {
							value: 'C',
							text: 'Contestant',
							selected: (data.mode === 'Contestant' ? true : undefined)
						}));

					if (add_moderator)
						res.push($('<option>', {
							value: 'M',
							text: 'Moderator',
							selected: (data.mode === 'Moderator' ? true : undefined)
						}));

					if (add_owner)
						res.push($('<option>', {
							value: 'O',
							text: 'Owner',
							selected: (data.mode === 'Owner' ? true : undefined)
						}));

					return res;
				}()
			}))).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Add'
				})
			})
		));

	}, '/c/c' + contest_id + '/contest_user/add');
}
function change_contest_user_mode(as_modal, contest_id, user_id) {
	old_view_ajax(as_modal, '/api/contest_users/c' + contest_id + '/=' + user_id, function(data) {
		if (data.rows === undefined || data.rows.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});


		data = data.rows[0];

		var actions = data.actions;
		var make_contestant = (actions.indexOf('Mc') !== -1);
		var make_moderator = (actions.indexOf('Mm') !== -1);
		var make_owner = (actions.indexOf('Mo') !== -1);
		if (make_contestant + make_moderator + make_owner < 2)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Change mode', '/api/contest_user/c' + contest_id + '/u' + user_id + '/change_mode', $('<center>', {
			html: [
				$('<label>', {
					html: [
						'Mode of the user ',
						a_view_button('/u/' + user_id, data.username, '', view_user.bind(null, true, user_id)),
						': ',
						$('<select>', {
							style: 'margin-left: 4px',
							name: 'mode',
							html: function() {
								var res = [];
								if (make_contestant)
									res.push($('<option>', {
										value: 'C',
										text: 'Contestant',
										selected: (data.mode === 'Contestant' ? true : undefined)
									}));

								if (make_moderator)
									res.push($('<option>', {
										value: 'M',
										text: 'Moderator',
										selected: (data.mode === 'Moderator' ? true : undefined)
									}));

								if (make_owner)
									res.push($('<option>', {
										value: 'O',
										text: 'Owner',
										selected: (data.mode === 'Owner' ? true : undefined)
									}));

								return res;
							}()
						})
					]
				}),
				$('<div>', {
					style: 'margin-top: 12px',
					html:
						$('<input>', {
							class: 'btn-small blue',
							type: 'submit',
							value: 'Update'
						})
				})
			]
		})));

	}, '/c/c' + contest_id + '/contest_user/' + user_id + '/change_mode');
}
function expel_contest_user(as_modal, contest_id, user_id) {
	old_view_ajax(as_modal, '/api/contest_users/c' + contest_id + '/=' + user_id, function(data) {
		if (data.rows === undefined || data.rows.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});


		data = data.rows[0];
		var actions = data.actions;

		if (actions.indexOf('E') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Expel user from the contest', '/api/contest_user/c' + contest_id + '/u' + user_id + '/expel', $('<center>', {
			html: [
				$('<label>', {
					html: [
						'Are you sure to expel the user ',
						a_view_button('/u/' + user_id, data.username,
							undefined, view_user.bind(null, true, user_id)),
						'?'
					]
				}),
				$('<div>', {
					style: 'margin-top: 12px',
					html: [
						$('<input>', {
							class: 'btn-small red',
							type: 'submit',
							value: 'Of course!'
						}),
						$('<a>', {
							class: 'btn-small',
							text: 'Nah, spare the user',
							click: function() {
								var modal = $(this).closest('.modal');
								if (modal.length === 0)
									history.back();
								else
									close_modal(modal);
							}
						})
					]
				})
			]
		})));

	}, '/c/c' + contest_id + '/contest_user/' + user_id + '/expel');
}

/* ============================= Contest files ============================= */
function ContestFilesLister(elem, query_suffix /*= ''*/) {
	var this_ = this;
	if (query_suffix === undefined)
		query_suffix = '';

	OldLister.call(this, elem);
	this.query_url = '/api/contest_files' + query_suffix;
	this.query_suffix = '';

	var x = query_suffix.indexOf('/c');
	var y = query_suffix.indexOf('/', x + 2);
	this.contest_id = query_suffix.substring(x + 2, y == -1 ? undefined : y);

	this.process_api_response = function(data, modal) {
		var show_creator = (data.overall_actions.indexOf('C') !== -1);
		if (this_.elem.children('thead').length === 0) {
			// Overall actions
			elem.prev('.tabmenu').prevAll().remove();
			this_.elem.parent().prepend(ActionsToHTML.contest_files(this_.contest_id, data.overall_actions));

			if (data.rows.length == 0) {
				this_.elem.parent().append($('<center>', {
					class: 'contest-files always_in_view',
					html: '<p>There are no files to show...</p>'
				}));
				remove_loader(this_.elem.parent()[0]);
				timed_hide_show(modal);
				return;
			}

			this_.elem.html('<thead><tr>' +
					(show_creator ? '<th class="creator">Creator</th>' : '') +
					'<th class="modified">Modified</th>' +
					'<th class="name">File name</th>' +
					'<th class="size">File size</th>' +
					'<th class="description">Description</th>' +
					'<th class="actions">Actions</th>' +
				'</tr></thead><tbody></tbody>');
			add_tz_marker(this_.elem[0].querySelector('thead th.modified'));
		}

		for (var x in data.rows) {
			x = data.rows[x];
			this_.query_suffix = '/<' + x.id;

			var row = $('<tr>');

			if (show_creator) {
				if (x.creator_id === null)
					row.append($('<td>', {text: '(deleted)'}));
				else
					row.append($('<td>', {html: a_view_button('/u/' + x.creator_id,
						x.creator_username, '',
						view_user.bind(null, true, x.creator_id))}));
			}

			row.append(normalize_datetime($('<td>', {
				datetime: x.modified,
				text: x.modified
			})));
			row.append($('<td>', {html: $('<a>', {
				href: '/api/download/contest_file/' + x.id,
				text: x.name
			})}));
			row.append($('<td>', {text: humanize_file_size(x.file_size)}));

			row.append($('<td>', {text: x.description}));

			// Actions
			row.append($('<td>', {
				html: ActionsToHTML.contest_file(x.id, x.actions)
			}));

			this_.elem.children('tbody').append(row);
		}
	};

	this.fetch_more();
}
function add_contest_file(as_modal, contest_id) {
	old_view_ajax(as_modal, '/api/contest_files/c' + contest_id, function(data) {
		if (data.overall_actions === undefined)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		if (data.overall_actions.indexOf('A') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Add contest file',
			'/api/contest_file/add/c' + contest_id, Form.field_group('File name', {
				type: 'text',
				name: 'name',
				size: 24,
				// maxlength: 'TODO...',
				trim_before_send: true,
				placeholder: 'The same as name of the uploaded file',
			}).add(Form.field_group('File', {
				type: 'file',
				name: 'file',
				required: true
			})).add(Form.field_group('Description',
				$('<textarea>', {
					name: 'description',
					maxlength: 512
				})
			)).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Add'
				})
			})
		));

	}, '/c/c' + contest_id + '/files/add');
}
function edit_contest_file(as_modal, contest_file_id) {
	old_view_ajax(as_modal, '/api/contest_files/=' + contest_file_id, function(data) {
		if (data.rows === undefined || data.rows.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		file = data.rows[0];
		if (file.actions.indexOf('E') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Edit contest file',
			'/api/contest_file/' + contest_file_id + '/edit', Form.field_group('File name', {
				type: 'text',
				name: 'name',
				value: file.name,
				size: 24,
				// maxlength: 'TODO...',
				trim_before_send: true,
				placeholder: 'The same as name of uploaded file',
			}).add(Form.field_group('File', {
				type: 'file',
				name: 'file'
			})).add(Form.field_group('Description',
				$('<textarea>', {
					name: 'description',
					maxlength: 512,
					text: file.description
				})
			)).add(Form.field_group('File size', {
				type: 'text',
				value: humanize_file_size(file.file_size) + ' (' + file.file_size + ' B)',
				disabled: true
			})).add(Form.field_group('Modified', normalize_datetime($('<span>', {
				datetime: file.modified,
				text: file.modified
			}), true))).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Update'
				})
			})
		));

	}, '/contest_file/' + contest_file_id + '/edit');
}
function delete_contest_file(as_modal, contest_file_id) {
	old_view_ajax(as_modal, '/api/contest_files/=' + contest_file_id, function(data) {
		if (data.rows === undefined || data.rows.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		file = data.rows[0];
		if (file.actions.indexOf('D') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Delete contest file',
			'/api/contest_file/' + contest_file_id + '/delete', $('<center>', {
			html: [
				$('<label>', {
					html: [
						'Do you really want to delete the contest file ',
						a_view_button('/contest_file/' + contest_file_id + '/edit', file.name,
							undefined, edit_contest_file.bind(null, true, contest_file_id)),
						'?'
					]
				}),
				$('<div>', {
					style: 'margin-top: 12px',
					html: [
						$('<input>', {
							class: 'btn-small red',
							type: 'submit',
							value: 'Yes, delete it'
						}),
						$('<a>', {
							class: 'btn-small',
							text: 'No, take me back',
							click: function() {
								var modal = $(this).closest('.modal');
								if (modal.length === 0)
									history.back();
								else
									close_modal(modal);
							}
						})
					]
				})
			]
		})));

	}, '/contest_file/' + contest_file_id + '/delete');
}

/* ============================ Contest's users ============================ */
function enter_contest_using_token(as_modal, contest_entry_token) {
	view_ajax(as_modal, url_api_contest_name_for_contest_entry_token(contest_entry_token), function(data) {
		this.append(ajax_form('Contest entry', url_api_use_contest_entry_token(contest_entry_token), $('<center>', {
			html: [
				$('<label>', {
					html: [
						'You are about to enter the contest ',
						a_view_button('/c/' + data.contest.id, data.contest.name,
							undefined, view_contest.bind(null, true, data.contest.id)),
						'. Are you sure?'
					]
				}),
				$('<div>', {
					style: 'margin-top: 12px',
					html: [
						$('<input>', {
							class: 'btn-small green',
							type: 'submit',
							value: 'Yes, I want to enter'
						}),
						$('<a>', {
							class: 'btn-small',
							text: 'No, take me back',
							click: function() {
								var modal = $(this).closest('.modal');
								if (modal.length === 0)
									history.back();
								else
									close_modal(modal);
							}
						})
					]
				})
			]
		})));

	}, url_enter_contest(contest_entry_token));
}

function open_calendar_on(time, text_input, hidden_input) {
	var months = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
	var month_chooser = [];

	var header = $('<span>', {
		style: 'cursor: pointer; user-select: none',
		click: function() {
			var update_month_and_year = function() {
				var row = month_chooser.find('tr').eq(1).children();
				row.eq(0).text(months[time.getMonth()]);
				row.eq(1).text(time.getFullYear());
			};

			month_chooser = modal($('<table>', {
				class: 'chooser',
				style: 'margin-left: -10px',
				html: $('<tbody>', {
					html: $('<tr>', {
						html: $('<td>', {
							html: $('<a>', {
								click: function() {
									change_month(+1);
									update_month_and_year();
								},
								html: '<svg width="60" height="30"><polygon points="60,21 0,21 30,7"></polygon></svg>'
							})
						}).add('<td>', {
							html: $('<a>', {
								click: function() {
									change_month(+12);
									update_month_and_year();
								},
								html: '<svg width="60" height="30"><polygon points="60,21 0,21 30,7"></polygon></svg>'
							})
						})
					}).add('<tr>', {
						html: $('<td>', {
							style: 'min-width: 170px',
							text: months[time.getMonth()]
						}).add('<td>', {
							text: time.getFullYear()
						})
					}).add('<tr>', {
						html: $('<td>', {
							html: $('<a>', {
								click: function() {
									change_month(-1);
									update_month_and_year();
								},
								html: '<svg width="60" height="30"><polygon points="0,9 60,9 30,23"></polygon></svg>'
							})
						}).add('<td>', {
							html: $('<a>', {
								click: function() {
									change_month(-12);
									update_month_and_year();
								},
								html: '<svg width="60" height="30"><polygon points="0,9 60,9 30,23"></polygon></svg>'
							})
						})
					})
				})
			}), function(modal) {
				modal[0].onmodalclose = update_calendar.bind(null);
			});
			month_chooser.find('td:first-child').on('wheel', function(e) {
				e.preventDefault();
				if (e.originalEvent.deltaY > 0)
					change_month(-1);
				else if (e.originalEvent.deltaY < 0)
					change_month(+1);
				update_month_and_year();
			});
			month_chooser.find('td:last-child').on('wheel', function(e) {
				e.preventDefault();
				if (e.originalEvent.deltaY > 0)
					change_month(-12);
				else if (e.originalEvent.deltaY < 0)
					change_month(+12);
				update_month_and_year();
			});
		}
	});

	var change_month = function(diff_num) {
		// Prevent changing to one month ahead of the intended one
		var day_set = time.getDate();
		time.setDate(10);
		time.setMonth(time.getMonth() + diff_num);
		var month = time.getMonth();
		time.setDate(day_set);
		if (time.getMonth() !== month)
			time.setDate(0); // The day no. is too big, so choose the last day of the month
	};

	var calendar = $('<table>', {
		class: 'calendar',
		html: $('<thead>', {
			html: [
				$('<tr>', {
					html: [
						$('<th>', {
							html: $('<a>', {click: function() {
								change_month(-1);
								update_calendar();
							}, html: '<svg width="12" height="14"><polygon points="12,0 12,14 0,7"></polygon></svg>'})
						}),
						$('<th>', {
							colspan: 5,
							html: header
						}),
						$('<th>', {
							html: $('<a>', {click: function() {
								change_month(+1);
								update_calendar();
							}, html: '<svg width="12" height="14"><polygon points="0,0 0,14 12,7"></polygon></svg>'})
						})
					]
				}).on('wheel', function(e) {
					e.preventDefault();
					if (e.originalEvent.deltaY > 0)
						change_month(-1);
					else if (e.originalEvent.deltaY < 0)
						change_month(+1);
					update_calendar();
				}),
				$('<tr>', {
					html: [
						$('<th>', {text: 'Mon'}),
						$('<th>', {text: 'Tue'}),
						$('<th>', {text: 'Wed'}),
						$('<th>', {text: 'Thu'}),
						$('<th>', {text: 'Fri'}),
						$('<th>', {text: 'Sat'}),
						$('<th>', {text: 'Sun'})
					]
				})
			]
		})
	});

	var change_second = function(diff_num) {
		time.setSeconds(time.getSeconds() + diff_num);
	};
	var time_chooser = $('<table>', {
		class: 'time-chooser',
		html: $('<tbody>', {
			html: [
				$('<tr>', {
					html: [21600,600,10].map(function(diff) {
						return $('<td>', {html:
							$('<a>', { html: '<svg width="60" height="30"><polygon points="50,27 30,19 10,27 30,6"></polygon></svg>'})
							.on('click', function() {
								change_second(+diff);
								update_calendar();
							})
						});
					})
				}), $('<tr>', {
					html: [3600,60,1].map(function(diff) {
						return $('<td>', {html:
							$('<a>', { html: '<svg width="60" height="30"><polygon points="50,19 10,19 30,9"></polygon></svg>'})
							.on('click', function() {
								change_second(+diff);
								update_calendar();
							})
						});
					})
				}), $('<tr>', {
					html: [23,59,59].map(function(max_val, idx) {
						var input = $('<input>', {
							type: 'text',
							maxlength: 2,
							prop: {
								seconds_per_unit: [3600, 60, 1][idx],
								jump_seconds_per_unit: [21600,600,10][idx],
							}
						});
						var input_value_change = function() {
							var val = parseInt(input.val());
							if (isNaN(val) || val < 0 || val > max_val) {
								input.css('background-color', '#ff9393');
								return;
							}
							input.css('background-color', 'initial');

							if (idx === 0)
								time.setHours(val);
							else if (idx === 1)
								time.setMinutes(val);
							else if (idx === 2)
								time.setSeconds(val);

							update_calendar(true, false);
						};
						// Event handlers
						input.on('input', input_value_change);
						input.on('focusout', function() {
							var val = parseInt($(this).val());
							if (isNaN(val) || val < 0)
								val = '00';
							else if (val > max_val)
								val = max_val;
							else if (val < 10)
								val = '0' + val;
							input.val(val);
							input_value_change();
						});

						return $('<td>', {html: input});
					})
				}), $('<tr>', {
					html: [3600,60,1].map(function(diff) {
						return $('<td>', {html:
							$('<a>', { html: '<svg width="60" height="30"><polygon points="10,11 50,11 30,21"></polygon></svg>'})
							.on('click', function() {
								change_second(-diff);
								update_calendar();
							})
						});
					})
				}), $('<tr>', {
					html: [21600,600,10].map(function(diff) {
						return $('<td>', {html:
							$('<a>', { html: '<svg width="60" height="30"><polygon points="10,3 30,11 50,3 30,24"></polygon></svg>'})
							.on('click', function() {
								change_second(-diff);
								update_calendar();
							})
						});
					})
				})
			]
		})
	});
	// Wheel handler on time_chooser
	time_chooser.find('tbody').on('wheel', function(e) {
		e.preventDefault();
		var tr = this.childNodes[2];
		var td = tr.childNodes[$(e.target).closest('td', this).index()];
		var input = td.childNodes[0];

		var sec_delta = (e.shiftKey ? input.jump_seconds_per_unit : input.seconds_per_unit);
		if (e.originalEvent.deltaY > 0)
			change_second(-sec_delta);
		else if (e.originalEvent.deltaY < 0)
			change_second(sec_delta);

		update_calendar();
	});

	// Days table
	var tbody, tbody_date = new Date(time);
	tbody_date.setDate(0); // Change month; this variable is used to skip
	                       // regenerating the whole table when it is unnecessary
	function update_calendar(update_datetime_input, update_time_inputs) {
		if (update_datetime_input !== false)
			$(datetime_info_input).val(date_to_datetime_str(time));

		// Time chooser
		if (update_time_inputs !== false) {
			var x = time_chooser.find('input');
			var foo = function(x) { return x < 10 ? '0' + x : x; };
			x.eq(0).val(foo(time.getHours()));
			x.eq(1).val(foo(time.getMinutes()));
			x.eq(2).val(foo(time.getSeconds()));
		}
		// Header
		header.text(months[time.getMonth()] + ' ' + time.getFullYear());
		// Tbody
		// Faster update - skipping regeneration of tbody
		if (time.getFullYear() === tbody_date.getFullYear() &&
			time.getMonth() === tbody_date.getMonth())
		{
			tbody.find('.chosen').removeClass('chosen');
			tbody.find('td:not(.gray)').eq(time.getDate() - 1).addClass('chosen');
			return;
		}

		tbody_date = new Date(time);
		tbody = $('<tbody>').on('wheel', function(e) {
			e.preventDefault();
			if (e.originalEvent.deltaY > 0)
				time.setDate(time.getDate() - 1);
			else if (e.originalEvent.deltaY < 0)
				time.setDate(time.getDate() + 1);

			update_calendar();
		});

		var day_onclick = function(that_elem_time) {
			var tet = new Date(that_elem_time.valueOf());
			return function() {
				// Update time
				time = new Date(tet.getFullYear(),
					tet.getMonth(),
					tet.getDate(),
					time.getHours(),
					time.getMinutes(),
					time.getSeconds());

				update_calendar();
			};
		};

		var tm = new Date(time.valueOf());
		tm.setDate(1);
		// Move tm back until it is Monday
		while (tm.getDay() != 1)
			tm.setDate(tm.getDate() - 1);
		// Fill all the table
		var passed_selected_day = false;
		var today = new Date();
		while (!passed_selected_day || tm.getMonth() === time.getMonth()) {
			// Add row (all the 7 days)
			var tr = $('<tr>');
			for (var i = 0; i < 7; ++i) {
				var td = $('<td>', {
					html: $('<a>', {
						text: tm.getDate(),
						click: day_onclick(tm)
					})
				});
				// Other month than currently selected
				if (tm.getMonth() !== time.getMonth())
					td.addClass('gray');
				// Mark today
				if (tm.getFullYear() === today.getFullYear() &&
					tm.getMonth() === today.getMonth() &&
					tm.getDate() === today.getDate())
				{
					td.addClass('today');
				}
				// Mark the selected day
				if (tm.getMonth() === time.getMonth() &&
					tm.getDate() === time.getDate())
				{
					passed_selected_day = true;
					td.addClass('chosen');
				}
				tr.append(td);

				tm.setDate(tm.getDate() + 1);
			}
			tbody.append(tr);
		}
		calendar.children('tbody').remove();
		calendar.append(tbody);
	}
	update_calendar();

	var round_up_5_minutes = function() {
		var k = time.getMinutes() + (time.getSeconds() !== 0) + 4;
		time.setMinutes(k - k % 5);
		time.setSeconds(0);
		update_calendar();
	};

	var save_changes = function() {
		$(text_input).val(date_to_datetime_str(time));
		$(hidden_input).val(Math.floor(time.getTime() / 1000));
	};

	var datetime_info_input = $('<input>', {
		type: 'text',
		class: 'calendar-input',
		value: date_to_datetime_str(time)
	});

	modal($('<div>', {
		html: [
			$('<center>').append(datetime_info_input),
			calendar,
			time_chooser,
			$('<center>', {html:
				$('<a>', {
					class: 'btn-small',
					text: 'Round up 5 minutes',
					click: round_up_5_minutes
				})
			}),
			$('<center>', {html:
				$('<a>', {
					class: 'btn-small',
					text: 'Set to now',
					click: function() {
						time = new Date();
						update_calendar();
					}
				})
			}),
			$('<center>', {html:
				$('<a>', {
					class: 'btn blue',
					text: 'Save changes',
					click: function() {
						save_changes();
						close_modal($(this).closest('.modal'));
					}
				})
			}),
		]
	}), function(modal_elem) {
		var keystorke_update = function(e) {
			// Editing combined datetime input field
			if (datetime_info_input.is(':focus')) {
				setTimeout(function() {
					// Dotetime validation
					var miliseconds_dt = Date.parse(datetime_info_input.val());
					if (isNaN(miliseconds_dt)) {
						datetime_info_input.css('background-color', '#ff9393');
					} else {
						// Propagate new datetime to the rest of calendar
						time = new Date(miliseconds_dt);
						datetime_info_input.css('background-color', 'initial');
						update_calendar(false);
						if (e.key == 'Enter') {
							save_changes();
							close_modal(modal_elem);
						}
					}
				});

				return;
			}

			// Time input field
			if (e.target.tagName === 'INPUT' && e.target.seconds_per_unit != null) {
				var sec_delta = (e.shiftKey ? e.target.jump_seconds_per_unit : e.target.seconds_per_unit);
				if (e.key === 'ArrowUp') {
					change_second(sec_delta);
					update_calendar();
				} else if (e.key === 'ArrowDown') {
					change_second(-sec_delta);
					update_calendar();
				}

				return;
			}

			if ($.contains(document.documentElement, month_chooser[0])) {
				if (e.key === 'ArrowUp')
					month_chooser.find('td > a').eq(0).click();
				else if (e.key === 'ArrowDown')
					month_chooser.find('td > a').eq(2).click();
				else
					return;

				e.preventDefault();
				return;
			}

			if (e.key === 'ArrowLeft')
				time.setDate(time.getDate() - 1);
			else if (e.key === 'ArrowRight')
				time.setDate(time.getDate() + 1);
			else if (e.key === 'ArrowUp')
				time.setDate(time.getDate() - 7);
			else if (e.key === 'ArrowDown')
				time.setDate(time.getDate() + 7);
			else
				return;

			e.preventDefault();
			update_calendar();
		};

		$(document).on('keydown', keystorke_update);
		modal_elem[0].onmodalclose = function() {
			$(document).off('keydown', keystorke_update);
			if (date_to_datetime_str(time) != $(text_input).val()) {
				modal($('<p>', {
					style: 'margin: 0; text-align: center',
					text: 'Your change to the datetime will be lost. Unsaved change: ' + date_to_datetime_str(time) + '.'
				}).add('<center>', {
					style: 'margin: 12px auto 0',
					html: $('<a>', {
						class: 'btn-small blue',
						text: 'Save changes',
						click: function() {
							save_changes();
							close_modal($(this).closest('.modal'));
							remove_modals(modal_elem);
						}
					}).add('<a>', {
						class: 'btn-small',
						text: 'Discard changes',
						click: function() {
							close_modal($(this).closest('.modal'));
							remove_modals(modal_elem);
						}
					})
				}), function(modal) {
					modal[0].onmodalclose = function() {
						remove_modals(modal_elem);
					};
				});

				return false;
			}
		};
	});

	datetime_info_input[0].select();
}
function dt_chooser_input(name, allow_neg_inf /* = false */, allow_inf /* = false */, copy_and_paste_buttons /* = false*/, initial_dt /* = undefined <=> use current time, Infinity <=> inf, -Infinity <=> -inf */, neg_inf_button_text /* = 'Set to -inf' */, inf_button_text /* = 'Set to inf' */) {
	/*const*/ var neg_inf_text_to_show = neg_inf_button_text;
	/*const*/ var inf_text_to_show = inf_button_text;

	if (neg_inf_button_text === undefined) {
		neg_inf_button_text = 'Set to -inf';
		neg_inf_text_to_show = '-inf';
	}
	if (inf_button_text === undefined) {
		inf_button_text = 'Set to inf';
		inf_text_to_show = 'inf';
	}

	var now_round_up_5_minutes = function() {
		var dt = new Date();
		// Round to 5 minutes
		var k = dt.getMinutes() + (dt.getSeconds() !== 0) + 4;
		dt.setMinutes(k - k % 5);
		dt.setSeconds(0);
		return dt;
	}

	if (initial_dt === undefined)
		initial_dt = now_round_up_5_minutes();

	var value_input = $('<input>', {
		type: 'hidden',
		name: name
	});

	var open_modal_chooser = function() {
		var date_set = utcdt_or_tm_to_Date(value_input.val());
		if (date_set == Infinity || date_set == -Infinity)
			date_set = now_round_up_5_minutes();
		open_calendar_on(date_set, chooser_input, value_input);
	};

	var chooser_input = $('<input>', {
		type: 'text',
		class: 'calendar-input',
		readonly: true,
		click: open_modal_chooser
	});

	var update_choosen_dt = function(new_dt) {
		if (new_dt === Infinity) {
			value_input.val('+inf');
			chooser_input.val(inf_text_to_show);
		} else if (new_dt === -Infinity) {
			value_input.val('-inf');
			chooser_input.val(neg_inf_text_to_show);
		} else {
			value_input.val(Math.floor(new_dt.getTime() / 1000));
			chooser_input.val(date_to_datetime_str(new_dt));
		}
	};

	update_choosen_dt(initial_dt);

	var buttons = $();
	if (allow_neg_inf) {
		buttons = buttons.add('<a>', {
			class: 'btn-small',
			text: neg_inf_button_text,
			click: update_choosen_dt.bind(null, -Infinity)
		});
	}
	if (allow_inf) {
		buttons = buttons.add('<a>', {
			class: 'btn-small',
			text: inf_button_text,
			click: update_choosen_dt.bind(null, Infinity)
		});
	}
	if (copy_and_paste_buttons) {
		buttons = buttons.add(copy_to_clipboard_btn(true, 'Copy', function() {
			localStorage.setItem('dt_chooser_clipboard', value_input.val());
			return chooser_input.val();
		}));
		buttons = buttons.add('<a>', {
			class: 'btn-small',
			text: 'Paste',
			style: 'position: relative',
			click: function() {
				let val = localStorage.getItem('dt_chooser_clipboard');
				if (val != null) {
					val = utcdt_or_tm_to_Date(val);
					if ((val === Infinity && !allow_inf) || (val == -Infinity && !allow_neg_inf)) {
						append_btn_tooltip($(this), 'Failed: invalid value in sim clipboard!');
					} else {
						update_choosen_dt(val);
						append_btn_tooltip($(this), 'Pasted!');
					}
				} else {
					append_btn_tooltip($(this), 'Failed: no datetime in sim clipboard!');
				}
			}
		});
	}

	return chooser_input.add(value_input).add(buttons);
}
