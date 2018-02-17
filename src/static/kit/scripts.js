/* If you add or edit page / lister / tabmenu / chooser or whatever make sure
   that:
   - centerize_modal() is put everywhere where needed
   - timed_hide_show() is also put everywhere where appropriate (to check you
       could change the timed_hide_delay to something big and see if everything
       show as soon as it is loaded)
   - all tabs (from tab-menus) and deeper / subsequent tabs works well with page
       refreshing
*/
function hex2str(hexx) {
	var hex = hexx.toString(); // force conversion
	var str = '';
	for (var i = 0; i < hex.length; i += 2)
		str += String.fromCharCode(parseInt(hex.substr(i, 2), 16));
	return str;
}
function text_to_safe_html(str) {
	return $('<div>', {text: str}).html();
}
function is_logged_in() {
	return ($('.navbar .rightbar .user + ul > a:first-child').length > 0);
}
function logged_user_id() {
	var x = $('.navbar .rightbar .user + ul > a:first-child').attr('href');
	return x.substr(x.lastIndexOf('/') + 1);
}
function logged_user_is_admin() {
	return ($('.navbar > div > a[href="/logs"]').length === 1);
}
function logged_user_is_tearcher_or_admin() {
	return ($('.navbar > div > a[href="/u"]').length === 1);
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

// $(document).ready(function() {
// 	// Converts datetimes
// 	$('*[datetime]').each(function() {
// 		normalize_datetime($(this),
// 			$(this).parents('.submissions, .problems, .jobs, .files').length == 0);
// 	});
// });
// Clock
$(document).ready(function update_clock() {
	if (update_clock.time_difference === undefined)
		update_clock.time_difference = window.performance.timing.responseStart - start_time;

	var time = new Date();
	time.setTime(time.getTime() - update_clock.time_difference);
	var hours = time.getHours();
	var minutes = time.getMinutes();
	var seconds = time.getSeconds();
	hours = (hours < 10 ? '0' : '') + hours;
	minutes = (minutes < 10 ? '0' : '') + minutes;
	seconds = (seconds < 10 ? '0' : '') + seconds;
	// Update the displayed time
	document.getElementById('clock').innerHTML = String().concat(hours, ':', minutes, ':', seconds, tz_marker());
	setTimeout(update_clock, 1000 - time.getMilliseconds());
});
// Handle navbar correct size
function normalize_navbar() {
	var navbar = $('.navbar');
	navbar.css('width', 'auto');

	if (navbar.outerWidth() <= $(window).width()) {
		navbar.css('width', '100%');
		navbar.css('position', 'fixed');
	} else {
		navbar.css('position', 'absolute');
		navbar.outerWidth($(document).width());
	}
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

// Adding csrf token to a form
function add_csrf_token_to(form) {
	var x = $(form);
	x.find('input[name="csrf_token"]').remove(); // Avoid duplication
	x.append('<input type="hidden" name="csrf_token" value="' +
		get_cookie('csrf_token') + '">');
	return form;
}

// Adding csrf token just before submitting a form
$(document).ready(function() {
	$('form').submit(function() { add_csrf_token_to(this); });
});

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

		var l = 0, r = this_.data.length;
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

/* ============================ URL hash parser ============================ */
var url_hash_parser = {};
(function () {
	var args = window.location.hash; // Must begin with '#'
	if (args == '')
		args = '#';
	var beg = 0; // Points to the '#' just before the next argument

	this.next_arg  = function() {
		var pos = args.indexOf('#', beg + 1);
		if (pos === -1)
			return args.substr(beg + 1);

		return args.substring(beg + 1, pos);
	};

	this.extract_next_arg  = function() {
		var pos = args.indexOf('#', beg + 1), res;
		if (pos === -1) {
			if (beg >= args.length)
				return '';

			res = args.substr(beg + 1);
			beg = args.length;
			return res;
		}

		res = args.substring(beg + 1, pos);
		beg = pos;
		return res;
	};

	this.empty = function() { return (beg >= args.length); };

	this.assign = function(new_hash) {
		beg = 0;
		if (new_hash.charAt(0) !== '#')
			args = '#' + new_hash;
		else
			args = new_hash;
	};

	this.assign_as_parsed = function(new_hash) {
		if (new_hash.charAt(0) !== '#')
			args = '#' + new_hash;
		else
			args = new_hash;
		beg = args.length;
	};


	this.append = function(next_args) {
		if (next_args.charAt(0) !== '#')
			args += '#' + next_args;
		else
			args += next_args;
	};

	this.parsed_prefix = function() { return args.substr(0, beg); };
}).call(url_hash_parser);

/* =========================== History management =========================== */
var egid = {};
(function () {
	var id = 0;
	this.next_from = function(egid) {
		var egid_id = String(egid).replace(/.*-/, '');
		return String(egid).replace(/[^-]*$/, ++egid_id);
	};

	this.get_next = function() {
		return ''.concat(window.performance.timing.responseStart, '-', id++);
	};
}).call(egid);

function give_body_egid() {
	if ($('body').attr('egid') !== undefined)
		return;

	var body_egid = egid.get_next();
	$('body').attr('egid', body_egid);
	window.history.replaceState({egid: body_egid}, document.title, document.URL);
}
$(document).ready(give_body_egid);

window.onpopstate = function(event) {
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

	// Remove other modals that cover elem
	var view_element_removed = false;
	modals.each(function() {
		var elem = $(this);
		if (elem.hasClass('view')) {
			if (view_element_removed)
				return false;
			view_element_removed = true;
		}

		remove_modals(elem);
	});
};

/* const */ var fade_in_duration = 50; // milliseconds

// For best effect, the values below should be the same
/* const */ var loader_show_delay = 400; // milliseconds
/* const */ var timed_hide_delay = 400; // milliseconds

/* ================================= Loader ================================= */
function remove_loader(elem) {
	$(elem).children('.loader, .loader-info').remove();
}
function append_loader(elem) {
	remove_loader(elem);
	elem = $(elem);
	if (elem.css('animationName') === undefined &&
		elem.css('WebkitAnimationName') === undefined)
	{
		$(elem).append('<img class="loader" src="/kit/img/loader.gif">');
	} else
		$(elem).append($('<span>', {
			class: 'loader',
			style: 'display: none',
			html: $('<div>', {class: 'spinner'})
		}).delay(loader_show_delay).fadeIn(fade_in_duration));
}
function show_success_via_loader(elem, html) {
	elem = $(elem);
	remove_loader(elem);
	elem.append($('<span>', {
		class: 'loader-info success',
		style: 'display:none',
		html: html
	}).fadeIn(fade_in_duration));
}
function show_error_via_loader(elem, response, err_status, try_again_handler) {
	if (err_status == 'success' || err_status == 'error' || err_status === undefined)
		err_status = '';
	else
		err_status = '; ' + err_status;

	elem = $(elem);
	remove_loader(elem);
	elem.append($('<span>', {
		class: 'loader-info error',
		style: 'display:none',
		html: $('<span>', {
			text: "Error: " + response.status + ' ' + response.statusText + err_status
		}).add(try_again_handler === undefined ? '' : $('<a>', {
			text: 'Try again',
			click: try_again_handler
		}))
	}).fadeIn(fade_in_duration));

	// Additional message
	var x = elem.find('.loader-info > span');
	try {
		var msg = $($.parseHTML(response.responseText)).text();

		if (msg != '')
			x.text(x.text().concat("\nInfo: ", msg));

	} catch (err) {
		if (response.responseText != '' && // There is a message
			response.responseText.lastIndexOf('<!DOCTYPE html>', 0) !== 0 && // Message is not a whole HTML page
			response.responseText.lastIndexOf('<!doctype html>', 0) !== 0) // Message is not a whole HTML page
		{
			x.text(x.text().concat("\nInfo: ",
				response.responseText));
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
var Form = {};
(function() {
	this.field_group = function(label, input_context_or_input) {
		var input;
		if (input_context_or_input instanceof jQuery)
			input = input_context_or_input;
		else
			input = $('<input>', input_context_or_input);

		var id;
		if (input.is('input[type="checkbox"]')) {
			id = 'checkbox' + get_unique_id();
			input.attr('id', id);
		}

		return $('<div>', {
			class: 'field-group',
			html: [
				$('<label>', {
					text: label,
					for: id
				}),
				input
			]
		});
	};

	this.send_via_ajax = function(form, url, success_msg /*= 'Success'*/, loader_parent)
	{
		if (success_msg === undefined)
			success_msg = 'Success';
		if (loader_parent === undefined)
			loader_parent = $(form);

		form = $(form);
		add_csrf_token_to(form);
		append_loader(loader_parent);

		$.ajax({
			type: 'POST',
			url: url,
			processData: false,
			contentType: false,
			data: new FormData(form[0]),
			success: function(resp) {
				if (typeof success_msg === "function") {
					success_msg.call(form, resp);
				} else
					show_success_via_loader(loader_parent, success_msg);
			},
			error: function(resp, status) {
				show_error_via_loader(loader_parent, resp, status);
			}
		});
		return false;
	};
}).call(Form);

function ajax_form(title, target, html, success_msg, classes) {
	return $('<div>', {
		class: 'form-container' + (classes === undefined ? '' : ' ' + classes),
		html: $('<h1>', {text: title})
		.add('<form>', {
			method: 'post',
			html: html
		}).submit(function() {
			return Form.send_via_ajax(this, target, success_msg);
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
		if (this.onmodalclose !== undefined)
			this.onmodalclose();
	});
	if (modal.is('.view'))
		window.history.back();
	else
		remove_modals(modal);
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

	return mod.appendTo('body').fadeIn(fade_in_duration);
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

function API_call(ajax_url, success_handler, loader_parent) {
	var thiss = this;
	var args = arguments;
	append_loader(loader_parent);
	$.ajax({
		type: 'GET',
		url: ajax_url,
		dataType: 'json',
		success: function(data) {
			remove_loader(loader_parent);
			success_handler.apply(this, arguments);
		},
		error: function(resp, status) {
			show_error_via_loader(loader_parent, resp, status,
				setTimeout.bind(null, API_call.bind(null, thiss), 0)); // Avoid recursion
		}
	});
}

/* ================================ Tab menu ================================ */
function tabname_to_hash(tabname) {
	return tabname.toLowerCase().replace(/ /g, '_');
}
/// Triggers 'tabmenuTabHasChanged' event on the result every time an active tab is changed
function tabmenu(attacher, tabs) {
	var res = $('<div>', {class: 'tabmenu'});
	/*const*/ var prior_hash = url_hash_parser.parsed_prefix();

	function set_min_width(elem) {
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
		return function() {
			if (!$(this).hasClass('active')) {
				set_min_width(this);
				$(this).parent().children('.active').removeClass('active');
				$(this).addClass('active');

				window.history.replaceState(window.history.state, '',
					document.URL.substr(0, document.URL.length - window.location.hash.length) + prior_hash + '#' + tabname_to_hash($(this).text()));
				url_hash_parser.assign_as_parsed(window.location.hash);

				res.trigger('tabmenuTabHasChanged', this);
				handler.call(this);
				centerize_modal($(this).parents('.modal'), false);
			}
		};
	};
	for (var i = 0; i < tabs.length; i += 2)
		res.append($('<a>', {
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
		window.history.pushState({egid: next_egid}, '', new_window_location);
		url_hash_parser.assign(window.location.hash);

		elem.appendTo('body').fadeIn(fade_in_duration);
		func.call(elem.find('div > div'));
		centerize_modal(elem);

	// Use body as the parent element
	} else if (no_modal_elem === undefined || $(no_modal_elem).is('body')) {
		give_body_egid();
		window.history.replaceState({egid: $('body').attr('egid')}, document.title, new_window_location);
		url_hash_parser.assign(window.location.hash);

		func.call($('body'));

	// Use @p no_modal_elem as the parent element
	} else {
		next_egid = egid.get_next();
		$(no_modal_elem).attr('egid', next_egid);
		window.history.pushState({egid: next_egid}, '', new_window_location);
		url_hash_parser.assign(window.location.hash);

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
function view_ajax(as_modal, ajax_url, success_handler, new_window_location, no_modal_elem /*= document.body*/, show_on_success /*= true */) {
	view_base(as_modal, new_window_location, function() {
		var elem = $(this);
		var modal = elem.parent().parent();
		if (as_modal)
			timed_hide(modal);
		API_call(ajax_url, function () {
			if (as_modal && (show_on_success !== false))
				timed_hide_show(modal);
			success_handler.apply(elem, arguments);
			if (as_modal)
				centerize_modal(modal);
		}, elem);
	}, no_modal_elem);
}
function a_view_button(href, text, classes, func) {
	return $('<a>', {
			'href': href,
			'text': text,
			class: classes,
			click: function(event) {
				if (event.ctrlKey)
					return true; // Allow opening the link in a new tab
				func();
				return false;
			}
		});
}

/* ================================= Lister ================================= */
function Lister(elem) {
	var this_ = this;
	this.elem = $(elem);
	var lock = false;

	this.fetch_more = function() {
		if (lock || !$.contains(document.documentElement, this_.elem[0]))
			return;

		lock = true;
		append_loader(this_.elem.parent());

		$.ajax({
			type: 'GET',
			url: this_.query_url + this_.query_suffix,
			dataType: 'json',
			success: function(data) {
				var modal = this_.elem.parents('.modal');
				this_.process_api_response(data, modal);

				remove_loader(this_.elem.parent());
				timed_hide_show(modal);
				centerize_modal(modal, false);

				if (data.length == 0)
					return; // No more data to load

				lock = false;
				if (this_.elem.height() - $(window).height() <= 300) {
					// Load more if scrolling down did not become possible
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
		var scres_unhandle = function() {
			if (!$.contains(document.documentElement, this_.elem)) {
				$(this).off('scroll', scres_handler);
				$(this).off('resize', scres_handler);
				return;
			}
		};
		var modal_parent = this_.elem.closest('.modal');
		if (modal_parent.length === 1) {
			scres_handler = function() {
				scres_unhandle();
				var height = $(this).children('div').height();
				var scroll_top = $(this).scrollTop();
				if (height - $(window).height() - scroll_top <= 300)
					this_.fetch_more();
			};

			modal_parent.on('scroll', scres_handler);
			$(window).on('resize', scres_handler);

		} else {
			scres_handler = function() {
				scres_unhandle();
				var x = $(document);
				if (x.height() - $(window).height() - x.scrollTop() <= 300)
					this_.fetch_more();
			};

			$(document).on('scroll', scres_handler);
			$(window).on('resize', scres_handler);
		}
	};
}

////////////////////////////////////////////////////////////////////////////////

/* ================================== Logs ================================== */
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
		if (contains(i, '\033[31m')) {
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

	var process_data = function(data) {
		data = String(data).split('\n');
		if (first_offset === undefined)
			first_offset = data[0];
		else if (data[0] > first_offset) { // Newer logs arrived
			first_offset = data[0];
			this_.elem.empty();
		}

		offset = data[0];
		data = hex2str(data[1]);

		var prev_height = this_.elem[0].scrollHeight;
		var prev = prev_height - this_.elem.scrollTop();

		remove_loader(this_.elem);
		this_.elem.html(colorize(text_to_safe_html(data) + this_.elem.html(),
			data.length + 2000));
		var curr_height = this_.elem[0].scrollHeight;
		this_.elem.scrollTop(curr_height - prev);

		// Load more logs if scrolling up did not become possible
		if (offset > 0) {
			lock = false;
			if (this_.elem.innerHeight() >= curr_height || prev_height == curr_height) {
				// avoid recursion
				setTimeout(this_.fetch_more, 0);
			}
		}
	};

	this.fetch_more = function() {
		if (lock)
			return;
		lock = true;

		append_loader(this_.elem);
		$.ajax({
			type: 'GET',
			url: '/api/logs/' + this_.type +
				(offset === undefined ? '' : '?' + offset),
			success: process_data,
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

		append_loader(this_.elem);
		$.ajax({
			type: 'GET',
			url: '/api/logs/' + this_.type,
			success: function(data) {
				if (String(data).split('\n')[0] !== first_offset)
					return process_data(data);

				remove_loader(this_.elem);
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

	setInterval(function() {
		var elem = this_.elem[0];
		if (elem.scrollHeight - elem.scrollTop === elem.clientHeight &&
			auto_refresh_checkbox.is(':checked'))
		{
			this_.try_fetching_newest();
		}
	}, 2000);

	this.monitor_scroll = function() {
		this_.elem.on('scroll', function() {
			if ($(this).scrollTop() <= 300)
				this_.fetch_more();
		});
	};

	this.fetch_more();
}

/* ============================ Actions buttons ============================ */
var ActionsToHTML = {};
(function() {
	this.job = function(job_id, actions_str, problem_id, job_view /*= false*/) {
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
							href: '/api/job/' + job_id + '/log',
							text: 'Job log'
						}), actions_str.indexOf('u') === -1 ? '' : $('<a>', {
							href: '/api/job/' + job_id + '/uploaded-package',
							text: 'Uploaded package'
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
			res.push($('<a>', {
				class: 'btn-small orange',
				text: 'Restart job',
				click: restart_job.bind(null, job_id)
			}));

		return res;
	};

	this.user = function(user_id, actions_str, user_view /*= false*/) {
		if (user_view === undefined)
			user_view = false;

		var res = [];
		if (!user_view && actions_str.indexOf('v') !== -1)
			res.push(a_view_button('/u/' + user_id, 'View', 'btn-small',
				view_user.bind(null, true, user_id)));

		if (actions_str.indexOf('E') !== -1)
			res.push(a_view_button('/u/' + user_id + '/edit', 'Edit',
				'btn-small blue', edit_user.bind(null, true, user_id)));

		if (actions_str.indexOf('D') !== -1)
			res.push(a_view_button('/u/' + user_id + '/delete', 'Delete',
				'btn-small red', delete_user.bind(null, true, user_id)));

		if (actions_str.indexOf('P') !== -1 || actions_str.indexOf('p') !== -1)
			res.push(a_view_button('/u/' + user_id + '/change-password',
				'Change password', 'btn-small orange',
				change_user_password.bind(null, true, user_id)));

		return res;
	};

	this.submission = function(submission_id, actions_str, submission_type, submission_view /*= false*/) {
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

			res.push($('<a>', {
				class: 'btn-small',
				href: '/api/submission/' + submission_id + '/download',
				text: 'Download'
			}));
		}

		if (actions_str.indexOf('C') !== -1)
			res.push(a_view_button('/s/' + submission_id + '/chtype', 'Change type',
				'btn-small orange',
				submission_chtype.bind(null, submission_id, submission_type)));

		if (actions_str.indexOf('R') !== -1)
			res.push($('<a>', {
				class: 'btn-small blue',
				text: 'Rejudge',
				click: rejudge_submission.bind(null, submission_id)
			}));

		if (actions_str.indexOf('D') !== -1)
			res.push($('<a>', {
				class: 'btn-small red',
				text: 'Delete',
				click: delete_submission.bind(null, submission_id)
			}));

		return res;
	};

	this.problem = function(problem_id, actions_str, problem_name, problem_view /*= false*/) {
		if (problem_view === undefined)
			problem_view = false;

		var res = [];
		if (!problem_view && actions_str.indexOf('v') !== -1)
			res.push(a_view_button('/p/' + problem_id, 'View', 'btn-small',
				view_problem.bind(null, true, problem_id)));

		if (actions_str.indexOf('V') !== -1)
			res.push($('<a>', {
				class: 'btn-small',
				href: '/api/problem/' + problem_id + '/statement/' + encodeURIComponent(problem_name),
				text: 'Statement'
			}));

		if (actions_str.indexOf('S') !== -1)
			res.push(a_view_button('/p/' + problem_id + '/submit', 'Submit',
				'btn-small blue', add_submission.bind(null, true, problem_id,
					problem_name, (actions_str.indexOf('i') !== -1))));

		if (actions_str.indexOf('s') !== -1)
			res.push(a_view_button('/p/' + problem_id + '#all_submissions#solutions',
				'Solutions', 'btn-small', view_problem.bind(null, true, problem_id,
					'#all_submissions#solutions')));

		if (problem_view && actions_str.indexOf('d') !== -1)
			res.push($('<a>', {
				class: 'btn-small',
				href: '/api/problem/' + problem_id + '/download',
				text: 'Download'
			}));

		if (actions_str.indexOf('E') !== -1)
			res.push(a_view_button('/p/' + problem_id + '/edit', 'Edit',
				'btn-small blue', edit_problem.bind(null, true, problem_id)));

		if (problem_view && actions_str.indexOf('R') !== -1)
			res.push(a_view_button('/p/' + problem_id + '/reupload', 'Reupload',
				'btn-small orange', reupload_problem.bind(null, true, problem_id)));

		if (problem_view && actions_str.indexOf('J') !== -1)
			res.push($('<a>', {
				class: 'btn-small blue',
				text: 'Rejudge all submissions',
				click: rejudge_problem_submissions.bind(null, problem_id)
			}));

		if (problem_view && actions_str.indexOf('D') !== -1)
			res.push(a_view_button('/p/' + problem_id + '/delete', 'Delete',
				'btn-small red', delete_problem.bind(null, true, problem_id)));

		return res;
	};

	this.contest = function(contest_id, actions_str, contest_view /*= false*/) {
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
}).call(ActionsToHTML);

/* ================================= Users ================================= */
function add_user(as_modal) {
	view_base(as_modal, '/u/add', function() {
		this.append(ajax_form('Add user', '/api/user/add',
			Form.field_group('Username', {
				type: 'text',
				name: 'username',
				size: 24,
				// maxlength: 'TODO...',
				required: true
			}).add(Form.field_group('Type',
				$('<select>', {
					name: 'type',
					required: true,
					html: $('<option>', {
						value: 'A',
						text: 'Admin',
					}).add('<option>', {
						value: 'T',
						text: 'Teacher',
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
				required: true
			})).add(Form.field_group('Last name', {
				type: 'text',
				name: 'last_name',
				size: 24,
				// maxlength: 'TODO...',
				required: true
			})).add(Form.field_group('Email', {
				type: 'email',
				name: 'email',
				size: 24,
				// maxlength: 'TODO...',
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
	view_ajax(as_modal, '/api/users/=' + user_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		data = data[0];

		this.append($('<div>', {
			class: 'header',
			html: $('<span>', {
				style: 'margin: auto 0',
				html: $('<a>', {
					href: '/u/' + user_id,
					text: data[1]
				})
			}).append(text_to_safe_html(' (' + data[2] + ' ' + data[3] + ')'))
			.add('<div>', {
				html: ActionsToHTML.user(user_id, data[6], true)
			})
		})).append($('<center>', {
			class: 'always_in_view',
			html: $('<div>', {
				class: 'user-info',
				html: $('<div>', {
					class: 'first-name',
					html: $('<label>', {text: 'First name'})
				}).append(text_to_safe_html(data[2]))
				.add($('<div>', {
					class: 'last-name',
					html: $('<label>', {text: 'Last name'})
				}).append(text_to_safe_html(data[3])))
				.add($('<div>', {
					class: 'username',
					html: $('<label>', {text: 'Username'})
				}).append(text_to_safe_html(data[1])))
				.add($('<div>', {
					class: 'type',
					html: $('<label>', {text: 'Account type'}).add('<span>', {
						class: data[5],
						text: String(data[5]).slice(0, 1).toUpperCase() +
							String(data[5]).slice(1)
					})
				}))
				.add($('<div>', {
					class: 'email',
					html: $('<label>', {text: 'Email'})
				}).append(text_to_safe_html(data[4])))
			})
		}));

		var main = $(this);

		tabmenu(function(elem) { elem.appendTo(main); }, [
			'Submissions', function() {
				$(this).parent().next().remove();
				main.append($('<div>', {html: "<h2>User's submissions</h2>"}));
				tab_submissions_lister(main.children().last(), '/u' + user_id);
			},
			'Jobs', function() {
				$(this).parent().next().remove();
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
	view_ajax(as_modal, '/api/users/=' + user_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		data = data[0];

		var actions = data[6];
		if (actions.indexOf('E') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Edit account', '/api/user/' + user_id + '/edit',
			Form.field_group('Username', {
				type: 'text',
				name: 'username',
				value: data[1],
				size: 24,
				// maxlength: 'TODO...',
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
								selected: ('admin' === data[5] ? true : undefined)
							}));

						if (actions.indexOf('T') !== -1)
							res.push($('<option>', {
								value: 'T',
								text: 'Teacher',
								selected: ('teacher' === data[5] ? true : undefined)
							}));

						if (actions.indexOf('N') !== -1)
							res.push($('<option>', {
								value: 'N',
								text: 'Normal',
								selected: ('normal' === data[5] ? true : undefined)
							}));

						return res;
					}()
				})
			)).add(Form.field_group('First name', {
				type: 'text',
				name: 'first_name',
				value: data[2],
				size: 24,
				// maxlength: 'TODO...',
				required: true
			})).add(Form.field_group('Last name', {
				type: 'text',
				name: 'last_name',
				value: data[3],
				size: 24,
				// maxlength: 'TODO...',
				required: true
			})).add(Form.field_group('Email', {
				type: 'email',
				name: 'email',
				value: data[4],
				size: 24,
				// maxlength: 'TODO...',
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
	view_ajax(as_modal, '/api/users/=' + user_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		data = data[0];

		var actions = data[6];
		if (actions.indexOf('D') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		var title = 'Delete user ' + user_id;
		var confirm_text = 'Delete user';
		var p = $('<p>', {
			style: 'margin: 0 0 20px; text-align: center',
			html: 'You are going to delete the user '
		}).append(a_view_button('/u/' + user_id, data[1], undefined,
			view_user.bind(null, true, user_id))).append('. As it cannot be undone,<br>you have to confirm it with YOUR password.');

		if (user_id == logged_user_id()) {
			title = confirm_text = 'Delete account';
			p.html('You are going to delete your account. As it cannot be undone,<br>you have to confirm it with your password.');
		}

		this.append(ajax_form(title,
			'/api/user/' + user_id + '/delete',
			p.add(Form.field_group('Your password', {
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
					click: (as_modal ? function() {
						close_modal($(this).closest('.modal'));
					} : undefined)
				})
			}), 'Deletion succeeded'));

	}, '/u/' + user_id + "/delete");
}
function change_user_password(as_modal, user_id) {
	view_ajax(as_modal, '/api/users/=' + user_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		data = data[0];

		var actions = data[6];
		if (actions.indexOf('P') === -1 && actions.indexOf('p') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		if (actions.indexOf('P') === -1 && $('.navbar .user > strong').text() != data[1])
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
function UsersLister(elem, query_suffix /*= ''*/) {
	var this_ = this;
	if (query_suffix === undefined)
		query_suffix = '';

	Lister.call(this, elem);
	this.query_url = '/api/users' + query_suffix;
	this.query_suffix = '';

	this.process_api_response = function(data, modal) {
		if (this_.elem.children('thead').length === 0) {
			if (data.length == 0) {
				this_.elem.parent().append($('<center>', {
					class: 'users always_in_view',
					// class: 'users',
					html: '<p>There are no users to show...</p>'
				}));
				remove_loader(this_.elem.parent());
				timed_hide_show(modal);
				return;
			}

			this_.elem.html('<thead><tr>' +
					'<th>Id</th>' +
					'<th class="username">Username</th>' +
					'<th class="first-name">First name</th>' +
					'<th class="last-name">Last name</th>' +
					'<th class="email">Email</th>' +
					'<th class="type">Type</th>' +
					'<th class="actions">Actions</th>' +
				'</tr></thead><tbody></tbody>');
		}

		for (var x in data) {
			x = data[x];
			this_.query_suffix = '/>' + x[0];

			var row = $('<tr>');
			row.append($('<td>', {text: x[0]}));
			row.append($('<td>', {text: x[1]}));
			row.append($('<td>', {text: x[2]}));
			row.append($('<td>', {text: x[3]}));
			row.append($('<td>', {text: x[4]}));
			row.append($('<td>', {
				class: x[5],
				text: String(x[5]).slice(0, 1).toUpperCase() +
					String(x[5]).slice(1)
			}));

			// Actions
			row.append($('<td>', {
				html: ActionsToHTML.user(x[0], x[6])
			}));

			this_.elem.children('tbody').append(row);
		}
	};

	this.fetch_more();
}
function tab_users_lister(parent_elem, query_suffix /*= ''*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	parent_elem = $(parent_elem);
	function retab(tab_qsuff) {
		parent_elem.children('.users, .loader, .loader-info').remove();
		var table = $('<table class="users stripped"></table>').appendTo(parent_elem);
		new UsersLister(table, query_suffix + tab_qsuff).monitor_scroll();
	}

	var tabs = [
		'All', retab.bind(null, ''),
		'Admins', retab.bind(null, '/tA'),
		'Teachers', retab.bind(null, '/tT'),
		'Normal', retab.bind(null, '/tN')
	];

	tabmenu(function(elem) { elem.appendTo(parent_elem); }, tabs);
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
	view_ajax(as_modal, '/api/jobs/=' + job_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		data = data[0];

		function info_html(info) {
			var td = $('<td>', {
				style: 'text-align:left'
			});
			for (var name in info) {
				td.append($('<label>', {text: name}));

				if (name == "submission")
					td.append(a_view_button('/s/' + info[name], info[name],
						undefined, view_submission.bind(null, true, info[name])));
				else if (name == "problem")
					td.append(a_view_button('/p/' + info[name], info[name],
						undefined, view_problem.bind(null, true, info[name])));
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
					html: ActionsToHTML.job(job_id, data[8], data[7].problem, true)
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
							text: data[2]
						}).add(normalize_datetime($('<td>', {
								datetime: data[1],
								text: data[1]
							}), true)
						).add('<td>', {
							class: 'status ' + data[3][0],
							text: data[3][1]
						}).add('<td>', {
							html: data[5] === null ? 'System' :
								(data[6] == null ? 'Deleted (id: ' + data[5] + ')'
								: a_view_button(
								'/u/' + data[5], data[6], undefined,
								view_user.bind(null, true, data[5])))
						}).add('<td>', {
							html: info_html(data[7])
						})
					})
				})
			})
		}));

		if (data[8].indexOf('r') !== -1) {
			this.append('<h2>Job log</h2>')
			.append($('<pre>', {
				class: 'job-log',
				html: colorize(text_to_safe_html(data[9][1]))
			}));

			if (data[9][0])
				this.append($('<p>', {
					text: 'The job log is too large to show it entirely here. If you want to see the whole, click: '
				}).append($('<a>', {
					class: 'btn-small',
					href: '/api/job/' + job_id + '/log',
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

	Lister.call(this, elem);
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
				remove_loader(this_.elem.parent());
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
			this_.query_suffix = '/<' + x[0];

			var row = $('<tr>');
			row.append($('<td>', {text: x[0]}));
			row.append($('<td>', {text: x[2]}));
			row.append($('<td>', {text: x[4]}));
			row.append($('<td>', {
				html: normalize_datetime(
					a_view_button('/jobs/' + x[0], x[1], undefined,
						view_job.bind(null, true, x[0])).attr('datetime', x[1]),
					false)
			}));
			row.append($('<td>', {
				class: 'status ' + x[3][0],
				text: x[3][1]
			}));
			row.append($('<td>', {
				html: x[5] === null ? 'System' : (x[6] == null ? x[5]
					: a_view_button('/u/' + x[5], x[6], undefined,
						view_user.bind(null, true, x[5])))
			}));
			// Info
			var info = x[7];
			{
				/* jshint loopfunc: true */
				var td = $('<td>');
				var append_tag = function(name, val) {
					td.append($('<label>', {text: name}));
					td.append(val);
				};

				if (info.submission !== undefined)
					append_tag('submission',
						a_view_button('/s/' + info.submission,
							info.submission, undefined,
							view_submission.bind(null, true, info.submission)));

				if (info.problem !== undefined)
					append_tag('problem',
						a_view_button('/p/' + info.problem,
							info.problem, undefined,
							view_problem.bind(null, true, info.problem)));

				var names = ['name', 'memory limit', 'problem type'];
				for (var idx in names) {
					var ni = names[idx];
					if (info[ni] !== undefined)
						append_tag(ni, info[ni]);
				}

				row.append(td);
			}

			// Actions
			row.append($('<td>', {
				html: ActionsToHTML.job(x[0], x[8], info.problem)
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
		parent_elem.children('.jobs, .loader, .loader-info').remove();
		var table = $('<table class="jobs"></table>').appendTo(parent_elem);
		new JobsLister(table, query_suffix + tab_qsuff).monitor_scroll();
	}

	var tabs = [
		'All', retab.bind(null, ''),
		'My', retab.bind(null, '/u' + logged_user_id())
	];

	tabmenu(function(elem) { elem.appendTo(parent_elem); }, tabs);
}

/* ============================== Submissions ============================== */
function add_submission(as_modal, problem_id, problem_name_to_show, maybe_ignored, contest_problem_id) {
	view_base(as_modal, (contest_problem_id === undefined ?
		'/p/' + problem_id + '/submit' : '/c/p' + contest_problem_id + '/submit'), function() {
		this.append(ajax_form('Submit a solution', '/api/submission/add/p' +
				problem_id + (contest_problem_id === undefined ? '' : '/cp' +
				contest_problem_id),
			Form.field_group('Problem', a_view_button('/p/' + problem_id,
				problem_name_to_show, undefined, view_problem.bind(null, true, problem_id))
			).add(Form.field_group('Solution', {
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
				name: 'ignored'
			}) : $()).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Submit'
				})
			}), function(resp) {
				if (as_modal) {
					show_success_via_loader(this, 'Submitted');
					view_submission(true, resp);
				} else {
					this.parent().remove();
					window.location.href = '/s/' + resp;
				}
			})
		);
	});
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
	view_ajax(as_modal, '/api/submissions/=' + submission_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		data = data[0];
		var actions = data[15];

		if (data[6] !== null)
			this.append($('<div>', {
				class: 'contest-path',
				html: [
					a_view_button('/c/c' + data[10], data[11], '',
						view_contest.bind(null, true, data[10])),
					' / ',
					a_view_button('/c/r' + data[8], data[9], '',
						view_contest_round.bind(null, true, data[8])),
					' / ',
					a_view_button('/c/p' + data[6], data[7], '',
						view_contest_problem.bind(null, true, data[6])),
					$('<a>', {
						class: 'btn-small',
						href: '/api/contest/p' + data[6] + '/statement/' +
							encodeURIComponent(data[7]),
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
							html: ActionsToHTML.submission(submission_id, actions,
								data[1], true)
						})
					]
				}),
				$('<table>', {
					html: [
						$('<thead>', {html: '<tr>' +
							(data[2] === null ? ''
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
								class: (data[1] === 'Ignored' ? 'ignored' : undefined),
								html: [
									(data[2] === null ? '' : $('<td>', {
										html: [a_view_button('/u/' + data[2], data[3],
												undefined, view_user.bind(null, true, data[2])),
											' (' + text_to_safe_html(data[16]) + ' ' +
												text_to_safe_html(data[17]) + ')'
										]
									})),
									$('<td>', {
										html: a_view_button('/p/' + data[4], data[5],
											undefined, view_problem.bind(null, true, data[4]))
									}),
									normalize_datetime($('<td>', {
										datetime: data[12],
										text: data[12]
									}), true),
									$('<td>', {
										class: 'status ' + data[13][0],
										text: (data[13][0].lastIndexOf('initial') === -1 ?
											'' : 'Initial: ') + data[13][1]
									}),
									$('<td>', {text: data[14]}),
									$('<td>', {text: data[1]})

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
				elem.children('.tabmenu').nextAll().remove();
				elem.append($('<div>', {
					class: 'results',
					html: function() {
						var res = [data[18], data[19]];
						if (data[19] === null)
							res.push($('<h2>', {text: 'Final testing report'}),
								$('<p>', {text: 'Final testing report will be visible since: '}).append(
									normalize_datetime($('<span>', {datetime: data[20]}))));
						return res;
					}()
				}));
			}
		];

		var cached_source;
		if (actions.indexOf('s') !== -1)
			tabs.push('Source', function() {
				timed_hide_show(elem.parents('.modal'));
				elem.children('.tabmenu').nextAll().remove();
				if (cached_source !== undefined)
					elem.append(cached_source);
				else {
					append_loader(elem);
					$.ajax({
						url: '/api/submission/' + submission_id + '/source',
						dataType: 'html',
						success: function(data) {
							cached_source = data;
							elem.append(data);
							remove_loader(elem);
							centerize_modal(elem.closest('.modal'), false);
						},
						error: function(resp, status) {
							show_error_via_loader(elem, resp, status);
						}
					});
				}
			});

		if (actions.indexOf('j') !== -1)
			tabs.push('Related jobs', function() {
				elem.children('.tabmenu').nextAll().remove();
				elem.append($('<table>', {class: 'jobs'}));
				new JobsLister(elem.children().last(), '/s' + submission_id).monitor_scroll();
			});

		tabmenu(function(x) { x.appendTo(elem); }, tabs);

	}, '/s/' + submission_id + (opt_hash === undefined ? '' : opt_hash), undefined, false);
}
function SubmissionsLister(elem, query_suffix /*= ''*/) {
	var this_ = this;
	if (query_suffix === undefined)
		query_suffix = '';

	this.show_user = (query_suffix.indexOf('/u') === -1 &&
		query_suffix.indexOf('/tS') === -1);

	this.show_contest = (query_suffix.indexOf('/C') === -1 &&
		query_suffix.indexOf('/R') === -1 &&
		query_suffix.indexOf('/P') === -1);

	Lister.call(this, elem);
	this.query_url = '/api/submissions' + query_suffix;
	this.query_suffix = '';

	this.process_api_response = function(data, modal) {
		if (this_.elem.children('thead').length === 0) {
			if (data.length === 0) {
				this_.elem.parent().append($('<center>', {
					class: 'submissions always_in_view',
					// class: 'submissions',
					html: '<p>There are no submissions to show...</p>'
				}));
				remove_loader(this_.elem.parent());
				timed_hide_show(modal);
				return;
			}

			this_.elem.html('<thead><tr>' +
					'<th>Id</th>' +
					(this_.show_user ? '<th class="username">Username</th>' : '') +
					'<th class="time">Added</th>' +
					'<th class="problem">Problem</th>' +
					'<th class="status">Status</th>' +
					'<th class="score">Score</th>' +
					'<th class="type">Type</th>' +
					'<th class="actions">Actions</th>' +
				'</tr></thead><tbody></tbody>');
			add_tz_marker(this_.elem.find('thead th.time'));
		}

		for (var x in data) {
			x = data[x];
			this_.query_suffix = '/<' + x[0];

			var row = $('<tr>', {
				class: (x[1] === 'Ignored' ? 'ignored' : undefined)
			});
			// Id
			row.append($('<td>', {text: x[0]}));

			// Username
			if (this_.show_user)
				row.append($('<td>', {
					html: x[2] === null ? 'System' : (x[3] == null ? x[2]
						: a_view_button('/u/' + x[2], x[3], undefined,
							view_user.bind(null, true, x[2])))
				}));

			// Submission time
			row.append($('<td>', {
				html: normalize_datetime(
					a_view_button('/s/' + x[0], x[12], undefined,
						view_submission.bind(null, true, x[0])).attr('datetime', x[12]),
					false)
			}));

			// Problem
			if (x[6] == null) // Not in the contest
				row.append($('<td>', {
					html: a_view_button('/p/' + x[4], x[5], undefined,
						view_problem.bind(null, true, x[4]))
				}));
			else
				row.append($('<td>', {
					html: [(this_.show_contest ? a_view_button('/c/c' + x[10], x[11], '', view_contest.bind(null, true, x[10])) : ''),
						(this_.show_contest ? ' / ' : ''),
						a_view_button('/c/r' + x[8], x[9], '',
							view_contest_round.bind(null, true, x[8])),
						' / ',
						a_view_button('/c/p' + x[6], x[7], '',
							view_contest_problem.bind(null, true, x[6]))
					]
				}));

			// Status
			row.append($('<td>', {
				class: 'status ' + x[13][0],
				text: (x[13][0].lastIndexOf('initial') === -1 ? ''
					: 'Initial: ') + x[13][1]
			}));

			// Score
			row.append($('<td>', {text: x[14]}));

			// Type
			row.append($('<td>', {text: x[1]}));

			// Actions
			row.append($('<td>', {
				html: ActionsToHTML.submission(x[0], x[15], x[1])
			}));

			this_.elem.children('tbody').append(row);
		}
	};

	this.fetch_more();
}
function tab_submissions_lister(parent_elem, query_suffix /*= ''*/, show_solutions_tab /* = false*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	parent_elem = $(parent_elem);
	function retab(tab_qsuff) {
		parent_elem.children('.submissions, .loader, .loader-info').remove();
		var table = $('<table class="submissions"></table>').appendTo(parent_elem);
		new SubmissionsLister(table, query_suffix + tab_qsuff).monitor_scroll();
	}

	var tabs = [
		'All', retab.bind(null, ''),
		'Final', retab.bind(null, '/tF'),
		'Ignored', retab.bind(null, '/tI')
	];
	if (show_solutions_tab)
		tabs.push('Solutions', retab.bind(null, '/tS'));

	tabmenu(function(elem) { elem.appendTo(parent_elem); }, tabs);
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
			)).add(Form.field_group('Memory limit [MB]', {
				type: 'text',
				name: 'mem_limit',
				size: 25,
				// maxlength: 'TODO...',
				placeholder: 'Take from Simfile',
			})).add(Form.field_group('Global time limit [s] (for each test)', {
				type: 'text',
				name: 'global_time_limit',
				size: 25,
				// maxlength: 'TODO...',
				placeholder: 'No global time limit',
			})).add(Form.field_group('Set time limits using model solution', {
				type: 'checkbox',
				name: 'relative_time_limits',
				checked: true
			})).add(Form.field_group('Ignore Simfile', {
				type: 'checkbox',
				name: 'ignore_simfile',
			})).add(Form.field_group('Seek for new tests', {
				type: 'checkbox',
				name: 'seek_for_new_tests',
				checked: true
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
					show_success_via_loader(this, 'Added');
					view_job(true, resp);
				} else {
					this.parent().remove();
					window.location.href = '/jobs/' + resp;
				}
			}, 'add-problem')
		);
	});
}
function reupload_problem(as_modal, problem_id) {
	view_ajax(as_modal, '/api/problems/=' + problem_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		data = data[0];

		var actions = data[7];
		if (actions.indexOf('R') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Reupload problem', '/api/problem/' + problem_id + '/reupload',
			Form.field_group("Problem's name", {
				type: 'text',
				name: 'name',
				value: data[3],
				size: 25,
				// maxlength: 'TODO...',
				placeholder: 'Take from Simfile',
			}).add(Form.field_group("Problem's label", {
				type: 'text',
				name: 'label',
				value: data[4],
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
						selected: ('Public' == data[2] ? true : undefined)
					}).add('<option>', {
						value: 'PRI',
						text: 'Private',
						selected: ('Private' == data[2] ? true : undefined)
					}).add('<option>', {
						value: 'CON',
						text: 'Contest only',
						selected: ('Contest only' == data[2] ? true : undefined)
					})
				})
			)).add(Form.field_group('Memory limit [MB]', {
				type: 'text',
				name: 'mem_limit',
				value: data[10],
				size: 25,
				// maxlength: 'TODO...',
				placeholder: 'Take from Simfile',
			})).add(Form.field_group('Global time limit [s] (for each test)', {
				type: 'text',
				name: 'global_time_limit',
				size: 25,
				// maxlength: 'TODO...',
				placeholder: 'No global time limit',
			})).add(Form.field_group('Set time limits using model solution', {
				type: 'checkbox',
				name: 'relative_time_limits',
				checked: true
			})).add(Form.field_group('Ignore Simfile', {
				type: 'checkbox',
				name: 'ignore_simfile',
			})).add(Form.field_group('Seek for new tests', {
				type: 'checkbox',
				name: 'seek_for_new_tests',
				checked: true
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
					show_success_via_loader(this, 'Reuploaded');
					view_job(true, resp);
				} else {
					this.parent().remove();
					window.location.href = '/jobs/' + resp;
				}
			}, 'add-problem')
		);
	}, '/p/' + problem_id + '/reupload');
}
function edit_problem(as_modal, problem_id) {
	// TODO
}
function delete_problem(as_modal, problem_id) {
	// TODO
}
function rejudge_problem_submissions(problem_id) {
	dialogue_modal_request("Rejudge all problem's submissions", $('<label>', {
			html: [
				'Are you sure to rejudge all submissions to the ',
				a_view_button('/p/' + problem_id, 'problem ' + problem_id, undefined,
					view_problem.bind(null, true, problem_id)),
				'?'
			]
		}), 'Rejudge all', 'btn-small blue',
		'/api/problem/' + problem_id + '/rejudge_all_submissions',
		'The rejudge jobs has been scheduled.', 'No, go back', true);
}
function view_problem(as_modal, problem_id, opt_hash /*= ''*/) {
	view_ajax(as_modal, '/api/problems/=' + problem_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		data = data[0];
		var actions = data[7];

		this.append($('<div>', {
			class: 'header',
			html: $('<h1>', {
					text: data[3]
				}).add('<div>', {
					html: ActionsToHTML.problem(data[0], actions, data[3], true)
				})
		})).append($('<center>', {
			class: 'always_in_view',
			html: $('<div>', {
				class: 'problem-info',
				html: $('<div>', {
					class: 'type',
					html: $('<label>', {text: 'Type'}).add('<span>', {
						text: String(data[2]).slice(0, 1).toLowerCase() +
							String(data[2]).slice(1)
					})
				})
				.add($('<div>', {
					class: 'name',
					html: $('<label>', {text: 'Name'})
				}).append(text_to_safe_html(data[3])))
				.add($('<div>', {
					class: 'label',
					html: $('<label>', {text: 'Label'})
				}).append(text_to_safe_html(data[4])))
				.add($('<div>', {
					class: 'tags',
					html: $('<label>', {text: 'Tags'})
				}).append(text_to_safe_html('')))
			})
		}));

		if (actions.indexOf('o') !== -1)
			$(this).find('.problem-info').append($('<div>', {
					class: 'owner',
					html: $('<label>', {text: 'Owner'}).add(
						a_view_button('/u/' + data[5], data[6], undefined,
							view_user.bind(null, true, data[5])))
				}));

		if (actions.indexOf('a') !== -1)
			$(this).find('.problem-info').append($('<div>', {
					class: 'added',
					html: $('<label>', {text: 'Added'})
				}).append(normalize_datetime($('<span>', {
					datetime: data[1],
					text: data[1]
				}), true)));

		var main = $(this);
		var tabs = [];
		if (actions.indexOf('s') !== -1)
			tabs.push('All submissions', function() {
					$(this).parent().next().remove();
					main.append($('<div>'));
					tab_submissions_lister(main.children().last(), '/p' + problem_id,
						true);
				});

		if (is_logged_in())
			tabs.push('My submissions', function() {
					$(this).parent().next().remove();
					main.append($('<div>'));
					tab_submissions_lister(main.children().last(), '/p' + problem_id + '/u' + logged_user_id());
				});

		if (actions.indexOf('f') !== -1)
			tabs.push('Simfile', function() {
				timed_hide_show(main.parents('.modal'));
				$(this).parent().next().remove();
				main.append($('<pre>', {
					class: 'simfile',
					style: 'text-align: initial',
					text: data[9]
				}));
			});

		if (actions.indexOf('j') !== -1)
			tabs.push('Related jobs', function() {
				$(this).parent().next().remove();
				var j_table = $('<table>', {
					class: 'jobs'
				});
				main.append($('<div>', {
					html: j_table
				}));
				new JobsLister(j_table, '/p' + problem_id).monitor_scroll();
			});

		tabmenu(function(x) { x.appendTo(main); }, tabs);

	}, '/p/' + problem_id + (opt_hash === undefined ? '' : opt_hash), undefined, false);
}
function ProblemsLister(elem, query_suffix /*= ''*/) {
	var this_ = this;
	if (query_suffix === undefined)
		query_suffix = '';

	this.show_owner = logged_user_is_tearcher_or_admin();
	this.show_added = logged_user_is_tearcher_or_admin() ||
		(query_suffix.indexOf('/u') !== -1);

	Lister.call(this, elem);
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
				remove_loader(this_.elem.parent());
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
			this_.query_suffix = '/<' + x[0];

			var row = $('<tr>');
			// Id
			row.append($('<td>', {text: x[0]}));
			// Type
			row.append($('<td>', {text: x[2]}));
			// Label
			row.append($('<td>', {text: x[4]}));
			// Name and tags
			row.append($('<td>', {text: x[3]}));

			// Owner
			if (this_.show_owner)
				row.append($('<td>', {
					html: x[5] === null ? '' :
						(a_view_button('/u/' + x[5], x[6], undefined,
							view_user.bind(null, true, x[5])))
				}));

			// Added
			if (this_.show_added)
				row.append(normalize_datetime($('<td>', {
					datetime: x[1],
					text: x[1]
				})));

			// Actions
			row.append($('<td>', {
				html: ActionsToHTML.problem(x[0], x[7], x[3])
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
		parent_elem.children('.problems, .loader, .loader-info').remove();
		var table = $('<table class="problems stripped"></table>').appendTo(parent_elem);
		new ProblemsLister(table, query_suffix + tab_qsuff).monitor_scroll();
	}

	var tabs = [
		'All', retab.bind(null, '')
	];

	if (is_logged_in())
		tabs.push('My', retab.bind(null, '/u' + logged_user_id()));

	tabmenu(function(elem) { elem.appendTo(parent_elem); }, tabs);
}
function problem_chooser(as_modal /*= true*/, opt_hash /*= ''*/) {
	view_base((as_modal === undefined ? true : as_modal),
		'/p' + (opt_hash === undefined ? '' : opt_hash), function() {
			timed_hide($(this).parent().parent().filter('.modal'));
			$(this).append($('<h1>', {text: 'Problems'}));
			if (logged_user_is_tearcher_or_admin())
				$(this).append(a_view_button('/p/add', 'Add problem', 'btn',
					add_problem.bind(null, true)));

			tab_problems_lister($(this));
		});
}

/* ================================ Contests ================================ */
function add_contest(as_modal) {
	view_base(as_modal, '/c/add', function() {
		this.append(ajax_form('Add contest', '/api/contest/add',
			Form.field_group("Contest's name", {
				type: 'text',
				name: 'name',
				size: 24,
				// maxlength: 'TODO...',
				required: true
			}).add(logged_user_is_admin() ? Form.field_group('Make public', {
				type: 'checkbox',
				name: 'public'
			}) : $()).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Add'
				})
			}), function(resp) {
				if (as_modal) {
					show_success_via_loader(this, 'Added');
					view_contest(true, resp);
				} else {
					this.parent().remove();
					window.location.href = '/c/c' + resp;
				}
			})
		);
	});
}
function add_contest_round(as_modal, contest_id) {
	view_base(as_modal, '/c/c' + contest_id + '/add_round', function() {
		this.append(ajax_form('Add round', '/api/contest/c' + contest_id + '/add_round',
			Form.field_group("Round's name", {
				type: 'text',
				name: 'name',
				size: 25,
				// maxlength: 'TODO...',
				required: true
			}).add(Form.field_group('Begin time', datetime_input('begins')
			)).add(Form.field_group('End time',
				datetime_input('ends', true, null, 'Never')
				.attr('placeholder', 'Never')
			)).add(Form.field_group('Full results time',
				datetime_input('full_results', true, null, 'Immediately')
				.attr('placeholder', 'Immediately')
			)).add(Form.field_group('Show ranking since',
				datetime_input('ranking_expo', true, null, "Don't show")
				.attr('placeholder', "Don't show")
			)).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Add'
				})
			}), function(resp) {
				if (as_modal) {
					show_success_via_loader(this, 'Added');
					view_contest_round(true, resp);
				} else {
					this.parent().remove();
					window.location.href = '/c/r' + resp;
				}
			})
		);
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
				placeholder: 'The same as in Problems',
			}).add(Form.field_group('Problem ID', {
				type: 'text',
				name: 'problem_id',
				size: 6,
				// maxlength: 'TODO...',
				required: true
			}).append(a_view_button('/p', 'Search problems', '', problem_chooser))
			).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Add'
				})
			}), function(resp) {
				if (as_modal) {
					show_success_via_loader(this, 'Added');
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
	view_ajax(as_modal, '/api/contests/=' + contest_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		data = data[0];

		var actions = data[4];
		if (actions.indexOf('A') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Edit contest', '/api/contest/c' + contest_id + '/edit',
			Form.field_group("Contest's name", {
				type: 'text',
				name: 'name',
				value: data[1],
				size: 24,
				// maxlength: 'TODO...',
				required: true
			}).add(Form.field_group('Public', {
				type: 'checkbox',
				name: 'public',
				checked: data[2],
				disabled: (data[2] || actions.indexOf('M') !== -1 ? undefined : true)
			})).add('<div>', {
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
	// TODO
}
function edit_contest_problem(as_modal, contest_problem_id) {
	// TODO
}
function rejudge_contest_problem_submissions(contest_problem_id) {
	// TODO
}
function delete_contest(as_modal, contest_id) {
	// TODO
}
function delete_contest_round(as_modal, contest_round_id) {
	// TODO
}
function delete_contest_problem(as_modal, contest_problem_id) {
	// TODO
}
function view_contest_impl(as_modal, id_for_api, opt_hash /*= ''*/) {
	view_ajax(as_modal, '/api/contest/' + id_for_api, function(data) {
		var contest = data[0];
		var rounds = data[1];
		var problems = data[2];
		var actions = contest[4];

		// Sort rounds by -items
		rounds.sort(function(a, b) { return b[2] - a[2]; });

		// Sort problems by (round_id, item)
		problems.sort(function(a, b) {
			return (a[1] == b[1] ? a[5] - b[5] : a[1] - b[1]);
		});


		// Header
		var header = $('<div>', {class: 'header', html:
			$('<h2>', {
				class: 'contest-path',
				html: function() {
					var res = [
						(id_for_api[0] === 'c' ? $('<span>', {text: contest[1]}) :
							$('<a>', {
								href: '/c/c' + contest[0],
								text: contest[1]
							}))
					];

					if (id_for_api[0] !== 'c')
						res.push(' / ', (id_for_api[0] === 'r' ?
							$('<span>', {text: rounds[0][1]}) :
							$('<a>', {
								href: '/c/r' + rounds[0][0],
								text: rounds[0][1]
							})));

					if (id_for_api[0] === 'p') {
						res.push(' / ', $('<span>', {text: problems[0][4]}));
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
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/c' + contest[0] + '/add_round', 'Add round', 'btn-small', add_contest_round.bind(null, true, contest[0]))),
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/c' + contest[0] + '/edit', 'Edit', 'btn-small blue', edit_contest.bind(null, true, contest[0]))),
				(actions.indexOf('D') === -1 ? '' : a_view_button('/c/c' + contest[0] + '/delete', 'Delete', 'btn-small red', delete_contest.bind(null, true, contest[0]))),
			]}));

		// Round buttons
		else if (id_for_api[0] === 'r')
			header.append($('<div>', {html: [
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/r' + rounds[0][0] + '/attach_problem', 'Attach problem', 'btn-small', add_contest_problem.bind(null, true, rounds[0][0]))),
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/r' + rounds[0][0] + '/edit', 'Edit', 'btn-small blue', edit_contest_round.bind(null, true, rounds[0][0]))),
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/r' + rounds[0][0] + '/delete', 'Delete', 'btn-small red', delete_contest_round.bind(null, true, rounds[0][0])))
			]}));

		// Problem buttons
		else if (id_for_api[0] === 'p')
			header.append($('<div>', {html: [
				a_view_button('/p/' + problems[0][2], 'View in Problems', 'btn-small green', view_problem.bind(null, true, problems[0][2])),
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/p' + problems[0][0] + '/edit', 'Edit', 'btn-small blue', edit_contest_problem.bind(null, true, problems[0][0]))),
				(actions.indexOf('A') === -1 ? '' :
					$('<a>', {
						class: 'btn-small blue',
						text: 'Rejudge submissions',
						click: rejudge_contest_problem_submissions.bind(null, problems[0][0])
					})),
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/p' + problems[0][0] + '/delete', 'Delete', 'btn-small red', delete_contest_problem.bind(null, true, problems[0][0])))
			]}));

		var contest_dashboard;
		var elem = $(this);
		var tabs = [
			'Dashboard', function() {
				timed_hide_show(elem.parents('.modal'));
				elem.children('.tabmenu').nextAll().remove();
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
					href: '/c/c' + contest[0],
					text: contest[1]
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
								href: '/c/r' + round[0],
								html: function() {
									var res = [$('<span>', {text: round[1]})];
									if (rounds.length !== 1)
										res.push($('<div>', {html: [
											$('<label>', {text: "B:"}),
											normalize_datetime($('<span>', {
												datetime: round[4],
												text: round[4]
											}), false, true)
										]}),
										$('<div>', {html: [
											$('<label>', {text: "E:"}),
											normalize_datetime($('<span>', (round[6] === null ?
												{text: 'never'}
												: {datetime: round[6], text: round[6]})
											), false, true)
										]}));
									else
										res.push($('<table>', {html: [
											$('<tr>', {html: [
												$('<td>', {text: 'Beginning'}),
												normalize_datetime($('<td>', {
													datetime: round[4],
													text: round[4]
												}), false, true)
											]}),
											$('<tr>', {html: [
												$('<td>', {text: 'End'}),
												normalize_datetime($('<td>', (round[6] === null ?
													{text: 'never'}
													: {datetime: round[6], text: round[6]})
												), false, true)
											]}),
											$('<tr>', {html: [
												$('<td>', {text: "Full results"}),
												normalize_datetime($('<td>', (round[5] === null ?
													{text: 'immediately'}
													: {datetime: round[5], text: round[5]})
												), false, true)
											]}),
											$('<tr>', {html: [
												$('<td>', {text: "Ranking since"}),
												normalize_datetime($('<td>', (round[3] === null ?
													{text: 'never'}
													: {datetime: round[3], text: round[3]})
												), false, true)
											]}),
										]}));

									return res;
								}()
							}))
						]
					}).appendTo(dashboard_rounds);
				}

				var problem2elem = new StaticMap();
				function append_problem(dashboard_round, problem, cannot_submit) {
					// Problem buttons
					if (id_for_api[0] === 'p')
						$('<center>', {html: [
							$('<a>', {
								class: 'btn-small',
								href: '/api/contest/p' + problem[0] + '/statement/' + encodeURIComponent(problem[4]),
								text: 'Statement'
							}),
							(cannot_submit ? '' : a_view_button('/c/p' + problem[0] + '/submit', 'Submit', 'btn-small blue', add_submission.bind(null, true, problem[2], problem[4], (actions.indexOf('A') !== -1), problem[0]))),
						]}).appendTo(dashboard.parent());

					var elem = $('<a>', {
						href: '/c/p' + problem[0],
						text: problem[4]
						// TODO: add info about score revealing when the problem is selected
					});

					problem2elem.add(problem[0], elem);
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
						if (p[1] < round[0])
							l = mid + 1;
						else
							r = mid;
					}
					// Check whether the round has ended
					var cannot_submit = (actions.indexOf('p') === -1);
					if (round[6] !== null && actions.indexOf('A') === -1) {
						var curr_time = date_to_datetime_str(new Date());
						var end_time = utcdt_or_tm_to_Date(round[6]);
						cannot_submit |= (end_time <= curr_time);
					}
					// Append problems
					while (l < problems.length && problems[l][1] == round[0])
						append_problem(dashboard_round, problems[l++], cannot_submit);
				}

				if (is_logged_in()) {
					problem2elem.prepare();
					var color_problems = function(query_suffix) {
						API_call('/api/submissions/' + id_for_api.toUpperCase() +
							'/u' + logged_user_id() + '/tF' + query_suffix, function(data)
						{
							if (data.length > 0) {
								for (var i in data) {
									(problem2elem.get(data[i][6]) || $()).addClass('status ' + data[i][13][0]);
								}
								color_problems('/<' + data[data.length - 1][0]);
							}

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
						}, $());
					};
					color_problems('');
				}
			}
		];

		if (actions.indexOf('A') !== -1)
			tabs.push('All submissions', function() {
				elem.children('.tabmenu').nextAll().remove();
				tab_submissions_lister($('<div>').appendTo(elem), '/' + id_for_api.toUpperCase());
			});

		if (actions.indexOf('p') !== -1)
			tabs.push('My submissions', function() {
				elem.children('.tabmenu').nextAll().remove();
				tab_submissions_lister($('<div>').appendTo(elem), '/' + id_for_api.toUpperCase() + '/u' + logged_user_id());
			});

		if (actions.indexOf('v') !== -1)
			tabs.push('Ranking', function() {
				elem.children('.tabmenu').nextAll().remove();
				contest_ranking($('<div>').appendTo(elem), id_for_api);
			});

		tabmenu(function(x) {
			x.on('tabmenuTabHasChanged', function(_, active_elem) {
				// Add / replace hashes in links in the contest-path
				elem.find('.contest-path').children('a:not(.btn-small)').each(function() {
					var xx = $(this);
					var href = xx.attr('href');
					var pos = href.indexOf('#');
					if (pos !== -1)
						href = href.substr(0, pos);
					xx.attr('href', href + '#' + tabname_to_hash($(active_elem).text()));
				});
			});
			x.appendTo(elem);
		}, tabs);

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
	API_call('/api/contest/' + id_for_api, function(cdata) {
		var contest = cdata[0];
		var rounds = cdata[1];
		var problems = cdata[2];
		var actions = contest[4];
		var is_admin = (actions.indexOf('A') !== -1);

		// Sort rounds by -items
		rounds.sort(function(a, b) { return b[2] - a[2]; });
		// Map rounds (by id) to their items
		var rid_to_item = new StaticMap();
		for (var i = 0; i < rounds.length; ++i) {
			if (!is_admin) {
				// Add only those rounds that have full results visible and the ranking_exposure point has passed
				var curr_time = date_to_datetime_str(new Date());
				var full_results = normalize_datetime($('<span>', {
					datetime: rounds[i][5]
				}), false).text();
				var ranking_exposure = normalize_datetime($('<span>', {
					datetime: rounds[i][3]
				}), false).text();

				if (curr_time < full_results || curr_time < ranking_exposure)
					continue; // Do not show the round
			}

			// Show the round
			rid_to_item.add(rounds[i][0], rounds[i][2]);
		}
		rid_to_item.prepare();

		// Add round item to the every problem (and remove invalid problems -
		// the ones which don't belong to the valid rounds)
		var tmp_problems = [];
		for (i = 0; i < problems.length; ++i) {
			var x = rid_to_item.get(problems[i][1]);
			if (x != null) {
				problems[i].push(x);
				tmp_problems.push(problems[i]);
			}
		}
		problems = tmp_problems;

		// Sort problems by (-round_item, item)
		problems.sort(function(a, b) {
			return (a[7] == b[7] ? a[5] - b[5] : b[7] - a[7]);
		});

		// Map problems (by id) to their indexes in the above array
		var problem_to_col_id = new StaticMap();
		for (i = 0; i < problems.length; ++i)
			problem_to_col_id.add(problems[i][0], i);
		problem_to_col_id.prepare();

		API_call('/api/contest/' + id_for_api + '/ranking', function(data_) {
			var modal = elem.parents('.modal');
			var data = data_;
			if (data.length == 0) {
				timed_hide_show(modal);
				var message_to_show = '<p>There is no one in the ranking yet...</p>';
				if (rounds.length === 1) {
					if (rounds[0][3] === null)
						message_to_show = '<p>The current round will not be shown in the ranking.</p>';
					else if (utcdt_or_tm_to_Date(rounds[0][3]) > new Date())
						message_to_show = $('<p>Ranking will be available since: </p>').append(normalize_datetime($('<span>', {datetime: rounds[0][3]})));
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
				while (rounds[j][0] != problem[1])
					++j; // Skip rounds (that have no problems attached)

				++colspan;
				// If current round's problems end
				if (i + 1 == problems.length || problems[i + 1][1] != problem[1]) {
					tr.append($('<th>', {
						colspan: colspan,
						html: $('<a>', {
							href: '/c/r' + rounds[j][0] + '#ranking',
							text: rounds[j][1]
						})
						// html: a_view_button('/c/r' + rounds[j][0] + '#ranking', rounds[j][1], '',
							// view_contest_round.bind(null, true, rounds[j][0], '#ranking'))
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
						href: '/c/p' + problems[i][0] + '#ranking',
						text: problems[i][3]
					})
					// html: a_view_button('/c/p' + problems[i][0] + '#ranking', problems[i][3], '',
						// view_contest_problem.bind(null, true, problems[i][0], '#ranking'))
				}));
			thead.append(tr);

			// Add score for each user add this to the user's info
			var submissions;
			for (i = 0; i < data.length; ++i) {
				submissions = data[i][2];
				var total_score = 0;
				// Count only valid problems (to fix potential discrepancies
				// between ranking submissions and the contest structure)
				for (j = 0; j < submissions.length; ++j)
					if (problem_to_col_id.get(submissions[j][2]) != null)
						total_score += submissions[j][4];

				data[i].push(total_score);
			}

			// Sort users (and their submissions) by their score
			data.sort(function(a, b) { return b[3] - a[3]; });

			// Add rows
			var tbody = $('<tbody>');
			var prev_score = data[0][3] + 1;
			var place;
			for (i = 0; i < data.length; ++i) {
				var user_row = data[i];
				tr = $('<tr>');
				// Place
				if (prev_score != user_row[3]) {
					place = i + 1;
					prev_score = user_row[3];
				}
				tr.append($('<td>', {text: place}));
				// User
				if (user_row[0] === null)
					tr.append($('<td>', {text: user_row[1]}));
				else {
					tr.append($('<td>', {
						html: a_view_button('/u/' + user_row[0], user_row[1], '',
							view_user.bind(null, true, user_row[0]))
					}));
				}
				// Score
				tr.append($('<td>', {text: user_row[3]}));
				// Submissions
				var row = new Array(problems.length);
				submissions = data[i][2];
				for (j = 0; j < submissions.length; ++j) {
					var x = problem_to_col_id.get(submissions[j][2]);
					if (x != null) {
						if (submissions[j][0] === null)
							row[x] = $('<td>', {
								class: 'status ' + submissions[j][3][0],
								text: submissions[j][4]
							});
						else {
							row[x] = $('<td>', {
								class: 'status ' + submissions[j][3][0],
								html: a_view_button('/s/' + submissions[j][0],
									submissions[j][4], '',
									view_submission.bind(null, true, submissions[j][0]))
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

	Lister.call(this, elem);
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
				remove_loader(this_.elem.parent());
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
			this_.query_suffix = '/<' + x[0];

			var row = $('<tr>',	{
				class: (x[2] ? undefined : 'grayed')
			});

			// Id
			if (logged_user_is_admin())
				row.append($('<td>', {text: x[0]}));
			// Name
			row.append($('<td>', {
				html: $('<a>', {
					href: '/c/c' + x[0],
					text: x[1]
				})
			}));

			// Actions
			row.append($('<td>', {
				html: ActionsToHTML.contest(x[0], x[4])
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
		parent_elem.children('.contests, .loader, .loader-info').remove();
		var table = $('<table class="contests"></table>').appendTo(parent_elem);
		new ContestsLister(table, query_suffix + tab_qsuff).monitor_scroll();
	}

	var tabs = [
		'All', retab.bind(null, '')
	];

	// TODO: implement it
	// if (is_logged_in())
		// tabs.push('My', retab.bind(null, '/u' + logged_user_id()));

	tabmenu(function(elem) { elem.appendTo(parent_elem); }, tabs);
}
function contest_chooser(as_modal /*= true*/, opt_hash /*= ''*/) {
	view_base((as_modal === undefined ? true : as_modal),
		'/c' + (opt_hash === undefined ? '' : opt_hash), function() {
			timed_hide($(this).parent().parent().filter('.modal'));
			$(this).append($('<h1>', {text: 'Contests'}));
			if (logged_user_is_tearcher_or_admin())
				$(this).append(a_view_button('/c/add', 'Add contest', 'btn',
					add_contest.bind(null, true)));

			tab_contests_lister($(this));
		});
}
/* ============================ Contest's users ============================ */
/*function addContestUser(contest_id) {
	modalForm('Add user to the contest',
		$('<div>', {
			class: 'field-group',
			style: 'margin-top: 5px',
			html: $('<div>', {
				html: $('<select>', {
				name: 'user_value_type',
					html: $('<option>', {
						value: 'username',
						text: 'Username',
						selected: true
					}).add('<option>', {
						value: 'uid',
						text: 'Uid'
					})
				})
			}).add('<input>', {
				type: 'text',
				name: 'user'
			})
		}).add('<div>', {
			class: 'field-group',
			html: $('<label>', {
				text: 'User mode'
			}).add('<select>', {
				name: 'mode',
				html: $('<option>', {
					value: 'c',
					text: 'Contestant',
					selected: true
				}).add('<option>', {
					value: 'm',
					text: 'Moderator'
				})
			})
		}).add(modalFormSubmitButton('Add user',
			'/c/' + contest_id + '/users/add',
			'User has been added.'))
	);
}
function changeContestUserMode(contest_id, user_id, mode) {
	modalForm('Change user mode',
		$('<div>', {
			html: $('<label>', {
				text: "New user's mode: "
			}).add('<select>', {
				name: 'mode',
				html: $('<option>', {
					value: 'c',
					text: 'Contestant',
					selected: (mode === 'c')
				}).add('<option>', {
					value: 'm',
					text: 'Moderator',
					selected: (mode === 'm')
				})
			}).add('<input>', {
				type: 'hidden',
				name: 'uid',
				value: user_id
			})
		}).add(modalFormSubmitButton('Change mode',
			'/c/' + contest_id + '/users/change-mode',
			'User mode has been changed.'))
	);
}
function expelContestUser(contest_id, user_id, username) {
	modalForm('Expel user from the contest',
		$('<div>', {
			html: $('<label>', {
				html: 'Are you sure to expel the user <a href="/u/' + user_id +
					'">' + username + '</a> with the id = ' + user_id + '?',
			}).add('<input>', {
				type: 'hidden',
				name: 'uid',
				value: user_id
			})
		}).add(modalFormSubmitButton('Of course!',
			'/c/' + contest_id + '/users/expel',
			'User has been expelled.', 'red',
			'No, the user may stay'))
	);
}*/

function open_calendar_on(time, text_input, hidden_input) {
	var months = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
	// var time = new Date();
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
						return $('<td>', {html: $('<input>', {
								type: 'text',
								maxlength: 2,
								click: function() { this.select(); },
							}).on('focusout', function() {
								var val = parseInt($(this).val());
								if (isNaN(val) || val < 0)
									val = '00';
								else if (val > max_val)
									val = max_val;
								else if (val < 10)
									val = '0' + val;

								$(this).val(val);
								if (idx === 0)
									time.setHours(val);
								else if (idx === 1)
									time.setMinutes(val);
								else if (idx === 2)
									time.setSeconds(val);
							})
						});
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
	// Wheel handlers on time_chooser
	time_chooser.find('td:first-child').on('wheel', function(e) {
		e.preventDefault();
		var coeff = $(this).parent().is(':first-child, :last-child') ? 6 : 1;
		if (e.originalEvent.deltaY > 0)
			change_second(-3600 * coeff);
		else if (e.originalEvent.deltaY < 0)
			change_second(+3600 * coeff);
		update_calendar();
	});
	time_chooser.find('td:nth-child(2)').on('wheel', function(e) {
		e.preventDefault();
		var coeff = $(this).parent().is(':first-child, :last-child') ? 10 : 1;
		if (e.originalEvent.deltaY > 0)
			change_second(-60 * coeff);
		else if (e.originalEvent.deltaY < 0)
			change_second(+60 * coeff);
		update_calendar();
	});
	time_chooser.find('td:last-child').on('wheel', function(e) {
		e.preventDefault();
		var coeff = $(this).parent().is(':first-child, :last-child') ? 10 : 1;
		if (e.originalEvent.deltaY > 0)
			change_second(-1 * coeff);
		else if (e.originalEvent.deltaY < 0)
			change_second(+1 * coeff);
		update_calendar();
	});

	// Days table
	var tbody, tbody_date = new Date(time);
	tbody_date.setDate(0); // Change month; this variable is used to skip
	                       // regenerating the whole table when it is unnecessary
	function update_calendar() {
		// Time chooser
		var x = time_chooser.find('input');
		var foo = function(x) { return x < 10 ? '0' + x : x; };
		x.eq(0).val(foo(time.getHours()));
		x.eq(1).val(foo(time.getMinutes()));
		x.eq(2).val(foo(time.getSeconds()));
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

	var round_to_5_minutes = function() {
		var k = time.getMinutes() + (time.getSeconds() !== 0) + 4;
		time.setMinutes(k - k % 5);
		time.setSeconds(0);
		update_calendar();
	};

	modal($('<div>', {
		html: [
			calendar,
			time_chooser,
			$('<center>', {html:
				$('<a>', {
					class: 'btn-small',
					text: 'Round to 5 minutes',
					click: round_to_5_minutes
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
			})
		]
	}), function(modal) {
		var arrow_update = function(e) {
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

		$(document).on('keydown', arrow_update);
		modal[0].onmodalclose = function() {
			$(document).off('keydown', arrow_update);
			$(text_input).val(date_to_datetime_str(time));
			$(hidden_input).val(time.getTime() / 1000 | 0);
		};
	});
}
function datetime_input(name, allow_null /* = false */, initial_time /* = undefined <=> use current time, null <=> no value */, button_text /* = 'Set to null' */) {
	var dt;
	if (initial_time === undefined || initial_time === null) {
		dt = new Date();
		// Round to 5 minutes
		var k = dt.getMinutes() + (dt.getSeconds() !== 0) + 4;
		dt.setMinutes(k - k % 5);
		dt.setSeconds(0);
	} else
		dt = utcdt_or_tm_to_Date(initial_time);

	var elems = $('<input>', {
		type: 'text',
		class: 'calendar-input',
		tm: (initial_time === null ? undefined : dt.getTime() / 1000 | 0),
		value: (initial_time === null ? '' : date_to_datetime_str(dt)),
		readonly: true,
		click: function() {
			open_calendar_on(dt, elems.eq(0), elems.eq(1));
		}
	}).add('<input>', {
		type: 'hidden',
		name: name,
		value: (initial_time === null ? undefined : dt.getTime() / 1000 | 0),
	});

	if (allow_null)
		return elems.add('<a>', {
			class: 'btn-small',
			text: (button_text === undefined ? 'Set to null' : button_text),
			click: function () {
				elems.eq(0).val('');
				elems.eq(1).val(null);
			}
		});

	return elems;
}
